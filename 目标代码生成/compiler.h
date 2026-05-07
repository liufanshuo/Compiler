#ifndef SYSY_COMPILER_H
#define SYSY_COMPILER_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    TYPE_INT,
    TYPE_VOID
} TypeSpec;

typedef struct IntList {
    int *data;
    int count;
    int capacity;
} IntList;

typedef struct StringList {
    char **items;
    int count;
    int capacity;
} StringList;

struct Expr;
struct Stmt;
struct Block;
struct Decl;
struct InitVal;

typedef struct ExprList {
    struct Expr **items;
    int count;
    int capacity;
} ExprList;

typedef struct InitValList {
    struct InitVal **items;
    int count;
    int capacity;
} InitValList;

typedef struct StmtList {
    struct Stmt **items;
    int count;
    int capacity;
} StmtList;

typedef struct BlockItemList {
    void **items;
    int *kinds;
    int count;
    int capacity;
} BlockItemList;

typedef struct DeclItem {
    char *name;
    IntList dims;
    struct InitVal *init;
} DeclItem;

typedef struct DeclItemList {
    DeclItem **items;
    int count;
    int capacity;
} DeclItemList;

typedef struct Param {
    char *name;
    bool is_array;
    IntList dims;
} Param;

typedef struct ParamList {
    Param **items;
    int count;
    int capacity;
} ParamList;

typedef struct FuncDef FuncDef;

typedef struct TopLevelItem {
    int kind;
    union {
        struct Decl *decl;
        FuncDef *func;
    } data;
} TopLevelItem;

typedef struct TopLevelList {
    TopLevelItem **items;
    int count;
    int capacity;
} TopLevelList;

typedef enum {
    EXPR_NUMBER,
    EXPR_LVAL,
    EXPR_CALL,
    EXPR_GETINT,
    EXPR_UNARY,
    EXPR_BINARY
} ExprKind;

typedef enum {
    UNARY_PLUS,
    UNARY_MINUS,
    UNARY_NOT
} UnaryOp;

typedef enum {
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
    BIN_MOD,
    BIN_LT,
    BIN_GT,
    BIN_LE,
    BIN_GE,
    BIN_EQ,
    BIN_NE,
    BIN_AND,
    BIN_OR
} BinaryOp;

typedef struct LVal {
    char *name;
    ExprList indices;
} LVal;

typedef struct Expr {
    ExprKind kind;
    union {
        int number;
        LVal *lval;
        struct {
            char *name;
            ExprList args;
        } call;
        struct {
            UnaryOp op;
            struct Expr *operand;
        } unary;
        struct {
            BinaryOp op;
            struct Expr *lhs;
            struct Expr *rhs;
        } binary;
    } data;
} Expr;

typedef struct InitVal {
    bool is_expr;
    Expr *expr;
    InitValList children;
} InitVal;

typedef struct Decl {
    bool is_const;
    DeclItemList items;
} Decl;

typedef enum {
    STMT_ASSIGN,
    STMT_EXPR,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_PRINTF
} StmtKind;

typedef struct Stmt {
    StmtKind kind;
    union {
        struct {
            LVal *lval;
            Expr *expr;
        } assign_stmt;
        Expr *expr_stmt;
        struct Block *block_stmt;
        struct {
            Expr *cond;
            struct Stmt *then_stmt;
            struct Stmt *else_stmt;
        } if_stmt;
        struct {
            Expr *cond;
            struct Stmt *body;
        } while_stmt;
        Expr *return_expr;
        struct {
            char *format;
            ExprList args;
        } printf_stmt;
    } data;
} Stmt;

typedef struct Block {
    BlockItemList items;
} Block;

struct FuncDef {
    TypeSpec ret_type;
    char *name;
    ParamList params;
    Block *block;
};

typedef struct Program {
    TopLevelList items;
} Program;

enum {
    BLOCK_ITEM_DECL = 1,
    BLOCK_ITEM_STMT = 2,
    TOP_LEVEL_DECL = 1,
    TOP_LEVEL_FUNC = 2
};

extern Program *g_program;
extern FILE *yyin;

char *xstrdup(const char *s);
char *str_printf(const char *fmt, ...);
int parse_int_literal(const char *text);
int eval_const_ast_expr(Expr *expr);
void register_const_binding(const char *name, int value);

void int_list_push(IntList *list, int value);
void string_list_push(StringList *list, char *value);
void expr_list_push(ExprList *list, Expr *expr);
void init_list_push(InitValList *list, InitVal *init);
void stmt_list_push(StmtList *list, Stmt *stmt);
void decl_item_list_push(DeclItemList *list, DeclItem *item);
void param_list_push(ParamList *list, Param *param);
void top_level_list_push(TopLevelList *list, TopLevelItem *item);
void block_item_list_push(BlockItemList *list, int kind, void *item);

LVal *make_lval(char *name, ExprList indices);
Expr *make_number_expr(int value);
Expr *make_lval_expr(LVal *lval);
Expr *make_call_expr(char *name, ExprList args);
Expr *make_getint_expr(void);
Expr *make_unary_expr(UnaryOp op, Expr *operand);
Expr *make_binary_expr(BinaryOp op, Expr *lhs, Expr *rhs);
InitVal *make_expr_init(Expr *expr);
InitVal *make_list_init(InitValList children);
DeclItem *make_decl_item(char *name, IntList dims, InitVal *init);
Decl *make_decl(bool is_const, DeclItemList items);
Param *make_param(char *name, bool is_array, IntList dims);
Stmt *make_assign_stmt(LVal *lval, Expr *expr);
Stmt *make_expr_stmt(Expr *expr);
Stmt *make_block_stmt(Block *block);
Stmt *make_if_stmt(Expr *cond, Stmt *then_stmt, Stmt *else_stmt);
Stmt *make_while_stmt(Expr *cond, Stmt *body);
Stmt *make_break_stmt(void);
Stmt *make_continue_stmt(void);
Stmt *make_return_stmt(Expr *expr);
Stmt *make_printf_stmt(char *format, ExprList args);
Block *make_block(BlockItemList items);
FuncDef *make_func(TypeSpec ret_type, char *name, ParamList params, Block *block);
TopLevelItem *make_top_decl(Decl *decl);
TopLevelItem *make_top_func(FuncDef *func);
Program *make_program(TopLevelList items);

void generate_program_ir(Program *program, FILE *out);

#endif
