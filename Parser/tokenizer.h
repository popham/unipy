#ifndef Py_TOKENIZER_H
#define Py_TOKENIZER_H
#ifdef __cplusplus
extern "C" {
#endif

/* Tokenizer interface */

#include "token.h"      /* For token types */

#define MAXINDENT 100   /* Max indentation level */

/* Tokenizer state */
struct tok_state {
    /* Input state; buf <= cur <= inp <= end */
    /* NB an entire line is held in the buffer */
    const unsigned char *buf;    /* Start of the current line */
    const unsigned char *cur;    /* Next character in buffer */
    const unsigned char *inp;    /* End of current line */
    const unsigned char *end;    /* End of input buffer */
    const unsigned char *start;  /* Start of current token if not NULL */
    int done;           /* E_OK normally, E_EOF at EOF, otherwise error code */
    /* NB If done != E_OK, cur must be == inp!!! */
    int indent;         /* Current indentation index */
    int indstack[MAXINDENT];            /* Stack of indents */
    int atbol;          /* Nonzero if at begin of new line */
    int pendin;         /* Pending indents (if > 0) or dedents (if < 0) */
    int lineno;         /* Current line number */
    int level;          /* () [] {} Parentheses nesting level */

    /* Used to allow free continuations inside them */
    int altwarning;     /* Issue warning if alternate tabs don't match */
    int alterror;       /* Issue error if alternate tabs don't match */
    int altindstack[MAXINDENT];         /* Stack of alternate indents */

    int decoding_erred;         /* whether erred in decoding  */
    int cont_line;              /* whether we are in a continuation line */
    const unsigned char* line_start;     /* pointer to start of current line */
};

extern struct tok_state *PyTokenizer_FromString(const unsigned char *);
extern void PyTokenizer_Free(struct tok_state *);
extern unsigned int PyTokenizer_Get(struct tok_state *, const unsigned char **, const unsigned char **);

#ifdef __cplusplus
}
#endif
#endif /* !Py_TOKENIZER_H */
