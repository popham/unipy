#include "magicate.h"

extern unsigned char *magicate(const unsigned char *source);

void
Py_Exit(int sts)
{
    exit(sts);
}

unsigned char *js_alloc(size_t length)
{
    return malloc(length);
}

void
Py_FatalError(const char *msg)
{
    fprintf(stderr, "Fatal Python error: %s\n", msg);
    fflush(stderr);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    long len;
    char *filename;
    unsigned char *file, *p;

    if (argc != 2) {
        fprintf(stderr,
            "usage: %s x.py\n", argv[0]);
        Py_Exit(2);
    }
    filename = argv[1];
    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        Py_Exit(1);
    }
    printf("Reading %s ...\n", filename);
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

    printf("Preimage:\n%s\n", file);

    printf("Image:\n%s\n", magicate(file));

    Py_Exit(0);
    return 0; /* Make gcc -Wall happy */
}
