
/* Grammar subroutines needed by parser */

#include "Python.h"
#include "pgenheaders.h"
#include "grammar.h"
#include "token.h"

/* Return the DFA for the given type */

dfa *
PyGrammar_FindDFA(grammar *g, register int type)
{
    register dfa *d;
    /* Massive speed-up */
    d = &g->g_dfa[type - NT_OFFSET];
    assert(d->d_type == type);
    return d;
}
