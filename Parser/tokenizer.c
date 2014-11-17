
/* Tokenizer implementation */

#include "Python.h"
#include "pgenheaders.h"

#include <ctype.h>
#include <assert.h>

#include "tokenizer.h"
#include "errcode.h"
#include "decode.h"

/* Don't ever change this -- it would break the portability of Python code */
#define TABSIZE 8

/* Forward */
static struct tok_state *tok_new(void);
static unsigned int tok_nextc(struct tok_state *tok);
static void tok_backup(struct tok_state *tok, unsigned int c);

/* Token names */
const unsigned char *_PyParser_TokenNames[] = {
    CUC("ENDMARKER"),
    CUC("NAME"),
    CUC("NUMBER"),
    CUC("STRING"),
    CUC("NEWLINE"),
    CUC("INDENT"),
    CUC("DEDENT"),
    CUC("LPAR"),
    CUC("RPAR"),
    CUC("LSQB"),
    CUC("RSQB"),
    CUC("COLON"),
    CUC("COMMA"),
    CUC("SEMI"),
    CUC("PLUS"),
    CUC("MINUS"),
    CUC("STAR"),
    CUC("SLASH"),
    CUC("VBAR"),
    CUC("AMPER"),
    CUC("LESS"),
    CUC("GREATER"),
    CUC("EQUAL"),
    CUC("DOT"),
    CUC("PERCENT"),
    CUC("BACKQUOTE"),
    CUC("LBRACE"),
    CUC("RBRACE"),
    CUC("EQEQUAL"),
    CUC("NOTEQUAL"),
    CUC("LESSEQUAL"),
    CUC("GREATEREQUAL"),
    CUC("TILDE"),
    CUC("CIRCUMFLEX"),
    CUC("LEFTSHIFT"),
    CUC("RIGHTSHIFT"),
    CUC("DOUBLESTAR"),
    CUC("PLUSEQUAL"),
    CUC("MINEQUAL"),
    CUC("STAREQUAL"),
    CUC("SLASHEQUAL"),
    CUC("PERCENTEQUAL"),
    CUC("AMPEREQUAL"),
    CUC("VBAREQUAL"),
    CUC("CIRCUMFLEXEQUAL"),
    CUC("LEFTSHIFTEQUAL"),
    CUC("RIGHTSHIFTEQUAL"),
    CUC("DOUBLESTAREQUAL"),
    CUC("DOUBLESLASH"),
    CUC("DOUBLESLASHEQUAL"),
    CUC("AT"),
    CUC("CIRCLEDPLUS"),
    CUC("CIRCLEDTIMES"),
    CUC("CIRCLEDPLUSEQUAL"),
    CUC("CIRCLEDTIMESEQUAL"),
    /* This table must match the #defines in token.h! */
    CUC("OP"),
    CUC("<ERRORTOKEN>"),
    CUC("<N_TOKENS>")
};

/* Create and initialize a new tok_state structure */

static struct tok_state *
tok_new(void)
{
    struct tok_state *tok =
        (struct tok_state *)PyMem_MALLOC(sizeof(struct tok_state));
    if (tok == NULL) return NULL;
    tok->buf = tok->cur = tok->end = tok->inp = tok->start = NULL;
    tok->done = E_OK;
    tok->indent = 0;
    tok->indstack[0] = 0;
    tok->atbol = 1;
    tok->pendin = 0;
    tok->lineno = 0;
    tok->level = 0;
    tok->altwarning = 0;
    tok->alterror = 0;
    tok->altindstack[0] = 0;
    tok->decoding_erred = 0;
    tok->cont_line = 0;

    return tok;
}

/* Set up tokenizer for string */

struct tok_state *
PyTokenizer_FromString(const unsigned char *str)
{
    if (str == NULL) return NULL;
    struct tok_state *tok = tok_new();
    if (tok == NULL) return NULL;

    tok->buf = tok->cur = tok->inp = tok->end = str;
    return tok;
}

/* Free a tok_state structure */

void
PyTokenizer_Free(struct tok_state *tok)
{
    PyMem_FREE(tok);
}

static unsigned int
tok_nextc(register struct tok_state *tok)
{
    unsigned int c;
    const unsigned char *start;

    for (;;) {
        if (tok->cur != tok->inp) {
            /* Fast path */
        unicodify:
            start = tok->cur;
            tok->cur = decode(tok->cur, &c);
            if (tok->cur - start == 0) tok->decoding_erred = 1;

#ifndef NDEBUG
            if (c & 0xffffff00) printf("More than one code point: 0x%08x\n", c);
#endif

            return c;
        }

        if (tok->done != E_OK)
            return EOF;

        const unsigned char *end = CUC(strchr((const char *)tok->inp, '\n'));
        if (end != NULL)
            end++;
        else {
            end = CUC(strchr((const char *)tok->inp, '\0'));
            if (end == tok->inp) {
                tok->done = E_EOF;
                return EOF;
            }
        }
        if (tok->start == NULL) {
            /*
             * Maintain `buf` at the start of the first line in a line
             * continuation?
             */
            tok->buf = tok->cur;
        }
        tok->line_start = tok->cur;
        tok->lineno++;
        tok->inp = end;

        goto unicodify;
    }
}

/* Back-up one character */

static void
tok_backup(register struct tok_state *tok, register unsigned int c)
{
    if (c != EOF) {
        // 0xxxxxxx
        // 110xxxxx 10xxxxxx
        // 1110xxxx 10xxxxxx 10xxxxxx
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        while ((*--tok->cur & 0xc0) == 0x80);
        if (tok->cur < tok->buf)
            Py_FatalError("tok_backup: beginning of buffer");
    }
}

unsigned int
PyToken_OneChar(unsigned int c)
{
    switch (c) {
    case '(':           return LPAR;
    case ')':           return RPAR;
    case '[':           return LSQB;
    case ']':           return RSQB;
    case ':':           return COLON;
    case ',':           return COMMA;
    case ';':           return SEMI;
    case '+':           return PLUS;
    case '-':           return MINUS;
    case '*':           return STAR;
    case '/':           return SLASH;
    case '|':           return VBAR;
    case '&':           return AMPER;
    case '<':           return LESS;
    case '>':           return GREATER;
    case '=':           return EQUAL;
    case '.':           return DOT;
    case '%':           return PERCENT;
    case '`':           return BACKQUOTE;
    case '{':           return LBRACE;
    case '}':           return RBRACE;
    case '^':           return CIRCUMFLEX;
    case '~':           return TILDE;
    case '@':           return AT;
    case 0x2295:        return CIRCLEDPLUS;
    case 0x2297:        return CIRCLEDTIMES;
    default:            return OP;
    }
}

unsigned int
PyToken_TwoChars(unsigned int c1, unsigned int c2)
{
    switch (c1) {
    case '=':
        switch (c2) {
        case '=':               return EQEQUAL;
        }
        break;
    case '!':
        switch (c2) {
        case '=':               return NOTEQUAL;
        }
        break;
    case '<':
        switch (c2) {
        case '>':               return NOTEQUAL;
        case '=':               return LESSEQUAL;
        case '<':               return LEFTSHIFT;
        }
        break;
    case '>':
        switch (c2) {
        case '=':               return GREATEREQUAL;
        case '>':               return RIGHTSHIFT;
        }
        break;
    case '+':
        switch (c2) {
        case '=':               return PLUSEQUAL;
        }
        break;
    case 0x2295:
        switch (c2) {
        case '=':               return CIRCLEDPLUSEQUAL;
        }
        break;
    case '-':
        switch (c2) {
        case '=':               return MINEQUAL;
        }
        break;
    case '*':
        switch (c2) {
        case '*':               return DOUBLESTAR;
        case '=':               return STAREQUAL;
        }
        break;
    case 0x2297:
        switch (c2) {
        case '=':               return CIRCLEDTIMESEQUAL;
        }
        break;
    case '/':
        switch (c2) {
        case '/':               return DOUBLESLASH;
        case '=':               return SLASHEQUAL;
        }
        break;
    case '|':
        switch (c2) {
        case '=':               return VBAREQUAL;
        }
        break;
    case '%':
        switch (c2) {
        case '=':               return PERCENTEQUAL;
        }
        break;
    case '&':
        switch (c2) {
        case '=':               return AMPEREQUAL;
        }
        break;
    case '^':
        switch (c2) {
        case '=':               return CIRCUMFLEXEQUAL;
        }
        break;
    }
    return OP;
}

unsigned int
PyToken_ThreeChars(unsigned int c1, unsigned int c2, unsigned int c3)
{
    switch (c1) {
    case '<':
        switch (c2) {
        case '<':
            switch (c3) {
            case '=':
                return LEFTSHIFTEQUAL;
            }
            break;
        }
        break;
    case '>':
        switch (c2) {
        case '>':
            switch (c3) {
            case '=':
                return RIGHTSHIFTEQUAL;
            }
            break;
        }
        break;
    case '*':
        switch (c2) {
        case '*':
            switch (c3) {
            case '=':
                return DOUBLESTAREQUAL;
            }
            break;
        }
        break;
    case '/':
        switch (c2) {
        case '/':
            switch (c3) {
            case '=':
                return DOUBLESLASHEQUAL;
            }
            break;
        }
        break;
    }
    return OP;
}

static int
indenterror(struct tok_state *tok)
{
    if (tok->alterror) {
        tok->done = E_TABSPACE;
        tok->cur = tok->inp;
        return 1;
    }
    return 0;
}

/* Get next token, after space stripping etc. */

static unsigned int
tok_get(register struct tok_state *tok, const unsigned char **p_start, const unsigned char **p_end)
{
    register unsigned int c;
    int blankline;

    *p_start = *p_end = NULL;
  nextline:
    tok->start = NULL;
    blankline = 0;

    /* Get indentation level */
    if (tok->atbol) {
        register int col = 0;
        register int altcol = 0;
        tok->atbol = 0;
        for (;;) {
            c = tok_nextc(tok);
            if (c == ' ')
                col++, altcol++;
            else if (c == '\t') {
                col = (col/TABSIZE + 1) * TABSIZE;
                altcol = (altcol/TABSIZE + 1) * TABSIZE;
            }
            else
                break;
        }
        tok_backup(tok, c);
        if (c == '#' || c == '\n') {
            blankline = 1;
            /* We can't jump back right here since we still
               may need to skip to the end of a comment */
        }
        if (!blankline && tok->level == 0) {
            if (col == tok->indstack[tok->indent]) {
                /* No change */
                if (altcol != tok->altindstack[tok->indent]) {
                    if (indenterror(tok))
                        return ERRORTOKEN;
                }
            }
            else if (col > tok->indstack[tok->indent]) {
                /* Indent -- always one */
                if (tok->indent+1 >= MAXINDENT) {
                    tok->done = E_TOODEEP;
                    tok->cur = tok->inp;
                    return ERRORTOKEN;
                }
                if (altcol <= tok->altindstack[tok->indent]) {
                    if (indenterror(tok))
                        return ERRORTOKEN;
                }
                tok->pendin++;
                tok->indstack[++tok->indent] = col;
                tok->altindstack[tok->indent] = altcol;
            }
            else /* col < tok->indstack[tok->indent] */ {
                /* Dedent -- any number, must be consistent */
                while (tok->indent > 0 && col < tok->indstack[tok->indent]) {
                    tok->pendin--;
                    tok->indent--;
                }
                if (col != tok->indstack[tok->indent]) {
                    tok->done = E_DEDENT;
                    tok->cur = tok->inp;
                    return ERRORTOKEN;
                }
                if (altcol != tok->altindstack[tok->indent]) {
                    if (indenterror(tok))
                        return ERRORTOKEN;
                }
            }
        }
    }

    tok->start = tok->cur;

    /* Return pending indents/dedents */
    if (tok->pendin != 0) {
        if (tok->pendin < 0) {
            tok->pendin++;
            return DEDENT;
        }
        else {
            tok->pendin--;
            return INDENT;
        }
    }

  again:
    tok->start = NULL;
    /* Skip spaces */
    do {
        tok->start = tok->cur; // Remember the start before incrementing.
        c = tok_nextc(tok);
    } while (c == ' ' || c == '\t');

    /* Skip comment */
    if (c == '#') {
        while (c != EOF && c != '\n')
            c = tok_nextc(tok);
    }

    /* Check for EOF and errors now */
    if (c == EOF)
        return tok->done == E_EOF ? ENDMARKER : ERRORTOKEN;

    /* Identifier (most frequent token!) */
    if (isalpha(c) || c == '_') {
        /* Process r"", u"" and ur"" */
        switch (c) {
        case 'b':
        case 'B':
            c = tok_nextc(tok);
            if (c == 'r' || c == 'R')
                c = tok_nextc(tok);
            if (c == '"' || c == '\'')
                goto letter_quote;
            break;
        case 'r':
        case 'R':
            c = tok_nextc(tok);
            if (c == '"' || c == '\'')
                goto letter_quote;
            break;
        case 'u':
        case 'U':
            c = tok_nextc(tok);
            if (c == 'r' || c == 'R')
                c = tok_nextc(tok);
            if (c == '"' || c == '\'')
                goto letter_quote;
            break;
        }
        while (c != EOF && (isalnum(c) || c == '_')) {
            c = tok_nextc(tok);
        }
        tok_backup(tok, c);
        *p_start = tok->start;
        *p_end = tok->cur;
        return NAME;
    }

    /* String continuation */
    if (c == '\n') {
        tok->atbol = 1;
        if (blankline || tok->level > 0)
            goto nextline;
        *p_start = tok->start;
        *p_end = tok->cur - 1; /* Leave '\n' out of the string */
        tok->cont_line = 0;
        return NEWLINE;
    }

    /* Period or number starting with period? */
    if (c == '.') {
        c = tok_nextc(tok);
        if (isdigit(c)) {
            goto fraction;
        }
        else {
            tok_backup(tok, c);
            *p_start = tok->start;
            *p_end = tok->cur;
            return DOT;
        }
    }

    /* Number */
    if (isdigit(c)) {
        if (c == '0') {
            /* Hex, octal or binary -- maybe. */
            c = tok_nextc(tok);
            if (c == '.')
                goto fraction;
#ifndef WITHOUT_COMPLEX
            if (c == 'j' || c == 'J')
                goto imaginary;
#endif
            if (c == 'x' || c == 'X') {

                /* Hex */
                c = tok_nextc(tok);
                if (!isxdigit(c)) {
                    tok->done = E_TOKEN;
                    tok_backup(tok, c);
                    return ERRORTOKEN;
                }
                do {
                    c = tok_nextc(tok);
                } while (isxdigit(c));
            }
            else if (c == 'o' || c == 'O') {
                /* Octal */
                c = tok_nextc(tok);
                if (c < '0' || c >= '8') {
                    tok->done = E_TOKEN;
                    tok_backup(tok, c);
                    return ERRORTOKEN;
                }
                do {
                    c = tok_nextc(tok);
                } while ('0' <= c && c < '8');
            }
            else if (c == 'b' || c == 'B') {
                /* Binary */
                c = tok_nextc(tok);
                if (c != '0' && c != '1') {
                    tok->done = E_TOKEN;
                    tok_backup(tok, c);
                    return ERRORTOKEN;
                }
                do {
                    c = tok_nextc(tok);
                } while (c == '0' || c == '1');
            }
            else {
                int found_decimal = 0;
                /* Octal; c is first char of it */
                /* There's no 'isoctdigit' macro, sigh */
                while ('0' <= c && c < '8') {
                    c = tok_nextc(tok);
                }
                if (isdigit(c)) {
                    found_decimal = 1;
                    do {
                        c = tok_nextc(tok);
                    } while (isdigit(c));
                }
                if (c == '.')
                    goto fraction;
                else if (c == 'e' || c == 'E')
                    goto exponent;
#ifndef WITHOUT_COMPLEX
                else if (c == 'j' || c == 'J')
                    goto imaginary;
#endif
                else if (found_decimal) {
                    tok->done = E_TOKEN;
                    tok_backup(tok, c);
                    return ERRORTOKEN;
                }
            }
            if (c == 'l' || c == 'L')
                c = tok_nextc(tok);
        }
        else {
            /* Decimal */
            do {
                c = tok_nextc(tok);
            } while (isdigit(c));
            if (c == 'l' || c == 'L')
                c = tok_nextc(tok);
            else {
                /* Accept floating point numbers. */
                if (c == '.') {
        fraction:
                    /* Fraction */
                    do {
                        c = tok_nextc(tok);
                    } while (isdigit(c));
                }
                if (c == 'e' || c == 'E') {
                    unsigned int e;
                  exponent:
                    e = c;
                    /* Exponent part */
                    c = tok_nextc(tok);
                    if (c == '+' || c == '-') {
                        c = tok_nextc(tok);
                        if (!isdigit(c)) {
                            tok->done = E_TOKEN;
                            tok_backup(tok, c);
                            return ERRORTOKEN;
                        }
                    } else if (!isdigit(c)) {
                        tok_backup(tok, c);
                        tok_backup(tok, e);
                        *p_start = tok->start;
                        *p_end = tok->cur;
                        return NUMBER;
                    }
                    do {
                        c = tok_nextc(tok);
                    } while (isdigit(c));
                }
#ifndef WITHOUT_COMPLEX
                if (c == 'j' || c == 'J')
                    /* Imaginary part */
        imaginary:
                    c = tok_nextc(tok);
#endif
            }
        }
        tok_backup(tok, c);
        *p_start = tok->start;
        *p_end = tok->cur;
        return NUMBER;
    }

  letter_quote:
    /* String */
    if (c == '\'' || c == '"') {
        Py_ssize_t quote2 = tok->cur - tok->start + 1;
        unsigned int quote = c;
        unsigned int triple = 0;
        unsigned int tripcount = 0;
        for (;;) {
            c = tok_nextc(tok);
            if (c == '\n') {
                if (!triple) {
                    tok->done = E_EOLS;
                    tok_backup(tok, c);
                    return ERRORTOKEN;
                }
                tripcount = 0;
                tok->cont_line = 1; /* multiline string. */
            }
            else if (c == EOF) {
                if (triple)
                    tok->done = E_EOFS;
                else
                    tok->done = E_EOLS;
                tok->cur = tok->inp;
                return ERRORTOKEN;
            }
            else if (c == quote) {
                tripcount++;
                if (tok->cur - tok->start == quote2) {
                    c = tok_nextc(tok);
                    if (c == quote) {
                        triple = 1;
                        tripcount = 0;
                        continue;
                    }
                    tok_backup(tok, c);
                }
                if (!triple || tripcount == 3)
                    break;
            }
            else if (c == '\\') {
                tripcount = 0;
                c = tok_nextc(tok);
                if (c == EOF) {
                    tok->done = E_EOLS;
                    tok->cur = tok->inp;
                    return ERRORTOKEN;
                }
            }
            else
                tripcount = 0;
        }
        *p_start = tok->start;
        *p_end = tok->cur;
        return STRING;
    }

    /* Line continuation */
    if (c == '\\') {
        c = tok_nextc(tok);
        if (c != '\n') {
            tok->done = E_LINECONT;
            tok->cur = tok->inp;
            return ERRORTOKEN;
        }
        tok->cont_line = 1;
        goto again; /* Read next line */
    }

    /* Check for two-character token */
    {
        unsigned int c2 = tok_nextc(tok);
        unsigned int token = PyToken_TwoChars(c, c2);
        if (token != OP) {
            unsigned int c3 = tok_nextc(tok);
            unsigned int token3 = PyToken_ThreeChars(c, c2, c3);
            if (token3 != OP) {
                token = token3;
            } else {
                tok_backup(tok, c3);
            }
            *p_start = tok->start;
            *p_end = tok->cur;
            return token;
        }
        tok_backup(tok, c2);
    }

    /* Keep track of parentheses nesting level */
    switch (c) {
    case '(':
    case '[':
    case '{':
        tok->level++;
        break;
    case ')':
    case ']':
    case '}':
        tok->level--;
        break;
    }

    /* Punctuation character */
    *p_start = tok->start;
    *p_end = tok->cur;
    return PyToken_OneChar(c);
}

unsigned int
PyTokenizer_Get(struct tok_state *tok, const unsigned char **p_start, const unsigned char **p_end)
{
    unsigned int result = tok_get(tok, p_start, p_end);
    if (tok->decoding_erred) {
        result = ERRORTOKEN;
        tok->done = E_DECODE;
    }
    return result;
}

#ifndef NDEBUG

void
tok_dump(int type, unsigned char *start, unsigned char *end)
{
    printf("%s", _PyParser_TokenNames[type]);
    if (type == NAME || type == NUMBER || type == STRING || type == OP)
        printf("(%.*s)", (int)(end - start), start);
}

#endif
