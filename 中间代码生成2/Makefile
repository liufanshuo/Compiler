CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

all: compiler

compiler: parser.tab.c lex.yy.c compiler.c
	$(CC) $(CFLAGS) -o $@ parser.tab.c lex.yy.c compiler.c

parser.tab.c parser.tab.h: Parser.y compiler.h
	bison -d -o parser.tab.c Parser.y

lex.yy.c: Lexer.l parser.tab.h compiler.h
	flex -o lex.yy.c Lexer.l

clean:
	rm -f compiler parser.tab.c parser.tab.h lex.yy.c output.ll
