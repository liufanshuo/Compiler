#ifndef SYSY_COMPILER_H
#define SYSY_COMPILER_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
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
    TypeSpec type;
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
    EXPR_FLOAT_NUMBER,
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
    TypeSpec type;
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

/* Forward declarations for mutually-referential IR structs. */
typedef struct IRType IRType;
typedef struct IRValue IRValue;
typedef struct IRUseList IRUseList;
typedef struct IRValueList IRValueList;
typedef struct IRInitializer IRInitializer;
typedef struct IRInitializerList IRInitializerList;
typedef struct IRGlobal IRGlobal;
typedef struct IRGlobalList IRGlobalList;
typedef struct IRParameter IRParameter;
typedef struct IRParameterList IRParameterList;
typedef struct IRBasicBlock IRBasicBlock;
typedef struct IRBasicBlockList IRBasicBlockList;
typedef struct IRInstruction IRInstruction;
typedef struct IRFunction IRFunction;
typedef struct IRFunctionList IRFunctionList;
typedef struct IRModule IRModule;

typedef enum {
    IR_TYPE_VOID,
    IR_TYPE_I1,
    IR_TYPE_I32,
    IR_TYPE_FLOAT,
    IR_TYPE_POINTER,
    IR_TYPE_ARRAY,
    IR_TYPE_FUNCTION
} IRTypeKind;

struct IRType {
    IRTypeKind kind;
    TypeSpec sysy_type;
    union {
        struct {
            IRType *pointee;
        } pointer;
        struct {
            int length;
            IRType *element;
        } array;
        struct {
            IRType *ret;
            IRType **params;
            int param_count;
            bool is_variadic;
        } function;
    } data;
};

typedef enum {
    IR_VALUE_NONE,
    IR_VALUE_CONST_INT,
    IR_VALUE_CONST_FLOAT,
    IR_VALUE_CONST_ZERO,
    IR_VALUE_LOCAL,
    IR_VALUE_GLOBAL,
    IR_VALUE_PARAM,
    IR_VALUE_FUNCTION,
    IR_VALUE_INSTRUCTION,
    IR_VALUE_BASIC_BLOCK
} IRValueKind;

struct IRValue {
    IRValueKind kind;
    IRType *type;
    TypeSpec base_type;
    int ptr_level;
    char *name;
    union {
        int int_value;
        int float_bits;
        IRGlobal *global;
        IRParameter *param;
        IRFunction *function;
        IRInstruction *instruction;
        IRBasicBlock *basic_block;
    } data;
};

struct IRUseList {
    IRValue **items;
    int count;
    int capacity;
};

struct IRValueList {
    IRValue **items;
    int count;
    int capacity;
};

typedef enum {
    IR_INIT_ZERO,
    IR_INIT_INT,
    IR_INIT_FLOAT,
    IR_INIT_ARRAY,
    IR_INIT_STRING
} IRInitializerKind;

struct IRInitializer {
    IRInitializerKind kind;
    IRType *type;
    union {
        int int_value;
        int float_bits;
        struct {
            IRInitializer **items;
            int count;
            int capacity;
        } array;
        struct {
            char *bytes;
            int length;
        } string;
    } data;
};

struct IRInitializerList {
    IRInitializer **items;
    int count;
    int capacity;
};

struct IRGlobal {
    char *name;
    IRType *type;
    TypeSpec elem_type;
    bool is_const;
    bool is_external;
    IRInitializer *initializer;
    IRGlobal *next;
};

struct IRGlobalList {
    IRGlobal **items;
    int count;
    int capacity;
};

struct IRParameter {
    char *name;
    IRType *type;
    TypeSpec sysy_type;
    bool is_array;
    IntList dims;
    IRValue value;
};

struct IRParameterList {
    IRParameter **items;
    int count;
    int capacity;
};

typedef enum {
    IR_INST_ALLOCA,
    IR_INST_LOAD,
    IR_INST_STORE,
    IR_INST_PHI,
    IR_INST_ADD,
    IR_INST_SUB,
    IR_INST_MUL,
    IR_INST_SDIV,
    IR_INST_SREM,
    IR_INST_FADD,
    IR_INST_FSUB,
    IR_INST_FMUL,
    IR_INST_FDIV,
    IR_INST_ICMP,
    IR_INST_FCMP,
    IR_INST_ZEXT,
    IR_INST_SITOFP,
    IR_INST_FPTOSI,
    IR_INST_BR,
    IR_INST_RET,
    IR_INST_CALL,
    IR_INST_GETELEMENTPTR,
    IR_INST_BITCAST
} IRInstructionKind;

typedef enum {
    IR_ICMP_EQ,
    IR_ICMP_NE,
    IR_ICMP_SLT,
    IR_ICMP_SLE,
    IR_ICMP_SGT,
    IR_ICMP_SGE
} IRIcmpPredicate;

typedef enum {
    IR_FCMP_OEQ,
    IR_FCMP_ONE,
    IR_FCMP_OLT,
    IR_FCMP_OLE,
    IR_FCMP_OGT,
    IR_FCMP_OGE
} IRFcmpPredicate;

struct IRInstruction {
    IRInstructionKind kind;
    IRType *result_type;
    IRValue result;
    IRBasicBlock *parent;
    IRInstruction *prev;
    IRInstruction *next;
    union {
        struct {
            IRType *allocated_type;
            int alignment;
        } alloca_inst;
        struct {
            IRValue *ptr;
            IRType *value_type;
            int alignment;
        } load_inst;
        struct {
            IRValue *value;
            IRValue *ptr;
            int alignment;
        } store_inst;
        struct {
            IRValueList values;
            IRBasicBlock **blocks;
            int count;
            int capacity;
            IRInstruction *alloca_inst;
        } phi_inst;
        struct {
            IRValue *lhs;
            IRValue *rhs;
        } binary_inst;
        struct {
            IRIcmpPredicate pred;
            IRValue *lhs;
            IRValue *rhs;
        } icmp_inst;
        struct {
            IRFcmpPredicate pred;
            IRValue *lhs;
            IRValue *rhs;
        } fcmp_inst;
        struct {
            IRValue *value;
            IRType *to_type;
        } cast_inst;
        struct {
            bool is_conditional;
            IRValue *condition;
            IRBasicBlock *true_block;
            IRBasicBlock *false_block;
        } br_inst;
        struct {
            IRValue *value;
        } ret_inst;
        struct {
            IRFunction *callee;
            IRType *ret_type;
            IRValueList args;
        } call_inst;
        struct {
            IRValue *base_ptr;
            IRType *source_element_type;
            IRValueList indices;
            bool inbounds;
        } gep_inst;
        struct {
            IRValue *value;
            IRType *to_type;
        } bitcast_inst;
    } data;
};

struct IRBasicBlock {
    char *name;
    IRFunction *parent;
    IRInstruction *first_inst;
    IRInstruction *last_inst;
    IRBasicBlock **preds;
    int pred_count;
    int pred_capacity;
    IRBasicBlock **succs;
    int succ_count;
    int succ_capacity;
    IRBasicBlock *next;
};

struct IRBasicBlockList {
    IRBasicBlock **items;
    int count;
    int capacity;
};

struct IRFunction {
    char *name;
    IRType *type;
    IRType *ret_type;
    TypeSpec sysy_ret_type;
    IRParameterList params;
    IRBasicBlockList blocks;
    IRBasicBlock *entry;
    bool is_external;
    IRFunction *next;
};

struct IRFunctionList {
    IRFunction **items;
    int count;
    int capacity;
};

struct IRModule {
    char *name;
    IRGlobalList globals;
    IRFunctionList functions;
    IRType *void_type;
    IRType *i1_type;
    IRType *i32_type;
    IRType *float_type;
};

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
void parse_const_scope_push(void);
void parse_const_scope_pop(void);

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
Expr *make_float_number_expr(int value);
Expr *make_lval_expr(LVal *lval);
Expr *make_call_expr(char *name, ExprList args);
Expr *make_getint_expr(void);
Expr *make_unary_expr(UnaryOp op, Expr *operand);
Expr *make_binary_expr(BinaryOp op, Expr *lhs, Expr *rhs);
InitVal *make_expr_init(Expr *expr);
InitVal *make_list_init(InitValList children);
DeclItem *make_decl_item(char *name, IntList dims, InitVal *init);
Decl *make_decl(TypeSpec type, bool is_const, DeclItemList items);
Param *make_param(TypeSpec type, char *name, bool is_array, IntList dims);
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

IRModule *ast_to_ir(Program *program);
void emit_riscv_from_ir(IRModule *module, FILE *out);
void generate_program_ir(Program *program, FILE *out);
void generate_program_mid_ir(Program *program, FILE *out);

#endif
