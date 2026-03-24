%option noyywrap
%{

#include "yacc.tab.h"

%}

INTCON     [0-9]+
IDENFR     [a-zA-Z_][a-zA-Z0-9_]*
STRCON     \"([^\"\\]|\\.)*\" 

%%
"//".*$    { /* 忽略单行注释 */ }  /* 匹配从//开始到行尾的所有内容并忽略 */

"main"              { printf("MAINTK main\n"); return MAIN; }
"const"             { printf("CONSTTK const\n"); return CONST; }
"int"               { printf("INTTK int\n"); return INT; } 
"getint"            { printf("GETINTTK getint\n"); return GETINT; }
"printf"            { printf("PRINTFTK printf\n"); return PRINTF; }
"return"            { printf("RETURNTK return\n"); return RETURN; }

"{"                 { printf("LBRACE %s\n", yytext); return yytext[0]; }
"}"                 { printf("RBRACE %s\n", yytext); return yytext[0]; }
"("                 { printf("LPARENT %s\n", yytext); return yytext[0]; }
")"                 { printf("RPARENT %s\n", yytext); return yytext[0]; }
";"                 { printf("SEMICN ;\n"); return yytext[0]; }
","                 { printf("COMMA ,\n"); return yytext[0]; }
"["                 { printf("LBRACK %s\n", yytext); return yytext[0]; }
"]"                 { printf("RBRACK %s\n", yytext); return yytext[0]; }

"="                 { printf("ASSIGN =\n"); return yytext[0]; }

{INTCON}            { printf("INTCON %s\n", yytext); return INT_CONST; }
{IDENFR}            { printf("IDENFR %s\n", yytext); return IDENT; }
{STRCON}            { printf("STRCON %s\n", yytext); return STR_CONST; }

[ \t\r\n]+          ;   // 忽略空白符
.                   { printf("TOKEN: UNKNOWN (%s)\n", yytext); return yytext[0]; }

%%