#include "magicate.h"

#include "node.h"
#include "grammar.h"
#include "graminit.h"
#include "token.h"
#include "parsetok.h"

extern grammar _PyParser_Grammar;

const unsigned char *_Magicate_Magic[] = {
    CUC(").___oplus___("),
    CUC(").___otimes___("),
    CUC(").___ioplus___("),
    CUC(").___iotimes___(")
};

const unsigned char *advance(unsigned char **target, const unsigned char *source, size_t length)
{
#ifndef NDEBUG
    printf("printing '%.*s' to target\n", (int)length, source);
#endif

    memcpy(*target, source, length);
    *target += length;

    return source + length;
}

// TODO: Validate AugAssign LHS, currently this produces code that runs under
// Python contrary to proper AugAssign operators.
const unsigned char *branch(const node *n, const unsigned char *source, unsigned char **target)
{
    unsigned int i;
    size_t length;
    const unsigned char *method;
    const node *child;

    if (NCH(n) == 0 && STRL(n) > 0) {
        length = STR(n) + STRL(n) - source;
        return advance(target, source, length);
    }

    switch (TYPE(n)) {
    case atom:
        for (i=0; i<NCH(n); ++i) {
            child = CHILD(n, i);
            switch (TYPE(child)) {
            case LPAR:
            case RPAR:
                assert(NCH(n) == 3);
                break;
            default:
                source = branch(child, source, target);
                continue;
            }

            length = STR(child) + STRL(child) - source;
            source = advance(target, source, length);
        }
        break;
    case arith_expr:
    case term:
        /*
         * For each extra op, insert a leading paren.  Each of these gets
         * closed by a `).___some_op___(`.  Left to right associativity.
         */
        for (i=(NCH(n) - 1)/2; i>0; --i) {
            if (ISEXTRAOP(TYPE(CHILD(n, i*2 - 1)))) {
#ifndef NDEBUG
    printf("printing '(' to target\n");
#endif
                **target = '(';
                *target += 1;
            }
        }

        for (i=0; i<NCH(n); ++i) {
            child = CHILD(n, i);

            if (!ISEXTRAOP(TYPE(child))) {
                source = branch(child, source, target);
                continue;
            }

            /*
             * All of my extraop types are binary, so there is always a trailing
             * operand.
             */
            assert(i != NCH(n)-1);

            // Print up to the old operator-token.
#ifndef NDEBUG
    printf("printing up to old token '%.*s' to target\n", (int)(STR(child)-source), source);
#endif
            source = advance(target, source, STR(child)-source);

            // Print the operator-token replacement.
            method = _Magicate_Magic[TYPE(child) - EXTRA_OP_OFFSET];
#ifndef NDEBUG
    printf("printing '%s' to target\n", method);
#endif
            length = strlen((const char *)method);
            memcpy(*target, method, length);
            *target += length;
            source += STRL(child); // Move just beyond the old operator-token.

            i += 1;
            source = branch(CHILD(n, i), source, target);

            // Close `).___some_op___(`.
#ifndef NDEBUG
    printf("printing ')' to target\n");
#endif
            **target = ')';
            *target += 1;
        }
        break;
    default:
        for (i=0; i<NCH(n); ++i) {
            source = branch(CHILD(n, i), source, target);
        }
    }

    return source;
}

unsigned int compute_delta(const node *n)
{
    int i;
    unsigned int delta;

    if (ISEXTRAOP(TYPE(n))) {
        // Close the prior and following groups => extra two parens.
      delta = 2 + strlen((const char *)_Magicate_Magic[TYPE(n) - EXTRA_OP_OFFSET]) - STRL(n);
    }
    else {
        delta = 0;
    }

    for (i=0; i<NCH(n); ++i) {
        delta += compute_delta(CHILD(n, i));
    }

    return delta;
}

// JS allocation no good.  Inaccessible client side.
unsigned char *magicate(const unsigned char *source)
{
    unsigned char **accumulator;
    unsigned char *result, *t;
    size_t length;
    grammar *g = &_PyParser_Grammar;
    perrdetail err;
    node *n = PyParser_ParseString(source, g, g->g_start, &err);

    length = strlen((const char *)source) + compute_delta(n);
    t = result = malloc(length+1);
    accumulator = &t;
    result[length] = '\0';

    source = branch(n, source, accumulator);

    // Write from the final position to '\0'
    length = strlen((const char *)source);
    memcpy(t, source, length);

    return result;
}
