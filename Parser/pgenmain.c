
/* Parser generator main program */

/* This expects a filename containing the grammar as argv[1] (UNIX)
   or asks the console for such a file name (THINK C).
   It writes its output on two files in the current directory:
   - "graminit.c" gets the grammar as a bunch of initialized data
   - "graminit.h" gets the grammar's non-terminals as #defines.
   Error messages and status info during the generation process are
   written to stdout, or sometimes to stderr. */

/* XXX TO DO:
   - check for duplicate definitions of names (instead of fatal err)
*/

#include "Python.h"
#include "pgenheaders.h"
#include "grammar.h"
#include "pymem.h"
#include "node.h"
#include "parsetok.h"
#include "pgen.h"

int Py_VerboseFlag;
int Py_IgnoreEnvironmentFlag;

/* Forward */
grammar *getgrammar(char *filename);

void
Py_Exit(int sts)
{
    exit(sts);
}

int
main(int argc, char **argv)
{
    grammar *g;
    FILE *fp;
    char *filename, *graminit_h, *graminit_c;

    if (argc != 4) {
        fprintf(stderr,
            "usage: %s grammar graminit.h graminit.c\n", argv[0]);
        Py_Exit(2);
    }
    filename = argv[1];
    graminit_h = argv[2];
    graminit_c = argv[3];
    g = getgrammar(filename);
    fp = fopen(graminit_c, "w");
    if (fp == NULL) {
        perror(graminit_c);
        Py_Exit(1);
    }
#ifndef NDEBUG
    printf("Writing %s ...\n", graminit_c);
#endif
    printgrammar(g, fp);
    fclose(fp);
    fp = fopen(graminit_h, "w");
    if (fp == NULL) {
        perror(graminit_h);
        Py_Exit(1);
    }
#ifndef NDEBUG
    printf("Writing %s ...\n", graminit_h);
#endif
    printnonterminals(g, fp);
    fclose(fp);
    Py_Exit(0);
    return 0; /* Make gcc -Wall happy */
}

grammar *
getgrammar(char *filename)
{
    FILE *fp;
    node *n;
    grammar *g0, *g;
    perrdetail err;
    long len;
    unsigned char *file, *p;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        Py_Exit(1);
    }

    len = -ftell(fp);
    fseek(fp, 0, SEEK_END);
    len += ftell(fp);

    fseek(fp, 0, SEEK_SET);
    p = file = PyMem_MALLOC(len+1);
    do {
      p += fread(p, 1, 512, fp);

    } while (!ferror(fp) && !feof(fp));
    file[len] = '\0';
    assert(len == p - file);

    fclose(fp);

    g0 = meta_grammar();
    n = PyParser_ParseString(file, g0, g0->g_start, &err);
    if (n == NULL) {
        fprintf(stderr, "Parsing error %d, line %d.\n",
            err.error, err.lineno);
        if (err.text != NULL) {
            size_t i;
            fprintf(stderr, "%s", err.text);
            i = strlen((const char *)err.text);
            if (i == 0 || err.text[i-1] != '\n')
                fprintf(stderr, "\n");
            for (i = 0; i < err.offset; i++) {
                if (err.text[i] == '\t')
                    putc('\t', stderr);
                else
                    putc(' ', stderr);
            }
            fprintf(stderr, "^\n");
            PyMem_FREE(err.text);
        }
        Py_Exit(1);
    }
    g = pgen(n);
    if (g == NULL) {
        printf("Bad grammar.\n");
        Py_Exit(1);
    }
    return g;
}

void
Py_FatalError(const char *msg)
{
    fprintf(stderr, "pgen: FATAL ERROR: %s\n", msg);
    Py_Exit(1);
}

#include <stdarg.h>

void
PySys_WriteStderr(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
}
