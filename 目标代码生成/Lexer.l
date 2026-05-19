%option noyywrap nodefault yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

extern int fileno(FILE *);

#include "compiler.h"
#include "parser.tab.h"

static char *float_literal_bits(const char *text) {
    float value = strtof(text, NULL);
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", bits);
    return xstrdup(buf);
}
%}

HEXCON      0[xX][0-9A-Fa-f]+
FLOATCON    (0[xX]([0-9A-Fa-f]+\.?[0-9A-Fa-f]*|\.[0-9A-Fa-f]+)[pP][+-]?[0-9]+[fFlL]?)|(([0-9]+\.[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?[fFlL]?)|([0-9]+[eE][+-]?[0-9]+[fFlL]?)
OCTCON      0[0-7]+
DECCON      [1-9][0-9]*|0
IDENFRR     [A-Za-z_][A-Za-z0-9_]*
STRCONR     \"([^\"\\\n]|\\.)*\"

%%

"const"             { return CONSTTK; }
"int"               { return INTTK; }
"float"             { return FLOATTK; }
"void"              { return VOIDTK; }
"if"                { return IFTK; }
"else"              { return ELSETK; }
"while"             { return WHILETK; }
"break"             { return BREAKTK; }
"continue"          { return CONTINUETK; }
"return"            { return RETURNTK; }
"getint"            { return GETINTTK; }
"printf"            { return PRINTFTK; }
"putf"              { return PRINTFTK; }

"<="                { return LEQ; }
">="                { return GEQ; }
"=="                { return EQL; }
"!="                { return NEQ; }
"&&"                { return AND; }
"||"                { return OR; }
"="                 { return ASSIGN; }
"<"                 { return LSS; }
">"                 { return GRE; }
"!"                 { return NOT; }
"+"                 { return PLUS; }
"-"                 { return MINU; }
"*"                 { return MULT; }
"/"                 { return DIV; }
"%"                 { return MOD; }

"("                 { return LPARENT; }
")"                 { return RPARENT; }
"["                 { return LBRACK; }
"]"                 { return RBRACK; }
"{"                 { return LBRACE; }
"}"                 { return RBRACE; }
","                 { return COMMA; }
";"                 { return SEMICN; }

{HEXCON}            { yylval.str = xstrdup(yytext); return INTCON; }
{FLOATCON}          { yylval.str = float_literal_bits(yytext); return FLOATCONTK; }
{OCTCON}            { yylval.str = xstrdup(yytext); return INTCON; }
{DECCON}            { yylval.str = xstrdup(yytext); return INTCON; }
{STRCONR}           { yylval.str = xstrdup(yytext); return STRCON; }
{IDENFRR}           { yylval.str = xstrdup(yytext); return IDENFR; }

"//".*              ;
"/*"([^*]|\*+[^*/])*\*+"/" ;
[ \t\r\n]+          ;

.                   { fprintf(stderr, "unknown token at line %d: %s\n", yylineno, yytext); return 0; }

%%
