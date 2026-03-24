%{
#define YYDEBUG 1

#include <cstdio>
#include <cstdlib>
#include <string>
#include <memory>
#include "ast.h"

// 声明全局根变量
std::unique_ptr<BaseAST> root;

int yylex();
void yyerror(const char *s);
extern char* yytext;

%}

// 定义语义值的可能类型
%union {
    BaseAST* ast_ptr;  // 用于AST节点指针
    int int_val;       // 用于整数值
    char* str_val;     // 用于字符串和标识符名称
}

// 定义token
%token MAIN CONST INT GETINT PRINTF RETURN VOID
%token <str_val> IDENT STR_CONST
%token <int_val> INT_CONST

// 定义非终结符的类型
%type <ast_ptr> CompUnit
%type <ast_ptr> Decl ConstDecl VarDecl
%type <ast_ptr> BType
%type <ast_ptr> MainFuncDef
%type <ast_ptr> Block BlockItems BlockItem
%type <ast_ptr> Stmt Assignment Number

%start CompUnit

%%

// 语法开始符号
CompUnit
    : Decl MainFuncDef {
        auto ast = std::make_unique<CompUnitAST>();
        ast->decl.reset(static_cast<BaseAST*>($1));
        ast->main_func_def.reset(static_cast<BaseAST*>($2));
        root = std::move(ast);  // 设置全局根节点
    }
    ;

Decl : 
    ConstDecl ';' {
        auto ast = std::make_unique<DeclAST>();
        ast->decl.reset(static_cast<BaseAST*>($1));
        ast->kind = DeclAST::DeclKind::CONST_DECL;
        $$ = ast.release();
    } | VarDecl ';' {
        auto ast = std::make_unique<DeclAST>();
        ast->decl.reset(static_cast<BaseAST*>($1));
        ast->kind = DeclAST::DeclKind::VAR_DECL;
        $$ = ast.release();
    }
    ;

// ConstDecl -> 'const' xxx
ConstDecl :
    CONST BType IDENT '=' Number {
        auto ast = std::make_unique<ConstDeclAST>();
        ast->type = TYPE_INT;  // 假设默认为INT类型
        
        // 使用$3而不是yytext
        ast->ident = std::string($3);  
        
        // 获取Number节点的值
        auto number = dynamic_cast<NumberAST*>($5);
        if (number) {
            ast->value = number->value;
        }
        
        // 释放字符串
        free($3);
        
        $$ = ast.release();
    }
    ;

// BType -> 'int'
BType : 
    INT {
        $$ = nullptr;  // BType不需要生成AST节点
    } |
    VOID {
        $$ = nullptr;  // BType不需要生成AST节点
    }
    ;

// VarDecl 
VarDecl :
    BType IDENT {
        auto ast = std::make_unique<VarDeclAST>();
        ast->type = TYPE_INT;
        ast->ident = std::string($2);
        
        // 释放字符串
        free($2);
        
        $$ = ast.release();
    }
    ;

MainFuncDef :
    INT MAIN '(' ')' Block {
        auto ast = std::make_unique<MainFuncDefAST>();
        ast->block.reset(static_cast<BaseAST*>($5));
        $$ = ast.release();
    }
    ;

Block :
    '{' BlockItems '}' {
        $$ = $2;  // 直接使用BlockItems生成的BlockAST
    } | '{' '}' {
        auto ast = std::make_unique<BlockAST>();  // 创建空块
        $$ = ast.release();
    }
    ;

BlockItems :
    BlockItems BlockItem {
        auto block = dynamic_cast<BlockAST*>($1);
        if (block && $2) {
            block->items.emplace_back(static_cast<BaseAST*>($2));
        }
        $$ = block;
    } | BlockItem {
        auto ast = std::make_unique<BlockAST>();
        if ($1) {
            ast->items.emplace_back(static_cast<BaseAST*>($1));
        }
        $$ = ast.release();
    }
    ;

BlockItem :
    Decl {
        $$ = $1;
    } | Stmt {
        $$ = $1;
    }
    ;

Stmt :
    Assignment ';' {
        auto ast = std::make_unique<StmtAST>();
        ast->kind = StmtAST::ASSIGNMENT;
        ast->content.reset(static_cast<BaseAST*>($1));
        $$ = ast.release();
    }
    | Block {
        auto ast = std::make_unique<StmtAST>();
        ast->kind = StmtAST::BLOCK;
        ast->content.reset(static_cast<BaseAST*>($1));
        $$ = ast.release();
    }
    | RETURN IDENT ';' {
        auto ast = std::make_unique<StmtAST>();
        ast->kind = StmtAST::RETURN;
        ast->return_ident = std::string($2);
        
        // 释放字符串
        free($2);
        
        $$ = ast.release();
    }
    ;

Assignment :
    IDENT '=' GETINT '(' ')' {
        auto ast = std::make_unique<AssignmentAST>();
        ast->ident = std::string($1);
        
        // 释放字符串
        free($1);
        
        $$ = ast.release();
    }
    ;

Number :
    INT_CONST {
        auto ast = std::make_unique<NumberAST>();
        ast->value = $1;
        $$ = ast.release();
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s\n", s);
}