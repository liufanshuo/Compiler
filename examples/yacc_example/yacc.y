%{
#define YYDEBUG 1

#include <stdio.h>
#include <stdlib.h>

int yylex();
void yyerror(const char *s);

%}

%token MAIN CONST INT BREAK CONTINUE IF ELSE
%token WHILE GETINT PRINTF RETURN VOID
%token IDENT INT_CONST STR_CONST

%start CompUnit
%%

// CompUnit -> {Decl} {FuncDef} MainFuncDef
CompUnit
    : Decl MainFuncDef {
        printf("<CompUnit>\n");
    }
    ;

Decl : 
    ConstDecl ';' {
        printf("<Decl>\n");
    } | VarDecl ';' {
        printf("<Decl>\n");
    }
    ;

// ConstDecl -> 'const' xxx
ConstDecl :
    CONST BType IDENT '=' Number {
        printf("<ConstDecl>\n");
    }
    ;

// BType -> 'int'
BType : 
    INT {

    } |
    VOID {

    }
    ;

// VarDecl 
VarDecl :
    BType IDENT {
        printf("<VarDecl>\n");
    }
    ;

MainFuncDef :
    INT MAIN '(' ')' Block {
        printf("<MainFuncDef>\n");
    }
    ;

Block :
    '{' BlockItems '}' {
        printf("<Block>\n");
    } | '{' '}' {
        printf("<Block>\n");
    }
    ;

BlockItems :
    BlockItems BlockItem {

    } | BlockItem {

    }
    ;

BlockItem :
    Decl {

    } | Stmt {

    }
    ;

Stmt :
    Assignment ';' {
        printf("<Stmt>\n");
    }
    | Block {
        printf("<Stmt>\n");
    }
    | RETURN IDENT ';' {
        printf("<Stmt>\n");
    }
    ;

Assignment :
    IDENT '=' GETINT '(' ')' {

    }
    ;

Number :
    INT_CONST {

    }
    ;
%% 

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s\n", s);
}

int main() {
    yydebug = 0;  // 1为启用调试，0为不启用调试
    return yyparse();
}