%option noyywrap
%{
#include <cstdlib>
#include <cstring>
#include <string>
#include "ast.h"
#include "yacc.tab.h"

// 字符串复制辅助函数
char* my_strdup(const char* s) {
    char* p = (char*)malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}

using std::string;
%}

INTCON     [0-9]+
IDENFR     [a-zA-Z_][a-zA-Z0-9_]*
STRCON     \"([^\"\\]|\\.)*\" 

%%
"//".*$    { /* 忽略单行注释 */ }  /* 匹配从//开始到行尾的所有内容并忽略 */

"main"              { return MAIN; }
"const"             { return CONST; }
"int"               { return INT; } 
"getint"            { return GETINT; }
"printf"            { return PRINTF; }
"return"            { return RETURN; }
"void"              { return VOID; }

"{"                 { return yytext[0]; }
"}"                 { return yytext[0]; }
"("                 { return yytext[0]; }
")"                 { return yytext[0]; }
";"                 { return yytext[0]; }
","                 { return yytext[0]; }
"["                 { return yytext[0]; }
"]"                 { return yytext[0]; }
"="                 { return yytext[0]; }

{INTCON}            { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{IDENFR}            { yylval.str_val = my_strdup(yytext); return IDENT; }
{STRCON}            { yylval.str_val = my_strdup(yytext); return STR_CONST; }

[ \t\r\n]+          ;   // 忽略空白符
.                   { return yytext[0]; }

%%