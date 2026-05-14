CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

all: compiler

compiler: parser.tab.c parser.tab.h lex.yy.c compiler.c compiler.h
	$(CC) $(CFLAGS) -o $@ parser.tab.c lex.yy.c compiler.c

clean:
	rm -f compiler output.ll output.s
