%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

int yylex(void);
void yyerror(const char *s);
extern int yylineno;
%}

%code requires {
#include "compiler.h"
}

%union {
    char *str;
    int intval;
    IntList int_list;
    Expr *expr;
    ExprList expr_list;
    InitVal *init;
    InitValList init_list;
    LVal *lval;
    DeclItem *decl_item;
    DeclItemList decl_items;
    Decl *decl;
    Param *param;
    ParamList params;
    Stmt *stmt;
    Block *block;
    BlockItemList block_items;
    FuncDef *func;
    TopLevelItem *top_item;
    TopLevelList top_items;
    TypeSpec type_spec;
}

%token CONSTTK INTTK FLOATTK VOIDTK
%token IFTK ELSETK WHILETK BREAKTK CONTINUETK RETURNTK
%token GETINTTK PRINTFTK
%token LEQ GEQ EQL NEQ LSS GRE
%token AND OR NOT
%token PLUS MINU MULT DIV MOD
%token ASSIGN
%token LPARENT RPARENT LBRACK RBRACK LBRACE RBRACE COMMA SEMICN
%token <str> IDENFR INTCON FLOATCONTK STRCON

%type <top_items> CompUnit
%type <top_item> CompUnitItem GlobalIntItem VoidFuncDef
%type <decl> Decl ConstDecl VarDecl
%type <decl_items> ConstDefList VarDefList
%type <decl_item> ConstDef VarDef
%type <int_list> ConstDefDims VarDefDims FuncFParamSuffix FuncFParamArrayDims
%type <init> ConstInitVal InitVal GlobalVarInitOpt
%type <init_list> ConstInitValList InitValList
%type <params> FuncFParams FuncFParamsList
%type <param> FuncFParam
%type <block> Block
%type <block_items> BlockItemList
%type <stmt> Stmt
%type <expr> Exp Cond PrimaryExp Number UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp ConstExp
%type <expr> ExpOpt
%type <expr_list> FuncRParams FuncRParamsList PrintfArgs LValIndices
%type <lval> LVal
%type <type_spec> BType
%type <decl_items> GlobalVarMore

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSETK

%start CompUnit

%%

CompUnit
    : CompUnitItem
        {
            TopLevelList list = {0};
            top_level_list_push(&list, $1);
            $$ = list;
            g_program = make_program($$);
        }
    | CompUnit CompUnitItem
        {
            $$ = $1;
            top_level_list_push(&$$, $2);
            g_program = make_program($$);
        }
    ;

CompUnitItem
    : Decl
        { $$ = make_top_decl($1); }
    | GlobalIntItem
        { $$ = $1; }
    | VoidFuncDef
        { $$ = $1; }
    ;

GlobalIntItem
    : BType IDENFR LPARENT RPARENT Block
        {
            ParamList list = {0};
            $$ = make_top_func(make_func($1, $2, list, $5));
        }
    | BType IDENFR LPARENT FuncFParams RPARENT Block
        { $$ = make_top_func(make_func($1, $2, $4, $6)); }
    | BType IDENFR VarDefDims GlobalVarInitOpt GlobalVarMore SEMICN
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, make_decl_item($2, $3, $4));
            for (int i = 0; i < $5.count; ++i) {
                decl_item_list_push(&list, $5.items[i]);
            }
            $$ = make_top_decl(make_decl($1, false, list));
        }
    ;

GlobalVarInitOpt
    :
        { $$ = NULL; }
    | ASSIGN InitVal
        { $$ = $2; }
    ;

GlobalVarMore
    :
        {
            DeclItemList list = {0};
            $$ = list;
        }
    | GlobalVarMore COMMA VarDef
        {
            $$ = $1;
            decl_item_list_push(&$$, $3);
        }
    ;

Decl
    : ConstDecl { $$ = $1; }
    | VarDecl { $$ = $1; }
    ;

ConstDecl
    : CONSTTK BType ConstDefList SEMICN
        { $$ = make_decl($2, true, $3); }
    ;

ConstDefList
    : ConstDef
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, $1);
            $$ = list;
        }
    | ConstDefList COMMA ConstDef
        {
            $$ = $1;
            decl_item_list_push(&$$, $3);
        }
    ;

BType
    : INTTK { $$ = TYPE_INT; }
    | FLOATTK { $$ = TYPE_FLOAT; }
    ;

ConstDef
    : IDENFR ConstDefDims ASSIGN ConstInitVal
        {
            $$ = make_decl_item($1, $2, $4);
            if ($2.count == 0 && $4->is_expr) {
                register_const_binding($1, eval_const_ast_expr($4->expr));
            }
        }
    ;

ConstDefDims
    :
        { IntList list = {0}; $$ = list; }
    | ConstDefDims LBRACK ConstExp RBRACK
        {
            $$ = $1;
            int_list_push(&$$, eval_const_ast_expr($3));
        }
    ;

ConstInitVal
    : ConstExp
        { $$ = make_expr_init($1); }
    | LBRACE RBRACE
        {
            InitValList list = {0};
            $$ = make_list_init(list);
        }
    | LBRACE ConstInitValList RBRACE
        { $$ = make_list_init($2); }
    ;

ConstInitValList
    : ConstInitVal
        {
            InitValList list = {0};
            init_list_push(&list, $1);
            $$ = list;
        }
    | ConstInitValList COMMA ConstInitVal
        {
            $$ = $1;
            init_list_push(&$$, $3);
        }
    ;

VoidFuncDef
    : VOIDTK IDENFR LPARENT RPARENT Block
        {
            ParamList list = {0};
            $$ = make_top_func(make_func(TYPE_VOID, $2, list, $5));
        }
    | VOIDTK IDENFR LPARENT FuncFParams RPARENT Block
        { $$ = make_top_func(make_func(TYPE_VOID, $2, $4, $6)); }
    ;

VarDecl
    : BType VarDefList SEMICN
        { $$ = make_decl($1, false, $2); }
    ;

VarDefList
    : VarDef
        {
            DeclItemList list = {0};
            decl_item_list_push(&list, $1);
            $$ = list;
        }
    | VarDefList COMMA VarDef
        {
            $$ = $1;
            decl_item_list_push(&$$, $3);
        }
    ;

VarDef
    : IDENFR VarDefDims
        { $$ = make_decl_item($1, $2, NULL); }
    | IDENFR VarDefDims ASSIGN InitVal
        { $$ = make_decl_item($1, $2, $4); }
    ;

VarDefDims
    :
        { IntList list = {0}; $$ = list; }
    | VarDefDims LBRACK ConstExp RBRACK
        {
            $$ = $1;
            int_list_push(&$$, eval_const_ast_expr($3));
        }
    ;

InitVal
    : Exp
        { $$ = make_expr_init($1); }
    | LBRACE RBRACE
        {
            InitValList list = {0};
            $$ = make_list_init(list);
        }
    | LBRACE InitValList RBRACE
        { $$ = make_list_init($2); }
    ;

InitValList
    : InitVal
        {
            InitValList list = {0};
            init_list_push(&list, $1);
            $$ = list;
        }
    | InitValList COMMA InitVal
        {
            $$ = $1;
            init_list_push(&$$, $3);
        }
    ;

FuncFParams
    : FuncFParamsList { $$ = $1; }
    ;

FuncFParamsList
    : FuncFParam
        {
            ParamList list = {0};
            param_list_push(&list, $1);
            $$ = list;
        }
    | FuncFParamsList COMMA FuncFParam
        {
            $$ = $1;
            param_list_push(&$$, $3);
        }
    ;

FuncFParam
    : BType IDENFR FuncFParamSuffix
        { $$ = make_param($1, $2, $3.count > 0 || $3.capacity == -1, $3); }
    ;

FuncFParamSuffix
    :
        { IntList list = {0}; $$ = list; }
    | LBRACK RBRACK FuncFParamArrayDims
        { $$ = $3; $$.capacity = -1; }
    ;

FuncFParamArrayDims
    :
        { IntList list = {0}; $$ = list; }
    | FuncFParamArrayDims LBRACK Exp RBRACK
        {
            $$ = $1;
            int_list_push(&$$, eval_const_ast_expr($3));
        }
    ;

Block
    : BlockBegin BlockItemList RBRACE
        {
            parse_const_scope_pop();
            $$ = make_block($2);
        }
    ;

BlockBegin
    : LBRACE
        { parse_const_scope_push(); }
    ;

BlockItemList
    :
        { BlockItemList list = {0}; $$ = list; }
    | BlockItemList Decl
        {
            $$ = $1;
            block_item_list_push(&$$, BLOCK_ITEM_DECL, $2);
        }
    | BlockItemList Stmt
        {
            $$ = $1;
            block_item_list_push(&$$, BLOCK_ITEM_STMT, $2);
        }
    ;

Stmt
    : LVal ASSIGN Exp SEMICN
        { $$ = make_assign_stmt($1, $3); }
    | ExpOpt SEMICN
        { $$ = make_expr_stmt($1); }
    | Block
        { $$ = make_block_stmt($1); }
    | IFTK LPARENT Cond RPARENT Stmt %prec LOWER_THAN_ELSE
        { $$ = make_if_stmt($3, $5, NULL); }
    | IFTK LPARENT Cond RPARENT Stmt ELSETK Stmt
        { $$ = make_if_stmt($3, $5, $7); }
    | WHILETK LPARENT Cond RPARENT Stmt
        { $$ = make_while_stmt($3, $5); }
    | BREAKTK SEMICN
        { $$ = make_break_stmt(); }
    | CONTINUETK SEMICN
        { $$ = make_continue_stmt(); }
    | RETURNTK SEMICN
        { $$ = make_return_stmt(NULL); }
    | RETURNTK Exp SEMICN
        { $$ = make_return_stmt($2); }
    | PRINTFTK LPARENT STRCON RPARENT SEMICN
        {
            ExprList list = {0};
            $$ = make_printf_stmt($3, list);
        }
    | PRINTFTK LPARENT STRCON PrintfArgs RPARENT SEMICN
        { $$ = make_printf_stmt($3, $4); }
    ;

ExpOpt
    :
        { $$ = NULL; }
    | Exp
        { $$ = $1; }
    ;

PrintfArgs
    : COMMA Exp
        {
            ExprList list = {0};
            expr_list_push(&list, $2);
            $$ = list;
        }
    | PrintfArgs COMMA Exp
        {
            $$ = $1;
            expr_list_push(&$$, $3);
        }
    ;

Exp
    : AddExp { $$ = $1; }
    ;

Cond
    : LOrExp { $$ = $1; }
    ;

LVal
    : IDENFR LValIndices
        { $$ = make_lval($1, $2); }
    ;

LValIndices
    :
        { ExprList list = {0}; $$ = list; }
    | LValIndices LBRACK Exp RBRACK
        {
            $$ = $1;
            expr_list_push(&$$, $3);
        }
    ;

PrimaryExp
    : LPARENT Exp RPARENT { $$ = $2; }
    | LVal { $$ = make_lval_expr($1); }
    | Number { $$ = $1; }
    ;

Number
    : INTCON { $$ = make_number_expr(parse_int_literal($1)); }
    | FLOATCONTK { $$ = make_float_number_expr(parse_int_literal($1)); }
    ;

UnaryExp
    : PrimaryExp { $$ = $1; }
    | IDENFR LPARENT RPARENT
        {
            ExprList list = {0};
            $$ = make_call_expr($1, list);
        }
    | IDENFR LPARENT FuncRParams RPARENT
        { $$ = make_call_expr($1, $3); }
    | GETINTTK LPARENT RPARENT
        { $$ = make_getint_expr(); }
    | PLUS UnaryExp
        { $$ = make_unary_expr(UNARY_PLUS, $2); }
    | MINU UnaryExp
        { $$ = make_unary_expr(UNARY_MINUS, $2); }
    | NOT UnaryExp
        { $$ = make_unary_expr(UNARY_NOT, $2); }
    ;

FuncRParams
    : FuncRParamsList { $$ = $1; }
    ;

FuncRParamsList
    : Exp
        {
            ExprList list = {0};
            expr_list_push(&list, $1);
            $$ = list;
        }
    | FuncRParamsList COMMA Exp
        {
            $$ = $1;
            expr_list_push(&$$, $3);
        }
    ;

MulExp
    : UnaryExp { $$ = $1; }
    | MulExp MULT UnaryExp
        { $$ = make_binary_expr(BIN_MUL, $1, $3); }
    | MulExp DIV UnaryExp
        { $$ = make_binary_expr(BIN_DIV, $1, $3); }
    | MulExp MOD UnaryExp
        { $$ = make_binary_expr(BIN_MOD, $1, $3); }
    ;

AddExp
    : MulExp { $$ = $1; }
    | AddExp PLUS MulExp
        { $$ = make_binary_expr(BIN_ADD, $1, $3); }
    | AddExp MINU MulExp
        { $$ = make_binary_expr(BIN_SUB, $1, $3); }
    ;

RelExp
    : AddExp { $$ = $1; }
    | RelExp LSS AddExp
        { $$ = make_binary_expr(BIN_LT, $1, $3); }
    | RelExp GRE AddExp
        { $$ = make_binary_expr(BIN_GT, $1, $3); }
    | RelExp LEQ AddExp
        { $$ = make_binary_expr(BIN_LE, $1, $3); }
    | RelExp GEQ AddExp
        { $$ = make_binary_expr(BIN_GE, $1, $3); }
    ;

EqExp
    : RelExp { $$ = $1; }
    | EqExp EQL RelExp
        { $$ = make_binary_expr(BIN_EQ, $1, $3); }
    | EqExp NEQ RelExp
        { $$ = make_binary_expr(BIN_NE, $1, $3); }
    ;

LAndExp
    : EqExp { $$ = $1; }
    | LAndExp AND EqExp
        { $$ = make_binary_expr(BIN_AND, $1, $3); }
    ;

LOrExp
    : LAndExp { $$ = $1; }
    | LOrExp OR LAndExp
        { $$ = make_binary_expr(BIN_OR, $1, $3); }
    ;

ConstExp
    : AddExp { $$ = $1; }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "syntax error at line %d: %s\n", yylineno, s);
}
