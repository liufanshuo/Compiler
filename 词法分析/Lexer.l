%option noyywrap nodefault yylineno

%x COMMENT

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SYNTAX_MODE
#define SYNTAX_MODE 1
#endif

#ifndef DEBUG_TOKENS
#define DEBUG_TOKENS 0
#endif

#if SYNTAX_MODE
#include "y.tab.h"
extern FILE *output_file;
#else
static FILE *output_file = NULL;
enum {
    CONSTTK = 256,
    INTTK,
    VOIDTK,
    IFTK,
    ELSETK,
    WHILETK,
    BREAKTK,
    CONTINUETK,
    RETURNTK,
    GETINTTK,
    PRINTFTK,
    IDENFR,
    INTCON,
    STRCON,
    LEQ,
    GEQ,
    EQL,
    NEQ,
    LSS,
    GRE,
    AND,
    OR,
    NOT,
    PLUS,
    MINU,
    MULT,
    DIV,
    MOD,
    ASSIGN,
    LPARENT,
    RPARENT,
    LBRACK,
    RBRACK,
    LBRACE,
    RBRACE,
    COMMA,
    SEMICN
};
#endif

static void debug_emit(const char *token_name, const char *lexeme) {
    if (DEBUG_TOKENS) {
        fprintf(stderr, "%s %s\n", token_name, lexeme);
    }
}

#define RET(token_name, token_code)                                      \
    do {                                                                 \
        if (!SYNTAX_MODE && output_file != NULL) {                       \
            fprintf(output_file, token_name " %s\n", yytext);            \
        }                                                                \
        debug_emit(token_name, yytext);                                  \
        return token_code;                                               \
    } while (0)
%}

HEXCON      0[xX][0-9A-Fa-f]+
INTCON      [0-9]+
IDENFR      [A-Za-z_][A-Za-z0-9_]*
STRCON      \"([^\"\\\n]|\\.)*\"

%%

"const"             { RET("CONSTTK", CONSTTK); }
"int"               { RET("INTTK", INTTK); }
"void"              { RET("VOIDTK", VOIDTK); }
"if"                { RET("IFTK", IFTK); }
"else"              { RET("ELSETK", ELSETK); }
"while"             { RET("WHILETK", WHILETK); }
"break"             { RET("BREAKTK", BREAKTK); }
"continue"          { RET("CONTINUETK", CONTINUETK); }
"return"            { RET("RETURNTK", RETURNTK); }

"main"              { RET("IDENFR", IDENFR); }
"getint"            {
                        if (SYNTAX_MODE) {
                            RET("GETINTTK", GETINTTK);
                        }
                        RET("IDENFR", IDENFR);
                    }
"printf"            {
                        if (SYNTAX_MODE) {
                            RET("PRINTFTK", PRINTFTK);
                        }
                        RET("IDENFR", IDENFR);
                    }

"<="                { RET("LEQ", LEQ); }
">="                { RET("GEQ", GEQ); }
"=="                { RET("EQL", EQL); }
"!="                { RET("NEQ", NEQ); }
"&&"                { RET("AND", AND); }
"||"                { RET("OR", OR); }
"="                 { RET("ASSIGN", ASSIGN); }
"<"                 { RET("LSS", LSS); }
">"                 { RET("GRE", GRE); }
"!"                 { RET("NOT", NOT); }
"+"                 { RET("PLUS", PLUS); }
"-"                 { RET("MINU", MINU); }
"*"                 { RET("MULT", MULT); }
"/"                 { RET("DIV", DIV); }
"%"                 { RET("MOD", MOD); }

"("                 { RET("LPARENT", LPARENT); }
")"                 { RET("RPARENT", RPARENT); }
"["                 { RET("LBRACK", LBRACK); }
"]"                 { RET("RBRACK", RBRACK); }
"{"                 { RET("LBRACE", LBRACE); }
"}"                 { RET("RBRACE", RBRACE); }
","                 { RET("COMMA", COMMA); }
";"                 { RET("SEMICN", SEMICN); }

{HEXCON}            { RET("INTCON", INTCON); }
{INTCON}            { RET("INTCON", INTCON); }
{STRCON}            { RET("STRCON", STRCON); }
{IDENFR}            { RET("IDENFR", IDENFR); }

"//".*              ;
"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         ;
<COMMENT>.          ;

[ \t\r\n]+          ;

.                   { fprintf(stderr, "Unknown token at line %d: %s\n", yylineno, yytext); return 0; }

%%

#if !SYNTAX_MODE
int main(void) {
    yyin = fopen("testfile.txt", "r");
    if (yyin == NULL) {
        return 1;
    }

    output_file = fopen("output.txt", "w");
    if (output_file == NULL) {
        fclose(yyin);
        return 1;
    }

    while (yylex()) {
    }

    fclose(yyin);
    fclose(output_file);
    return 0;
}
#endif
