%option noyywrap
%{

#include <stdio.h>
#include <stdlib.h>

%}

INTCON     [0-9]+
IDENFR     [a-zA-Z_][a-zA-Z0-9_]*
STRCON     \"([^\"\\]|\\.)*\" 

%%

"const"             { printf("CONSTTK const\n"); }
"int"               { printf("INTTK int\n"); }
"return"            { printf("RETURNTK return\n"); }
"printf"            { printf("PRINTFTK printf\n"); }
"getint"            { printf("GETINTTK getint\n"); }
"main"              { printf("MAINTK main\n"); }

"="                 { printf("ASSIGN =\n"); }
"{"                 { printf("LBRACE %s\n", yytext); }
"}"                 { printf("RBRACE %s\n", yytext); }
"("                 { printf("LPARENT %s\n", yytext); }
")"                 { printf("RPARENT %s\n", yytext); }
";"                 { printf("SEMICN ;\n"); }
","                 { printf("COMMA ,\n"); }
"["                 { printf("LBRACK %s\n", yytext); }
"]"                 { printf("RBRACK %s\n", yytext); }

{INTCON}            { printf("INTCON %s\n", yytext); }
{IDENFR}            { printf("IDENFR %s\n", yytext); }
{STRCON}            { printf("STRCON %s\n", yytext); }

[ \t\r\n]+          ;   // 忽略空白符
.                   { printf("TOKEN: UNKNOWN (%s)\n", yytext); }

%%

int main(int argc, char *argv[]) {
    yyin = stdin;
    yylex();
    return 0;
}