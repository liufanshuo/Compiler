%{
#define SYNTAX_MODE 1
#define DEBUG_TOKENS 0

#include <stdio.h>
#include <stdlib.h>

int yylex(void);
void yyerror(const char *s);
extern int yylineno;
extern FILE *yyin;

FILE *output_file = NULL;

static void print_node(const char *name) {
    fprintf(output_file, "%s\n", name);
}
%}

%token CONSTTK INTTK VOIDTK
%token IFTK ELSETK WHILETK BREAKTK CONTINUETK RETURNTK
%token GETINTTK PRINTFTK
%token IDENFR INTCON STRCON
%token LEQ GEQ EQL NEQ LSS GRE
%token AND OR NOT
%token PLUS MINU MULT DIV MOD
%token ASSIGN
%token LPARENT RPARENT LBRACK RBRACK LBRACE RBRACE COMMA SEMICN

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSETK

%start CompUnit

%%

CompUnit
    : CompUnitItem
        { print_node("<CompUnit>"); }
    | CompUnit CompUnitItem
        { print_node("<CompUnit>"); }
    ;

CompUnitItem
    : ConstDecl
    | GlobalIntItem
    | VoidFuncDef
    ;

ConstDecl
    : CONSTTK BType ConstDefList SEMICN
        { print_node("<ConstDecl>"); }
    ;

ConstDefList
    : ConstDef
    | ConstDefList COMMA ConstDef
    ;

BType
    : INTTK
    ;

ConstDef
    : IDENFR ConstDefDims ASSIGN ConstInitVal
        { print_node("<ConstDef>"); }
    ;

ConstDefDims
    :
    | ConstDefDims LBRACK ConstExp RBRACK
    ;

ConstInitVal
    : ConstExp
        { print_node("<ConstInitVal>"); }
    | LBRACE RBRACE
        { print_node("<ConstInitVal>"); }
    | LBRACE ConstInitValList RBRACE
        { print_node("<ConstInitVal>"); }
    ;

ConstInitValList
    : ConstInitVal
    | ConstInitValList COMMA ConstInitVal
    ;

Decl
    : ConstDecl
    | VarDecl
    ;

GlobalIntItem
    : INTTK IDENFR GlobalIntTail
    ;

GlobalIntTail
    : GlobalVarDefTail GlobalVarDefMore SEMICN
        { print_node("<VarDecl>"); }
    | LPARENT
        { print_node("<FuncType>"); }
      RPARENT Block
        { print_node("<FuncDef>"); }
    | LPARENT
        { print_node("<FuncType>"); }
      FuncFParams RPARENT Block
        { print_node("<FuncDef>"); }
    ;

GlobalVarDefTail
    : VarDefDims VarInitOpt
        { print_node("<VarDef>"); }
    ;

GlobalVarDefMore
    :
    | GlobalVarDefMore COMMA VarDef
    ;

VoidFuncDef
    : VOIDTK IDENFR LPARENT
        { print_node("<FuncType>"); }
      RPARENT Block
        { print_node("<FuncDef>"); }
    | VOIDTK IDENFR LPARENT
        { print_node("<FuncType>"); }
      FuncFParams RPARENT Block
        { print_node("<FuncDef>"); }
    ;

VarDecl
    : BType VarDefList SEMICN
        { print_node("<VarDecl>"); }
    ;

VarDefList
    : VarDef
    | VarDefList COMMA VarDef
    ;

VarDef
    : IDENFR VarDefDims VarInitOpt
        { print_node("<VarDef>"); }
    ;

VarDefDims
    :
    | VarDefDims LBRACK ConstExp RBRACK
    ;

VarInitOpt
    :
    | ASSIGN InitVal
    ;

InitVal
    : Exp
        { print_node("<InitVal>"); }
    | LBRACE RBRACE
        { print_node("<InitVal>"); }
    | LBRACE InitValList RBRACE
        { print_node("<InitVal>"); }
    ;

InitValList
    : InitVal
    | InitValList COMMA InitVal
    ;

FuncFParams
    : FuncFParamsList
        { print_node("<FuncFParams>"); }
    ;

FuncFParamsList
    : FuncFParam
    | FuncFParamsList COMMA FuncFParam
    ;

FuncFParam
    : BType IDENFR FuncFParamSuffix
        { print_node("<FuncFParam>"); }
    ;

FuncFParamSuffix
    :
    | LBRACK RBRACK FuncFParamArrayDims
    ;

FuncFParamArrayDims
    :
    | FuncFParamArrayDims LBRACK Exp RBRACK
    ;

Block
    : LBRACE BlockItemList RBRACE
        { print_node("<Block>"); }
    ;

BlockItemList
    :
    | BlockItemList BlockItem
    ;

BlockItem
    : Decl
    | Stmt
    ;

Stmt
    : LVal ASSIGN Exp SEMICN
        { print_node("<Stmt>"); }
    | ExpOpt SEMICN
        { print_node("<Stmt>"); }
    | Block
        { print_node("<Stmt>"); }
    | IFTK LPARENT Cond RPARENT Stmt %prec LOWER_THAN_ELSE
        { print_node("<Stmt>"); }
    | IFTK LPARENT Cond RPARENT Stmt ELSETK Stmt
        { print_node("<Stmt>"); }
    | WHILETK LPARENT Cond RPARENT Stmt
        { print_node("<Stmt>"); }
    | BREAKTK SEMICN
        { print_node("<Stmt>"); }
    | CONTINUETK SEMICN
        { print_node("<Stmt>"); }
    | RETURNTK SEMICN
        { print_node("<Stmt>"); }
    | RETURNTK Exp SEMICN
        { print_node("<Stmt>"); }
    | PRINTFTK LPARENT STRCON RPARENT SEMICN
        { print_node("<Stmt>"); }
    | PRINTFTK LPARENT STRCON PrintfArgs RPARENT SEMICN
        { print_node("<Stmt>"); }
    ;

ExpOpt
    :
    | Exp
    ;

PrintfArgs
    : COMMA Exp
    | PrintfArgs COMMA Exp
    ;

Exp
    : AddExp
        { print_node("<Exp>"); }
    ;

Cond
    : LOrExp
        { print_node("<Cond>"); }
    ;

LVal
    : IDENFR LValIndices
        { print_node("<LVal>"); }
    ;

LValIndices
    :
    | LValIndices LBRACK Exp RBRACK
    ;

PrimaryExp
    : LPARENT Exp RPARENT
        { print_node("<PrimaryExp>"); }
    | LVal
        { print_node("<PrimaryExp>"); }
    | Number
        { print_node("<PrimaryExp>"); }
    ;

Number
    : INTCON
        { print_node("<Number>"); }
    ;

UnaryExp
    : PrimaryExp
        { print_node("<UnaryExp>"); }
    | IDENFR LPARENT RPARENT
        { print_node("<UnaryExp>"); }
    | IDENFR LPARENT FuncRParams RPARENT
        { print_node("<UnaryExp>"); }
    | GETINTTK LPARENT RPARENT
        { print_node("<UnaryExp>"); }
    | UnaryOp UnaryExp
        { print_node("<UnaryExp>"); }
    ;

UnaryOp
    : PLUS
        { print_node("<UnaryOp>"); }
    | MINU
        { print_node("<UnaryOp>"); }
    | NOT
        { print_node("<UnaryOp>"); }
    ;

FuncRParams
    : FuncRParamsList
        { print_node("<FuncRParams>"); }
    ;

FuncRParamsList
    : Exp
    | FuncRParamsList COMMA Exp
    ;

MulExp
    : UnaryExp
        { print_node("<MulExp>"); }
    | MulExp MULT UnaryExp
        { print_node("<MulExp>"); }
    | MulExp DIV UnaryExp
        { print_node("<MulExp>"); }
    | MulExp MOD UnaryExp
        { print_node("<MulExp>"); }
    ;

AddExp
    : MulExp
        { print_node("<AddExp>"); }
    | AddExp PLUS MulExp
        { print_node("<AddExp>"); }
    | AddExp MINU MulExp
        { print_node("<AddExp>"); }
    ;

RelExp
    : AddExp
        { print_node("<RelExp>"); }
    | RelExp LSS AddExp
        { print_node("<RelExp>"); }
    | RelExp GRE AddExp
        { print_node("<RelExp>"); }
    | RelExp LEQ AddExp
        { print_node("<RelExp>"); }
    | RelExp GEQ AddExp
        { print_node("<RelExp>"); }
    ;

EqExp
    : RelExp
        { print_node("<EqExp>"); }
    | EqExp EQL RelExp
        { print_node("<EqExp>"); }
    | EqExp NEQ RelExp
        { print_node("<EqExp>"); }
    ;

LAndExp
    : EqExp
        { print_node("<LAndExp>"); }
    | LAndExp AND EqExp
        { print_node("<LAndExp>"); }
    ;

LOrExp
    : LAndExp
        { print_node("<LOrExp>"); }
    | LOrExp OR LAndExp
        { print_node("<LOrExp>"); }
    ;

ConstExp
    : AddExp
        { print_node("<ConstExp>"); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Syntax error at line %d: %s\n", yylineno, s);
}

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

    if (yyparse() != 0) {
        fclose(output_file);
        fclose(yyin);
        return 1;
    }

    fclose(output_file);
    fclose(yyin);
    return 0;
}
