CC=     gcc -pthread
CXX=    g++ -pthread
EMCC=   /home/popham/emscripten/emcc

# Compiler options
OPT=       -DNDEBUG -fwrapv -O3 -Wall -Wstrict-prototypes
BASECFLAGS=-fno-strict-aliasing
CFLAGS=    -I. -IInclude $(BASECFLAGS) -ggdb -Wall
EMFLAGS=   -I. -IInclude -O2 --profiling
# --closure 1 -Os
CPPFLAGS=  -I. -IInclude

##########################################################################
# Grammar
GRAMMAR_H=	Include/graminit.h
GRAMMAR_C=	Magicate/graminit.c
GRAMMAR_INPUT=	Grammar/Grammar

##########################################################################
# Parser

PSRCS=Parser/acceler.c \
      Parser/grammar1.c \
      Parser/listnode.c \
      Parser/node.c \
      Parser/parser.c \
      Parser/parsetok.c \
      Parser/bitset.c \
      Parser/metagrammar.c \
      Parser/firstsets.c \
      Parser/grammar.c \
      Parser/pgen.c \
      Parser/decode.c

POBJS=Parser/acceler.o \
      Parser/grammar1.o \
      Parser/listnode.o \
      Parser/node.o \
      Parser/parser.o \
      Parser/parsetok.o \
      Parser/bitset.o \
      Parser/metagrammar.o \
      Parser/firstsets.o \
      Parser/grammar.o \
      Parser/pgen.o \
      Parser/decode.o

PARSER_OBJS=$(POBJS) Parser/tokenizer.o

PGSRCS=Parser/tokenizer_pgen.c \
       Parser/printgrammar.c \
       Parser/pgenmain.c

PGOBJS=Parser/tokenizer_pgen.o \
       Parser/printgrammar.o \
       Parser/pgenmain.o

PARSER_HEADERS=Parser/parser.h \
               Parser/tokenizer.h

PGENSRCS=$(PSRCS) $(PGSRCS)
PGENOBJS=$(POBJS) $(PGOBJS)

MAGSRCS=Magicate/magicate.c \
        Magicate/graminit.c \
        Parser/acceler.c \
        Parser/grammar1.c \
        Parser/node.c \
        Parser/parser.c \
        Parser/parsetok.c \
        Parser/tokenizer.c \
        Parser/bitset.c \
        Parser/grammar.c \
        Parser/decode.c

MAGOBJS=Magicate/magicate.o \
        Magicate/graminit.o \
        Parser/acceler.o \
        Parser/grammar1.o \
        Parser/node.o \
        Parser/parser.o \
        Parser/parsetok.o \
        Parser/tokenizer.o \
        Parser/bitset.o \
        Parser/grammar.o \
        Parser/decode.o

#########################################################################
# Rules

# Default target
all: build_all
build_all: Parser/pgen magicatec magicate
clean:
	rm -f Parser/pgen $(POBJS) $(PGOBJS) $(MAGOBJS)
	rm -f Include/graminit.h
	rm -f Magicate/graminit.c
	rm -f Magicate/cli
	rm -f Magicate/magicate.js
	rm -f Magicate/magicate.js.mem

$(GRAMMAR_H): $(GRAMMAR_INPUT) $(PGENSRCS)
	touch -c $(GRAMMAR_H)

$(GRAMMAR_C): $(GRAMMAR_H) $(GRAMMAR_INPUT) Parser/pgen
	Parser/pgen $(GRAMMAR_INPUT) $(GRAMMAR_H) $(GRAMMAR_C)
	touch -c $(GRAMMAR_H)
	touch -c $(GRAMMAR_C)

Parser/pgen: $(PGENOBJS)
	$(CC) $(CFLAGS) $(PGENOBJS) -o Parser/pgen

Parser/grammar.o: Parser/grammar.c \
                  Include/token.h \
                  Include/grammar.h

magicatec: $(MAGOBJS) $(GRAMMAR_C)
	$(CC) $(CFLAGS) $(MAGOBJS) Magicate/main.c -o Magicate/cli

magicate: $(MAGOBJS)
	$(EMCC) --js-library Magicate/js_alloc.js --js-library Magicate/signal.js -s EXPORTED_FUNCTIONS="['_magicate']" $(EMFLAGS) $(MAGSRCS) -o Magicate/magicate.js

Magicate/magicate.o: Magicate/graminit.o
