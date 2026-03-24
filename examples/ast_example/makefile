CC = g++
CFLAGS = -std=c++17 -Wall -g -Wno-register

all: parser

parser: lex.yy.c yacc.tab.c main.cpp
	$(CC) $(CFLAGS) -o parser lex.yy.c yacc.tab.c main.cpp

yacc.tab.c yacc.tab.h: yacc.y
	bison -d yacc.y

lex.yy.c: lex.l yacc.tab.h
	flex lex.l

clean:
	rm -f parser lex.yy.c yacc.tab.c yacc.tab.h *.dot *.png *.o

test: parser
	./parser test.txt -o ast.dot
	dot -Tpng ast.dot -o ast.png
	display ast.png

.PHONY: all clean test
