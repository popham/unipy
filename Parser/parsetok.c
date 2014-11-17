
/* Parser-tokenizer link implementation */

#include "pgenheaders.h"
#include "tokenizer.h"
#include "node.h"
#include "grammar.h"
#include "parser.h"
#include "parsetok.h"
#include "errcode.h"

int Py_TabcheckFlag;

/* Forward */
static node *parsetok(struct tok_state *, grammar *, int, perrdetail *, unsigned long *);
static void initerr(perrdetail *err_ret);

/* Parse input coming from a string.  Return error code, print some errors. */
node *
PyParser_ParseString(const unsigned char *s, grammar *g, int start, perrdetail *err_ret)
{
    return PyParser_ParseStringFlags(s, g, start, err_ret, 0);
}

node *
PyParser_ParseStringFlags(const unsigned char *s, grammar *g, int start, perrdetail *err_ret, unsigned long flags)
{
    struct tok_state *tok;

    initerr(err_ret);

    if ((tok = PyTokenizer_FromString(s)) == NULL) {
        err_ret->error = E_NOMEM;
        return NULL;
    }

    return parsetok(tok, g, start, err_ret, &flags);
}

/* Parse input coming from the given tokenizer structure.
   Return error code. */

static node *
parsetok(struct tok_state *tok, grammar *g, int start, perrdetail *err_ret, unsigned long *flags)
{
    parser_state *ps;
    node *n;

    if ((ps = PyParser_New(g, start)) == NULL) {
        fprintf(stderr, "no mem for new parser\n");
        err_ret->error = E_NOMEM;
        PyTokenizer_Free(tok);
        return NULL;
    }

#ifdef PY_PARSER_REQUIRES_FUTURE_KEYWORD
    if (*flags & PyPARSE_PRINT_IS_FUNCTION) {
        ps->p_flags |= CO_FUTURE_PRINT_FUNCTION;
    }
    if (*flags & PyPARSE_UNICODE_LITERALS) {
        ps->p_flags |= CO_FUTURE_UNICODE_LITERALS;
    }
#endif

    for (;;) {
        const unsigned char *a, *b;
        int type;
        int col_offset;

        type = PyTokenizer_Get(tok, &a, &b);
        if (type == ERRORTOKEN) {
            err_ret->error = tok->done;
            break;
        }
        if (type == ENDMARKER && tok->indent != 0) {
            type = NEWLINE;
            tok->pendin = -tok->indent;
            tok->indent = 0;
        }

        if (a >= tok->line_start)
            col_offset = a - tok->line_start;
        else
            col_offset = -1;

        if ((err_ret->error = PyParser_AddToken(ps,
                                                (int)type,
                                                a, b-a,
                                                tok->lineno, col_offset,
                                                &(err_ret->expected))
             ) != E_OK) {

            if (err_ret->error != E_DONE) err_ret->token = type;
            break;
        }
    }

    if (err_ret->error == E_DONE) {
        n = ps->p_tree;
        ps->p_tree = NULL;
    }
    else
        n = NULL;

#ifdef PY_PARSER_REQUIRES_FUTURE_KEYWORD
    *flags = ps->p_flags;
#endif
    PyParser_Delete(ps);

    if (n == NULL) {
        if (tok->lineno <= 1 && tok->done == E_EOF)
            err_ret->error = E_EOF;
        err_ret->lineno = tok->lineno;
        assert(tok->cur - tok->buf < INT_MAX);
        err_ret->offset = (int)(tok->cur - tok->buf);
    }

    PyTokenizer_Free(tok);

    return n;
}

static void
initerr(perrdetail *err_ret)
{
    err_ret->error = E_OK;
    err_ret->lineno = 0;
    err_ret->offset = 0;
    err_ret->text = NULL;
    err_ret->token = -1;
    err_ret->expected = -1;
}
