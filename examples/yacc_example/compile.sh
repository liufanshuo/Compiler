bison -d yacc.y      # 生成 parser.tab.c 和 parser.tab.h
flex lex.l             # 生成 lex.yy.c
gcc lex.yy.c yacc.tab.c -o parser -lm