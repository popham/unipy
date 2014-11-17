
/* Grammar implementation */

#include "Python.h"
#include "pgenheaders.h"

#include <ctype.h>

#include "token.h"
#include "grammar.h"
#include "decode.h"

#ifdef RISCOS
#include <unixlib.h>
#endif

static const size_t maximum_string_length=20;

grammar *
newgrammar(int start)
{
    grammar *g;

    g = (grammar *) PyMem_MALLOC(sizeof(grammar));
    if (g == NULL)
        Py_FatalError("no mem for new grammar");
    g->g_ndfas = 0;
    g->g_dfa = NULL;
    g->g_start = start;
    g->g_ll.ll_nlabels = 0;
    g->g_ll.ll_label = NULL;
    g->g_accel = 0;
    return g;
}

dfa *
adddfa(grammar *g, int type, const unsigned char *name, size_t name_length)
{
    dfa *d;

    g->g_dfa = (dfa *) PyMem_REALLOC(
        g->g_dfa,
        sizeof(dfa) * (g->g_ndfas + 1)
    );
    if (g->g_dfa == NULL)
        Py_FatalError("no mem to resize dfa in adddfa");
    d = &g->g_dfa[g->g_ndfas++];
    d->d_type = type;
    d->d_name = name;
    d->d_name_length = name_length;
    d->d_nstates = 0;
    d->d_state = NULL;
    d->d_initial = -1;
    d->d_first = NULL;
    return d; /* Only use while fresh! */
}

int
addstate(dfa *d)
{
    state *s;

    d->d_state = (state *) PyMem_REALLOC(d->d_state,
                                         sizeof(state) * (d->d_nstates + 1));
    if (d->d_state == NULL)
        Py_FatalError("no mem to resize state in addstate");
    s = &d->d_state[d->d_nstates++];
    s->s_narcs = 0;
    s->s_arc = NULL;
    s->s_lower = 0;
    s->s_upper = 0;
    s->s_accel = NULL;
    s->s_accept = 0;
    return s - d->d_state;
}

void
addarc(dfa *d, int from, int to, int lbl)
{
    state *s;
    arc *a;

    assert(0 <= from && from < d->d_nstates);
    assert(0 <= to && to < d->d_nstates);

    s = &d->d_state[from];
    s->s_arc = (arc *)PyMem_REALLOC(s->s_arc, sizeof(arc) * (s->s_narcs + 1));
    if (s->s_arc == NULL)
        Py_FatalError("no mem to resize arc list in addarc");
    a = &s->s_arc[s->s_narcs++];
    a->a_lbl = lbl;
    a->a_arrow = to;
}

int
addlabel(labellist *ll, int type, const unsigned char *str, size_t str_length)
{
    int i;
    label *lb;

    for (i = 0; i < ll->ll_nlabels; i++) {
        if (ll->ll_label[i].lb_type == type &&
            ll->ll_label[i].lb_str_length == str_length &&
            memcmp(ll->ll_label[i].lb_str, str, str_length) == 0)

            return i;
    }
    ll->ll_label = (label *) PyMem_REALLOC(
        ll->ll_label,
        sizeof(label) * (ll->ll_nlabels + 1)
    );
    if (ll->ll_label == NULL)
        Py_FatalError("no mem to resize labellist in addlabel");
    lb = &ll->ll_label[ll->ll_nlabels++];
    lb->lb_type = type;
    lb->lb_str = str;
    lb->lb_str_length = str_length;

    return lb - ll->ll_label;
}

/* Same, but rather dies than adds */

int
findlabel(labellist *ll, int type, const unsigned char *str, size_t str_length)
{
    int i;

    for (i = 0; i < ll->ll_nlabels; i++) {
        if (ll->ll_label[i].lb_type == type)
            return i;
    }
    fprintf(stderr, "Label %d/'%.*s' not found\n", type, (int)str_length, str);
    Py_FatalError("grammar.c:findlabel()");
    return 0; /* Make gcc -Wall happy */
}

/* Forward */
static void translabel(grammar *, label *);

void
translatelabels(grammar *g)
{
    int i;

#ifndef NDEBUG
    printf("Translating labels ...\n");
#endif
    /* Don't translate EMPTY */
    for (i = EMPTY+1; i < g->g_ll.ll_nlabels; i++)
        translabel(g, &g->g_ll.ll_label[i]);
}

static void
translabel(grammar *g, label *lb)
{
    int i;

    if (lb->lb_type == NAME) {
        for (i = 0; i < g->g_ndfas; i++) {
            if (lb->lb_str_length == g->g_dfa[i].d_name_length &&
                memcmp(lb->lb_str, g->g_dfa[i].d_name, lb->lb_str_length) == 0) {
#ifndef NDEBUG
                printf("Label %.*s is non-terminal %d.\n",
                       (int)lb->lb_str_length, lb->lb_str,
                       g->g_dfa[i].d_type);
#endif
                lb->lb_type = g->g_dfa[i].d_type;
                lb->lb_str = NULL;
                lb->lb_str_length = 0;
                return;
            }
        }
        for (i = 0; i < (int)N_TOKENS; i++) {
            if (lb->lb_str_length == strlen((const char*)_PyParser_TokenNames[i]) &&
                memcmp(lb->lb_str, _PyParser_TokenNames[i], lb->lb_str_length) == 0) {
#ifndef NDEBUG
                printf("Label %.*s is terminal %d.\n",
                       (int)lb->lb_str_length, lb->lb_str,
                       i);
#endif
                lb->lb_type = i;
                lb->lb_str = NULL;
                lb->lb_str_length = 0;
                return;
            }
        }
        printf("Can't translate NAME label '%.*s'\n",
               (int)lb->lb_str_length, lb->lb_str);
        return;
    }

    if (lb->lb_type == STRING) {
        if (isalpha(Py_CHARMASK(lb->lb_str[1])) || lb->lb_str[1] == '_') {
            // Maintain the constraint of keywords to the ASCII character set.

          const unsigned char *p;
#ifndef NDEBUG
          printf("Label %.*s is a keyword\n",
                 (int)lb->lb_str_length, lb->lb_str);
#endif
            lb->lb_type = NAME;
            lb->lb_str += 1;
            p = CUC(strchr((const char *)lb->lb_str, '\''));
            if (p != NULL)
                lb->lb_str_length = p - lb->lb_str;
        }
        else {
            int type;
            unsigned int *utf;

            // Limit strings to `maximum_string_length` (+2 quotes).
            unsigned int utf_data[maximum_string_length+2];

            // Decode the label and bracket it [`utf`, `utf_end`).
            unsigned int *utf_end = &utf_data[0];
            const unsigned char *i=lb->lb_str;
            while (i - lb->lb_str < lb->lb_str_length) {
                i = decode(i, utf_end++);

                assert(utf_end - utf_data < maximum_string_length);
            }

            assert(utf_data[0] == (unsigned int)(unsigned char)'\'');

            utf_end -= 1;
            assert(*utf_end == (unsigned int)(unsigned char)'\'');

            utf = &utf_data[1];
            switch (utf_end - utf) {
              /*
               * Operators from the grammar are wrapped by single quotes, so I'm
               * looking for the operator's length plus two.
               */
            case 1:
                type = (int) PyToken_OneChar(utf[0]);
                if (type != OP) {
                    lb->lb_type = type;
                    lb->lb_str = NULL;
                    lb->lb_str_length = 0;
                }
                else printf("Unknown OP label %.*s\n",
                            (int)lb->lb_str_length, (const char *)lb->lb_str);
                break;
            case 2:
                type = (int) PyToken_TwoChars(utf[0], utf[1]);
                if (type != OP) {
                    lb->lb_type = type;
                    lb->lb_str = NULL;
                    lb->lb_str_length = 0;
                }
                else printf("Unknown OP label %.*s\n",
                            (int)lb->lb_str_length, (const char *)lb->lb_str);
                break;
            case 3:
                type = (int) PyToken_ThreeChars(utf[0], utf[1], utf[2]);
                if (type != OP) {
                    lb->lb_type = type;
                    lb->lb_str = NULL;
                    lb->lb_str_length = 0;
                }
                else printf("Unknown OP label %.*s\n",
                            (int)lb->lb_str_length, (const char *)lb->lb_str);
                break;
            default: printf("Can't translate STRING label %.*s\n",
                            (int)lb->lb_str_length, (const char*)lb->lb_str);
            }
        }
    }
    else
        printf("Can't translate label of type '%i'\n", lb->lb_type);
}
