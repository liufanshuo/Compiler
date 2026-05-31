#define _POSIX_C_SOURCE 200809L

#include "compiler.h"

#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

Program *g_program = NULL;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

typedef struct ParseConstBinding {
    char *name;
    int value;
    struct ParseConstBinding *next;
    struct ParseConstBinding *hash_next;
} ParseConstBinding;

typedef struct ParseConstScope {
    ParseConstBinding *mark;
    struct ParseConstScope *next;
} ParseConstScope;

static ParseConstBinding *g_parse_consts = NULL;
#define PARSE_CONST_BUCKETS 4096
static ParseConstBinding *g_parse_const_buckets[PARSE_CONST_BUCKETS];
static ParseConstScope *g_parse_const_scopes = NULL;

typedef struct Symbol Symbol;
typedef struct FunctionSymbol FunctionSymbol;

typedef struct {
    int dim_count;
    int *dims;
    int total_slots;
    bool is_const;
    bool is_param_array;
    bool is_flat_storage;
} VarInfo;

struct Symbol {
    char *name;
    VarInfo info;
    bool is_global;
    bool is_function;
    TypeSpec value_type;
    bool is_const_scalar;
    int const_scalar;
    int *const_flat;
    char *llvm_name;
    char *flat_type;
    int stack_offset;
    Symbol *next;
    Symbol *hash_next;
};

struct FunctionSymbol {
    char *name;
    char *llvm_name;
    TypeSpec ret_type;
    ParamList params;
    FuncDef *func;
    FunctionSymbol *next;
    FunctionSymbol *hash_next;
};

static FunctionSymbol *g_function_buckets[512];

typedef struct Scope {
    Symbol *symbols;
    Symbol **buckets;
    struct Scope *next;
} Scope;

typedef struct {
    FILE *out;
    StrBuf globals;
    StrBuf functions;
    StrBuf *current_allocas;
    StrBuf *current_body;
    Scope *scopes;
    FunctionSymbol *functions_meta;
    int global_id;
    int function_id;
    int temp_id;
    int label_id;
    TypeSpec current_ret_type;
    bool current_block_terminated;
    StringList break_labels;
    StringList continue_labels;
} IRGen;

typedef struct {
    char *value;
    TypeSpec type;
    int dim_count;
    int *dims;
    bool is_pointer_value;
    bool is_param_array;
} Value;

typedef struct {
    char *ptr;
    TypeSpec type;
    int dim_count;
    int *dims;
    bool is_param_array;
} Address;

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
    void *res = realloc(ptr, size);
    if (res == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return res;
}

static unsigned hash_string(const char *s) {
    unsigned hash = 2166136261u;
    while (*s != '\0') {
        hash ^= (unsigned char)*s++;
        hash *= 16777619u;
    }
    return hash;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = (char *)xmalloc(n);
    memcpy(copy, s, n);
    return copy;
}

char *str_printf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *buf = (char *)xmalloc((size_t)n + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void register_const_binding(const char *name, int value) {
    ParseConstBinding *node = (ParseConstBinding *)xmalloc(sizeof(ParseConstBinding));
    node->name = xstrdup(name);
    node->value = value;
    node->next = g_parse_consts;
    g_parse_consts = node;
    unsigned idx = hash_string(name) % PARSE_CONST_BUCKETS;
    node->hash_next = g_parse_const_buckets[idx];
    g_parse_const_buckets[idx] = node;
}

void parse_const_scope_push(void) {
    ParseConstScope *scope = (ParseConstScope *)xmalloc(sizeof(ParseConstScope));
    scope->mark = g_parse_consts;
    scope->next = g_parse_const_scopes;
    g_parse_const_scopes = scope;
}

void parse_const_scope_pop(void) {
    if (g_parse_const_scopes == NULL) {
        return;
    }
    ParseConstBinding *mark = g_parse_const_scopes->mark;
    while (g_parse_consts != mark) {
        ParseConstBinding *node = g_parse_consts;
        unsigned idx = hash_string(node->name) % PARSE_CONST_BUCKETS;
        ParseConstBinding **link = &g_parse_const_buckets[idx];
        while (*link != NULL && *link != node) {
            link = &(*link)->hash_next;
        }
        if (*link == node) {
            *link = node->hash_next;
        }
        g_parse_consts = node->next;
    }
    g_parse_const_scopes = g_parse_const_scopes->next;
}

static bool lookup_parse_const(const char *name, int *value) {
    unsigned idx = hash_string(name) % PARSE_CONST_BUCKETS;
    for (ParseConstBinding *node = g_parse_const_buckets[idx]; node != NULL; node = node->hash_next) {
        if (strcmp(node->name, name) == 0) {
            *value = node->value;
            return true;
        }
    }
    return false;
}

static void ensure_capacity(void **data, int *capacity, size_t elem_size, int count) {
    if (*capacity >= count) {
        return;
    }
    int next = (*capacity == 0) ? 4 : *capacity * 2;
    while (next < count) {
        next *= 2;
    }
    *data = xrealloc(*data, elem_size * (size_t)next);
    *capacity = next;
}

void int_list_push(IntList *list, int value) {
    ensure_capacity((void **)&list->data, &list->capacity, sizeof(int), list->count + 1);
    list->data[list->count++] = value;
}

void string_list_push(StringList *list, char *value) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(char *), list->count + 1);
    list->items[list->count++] = value;
}

void expr_list_push(ExprList *list, Expr *expr) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(Expr *), list->count + 1);
    list->items[list->count++] = expr;
}

void init_list_push(InitValList *list, InitVal *init) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(InitVal *), list->count + 1);
    list->items[list->count++] = init;
}

void stmt_list_push(StmtList *list, Stmt *stmt) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(Stmt *), list->count + 1);
    list->items[list->count++] = stmt;
}

void decl_item_list_push(DeclItemList *list, DeclItem *item) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(DeclItem *), list->count + 1);
    list->items[list->count++] = item;
}

void param_list_push(ParamList *list, Param *param) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(Param *), list->count + 1);
    list->items[list->count++] = param;
}

void top_level_list_push(TopLevelList *list, TopLevelItem *item) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(TopLevelItem *), list->count + 1);
    list->items[list->count++] = item;
}

void block_item_list_push(BlockItemList *list, int kind, void *item) {
    if (list->capacity < list->count + 1) {
        int next = (list->capacity == 0) ? 4 : list->capacity * 2;
        while (next < list->count + 1) {
            next *= 2;
        }
        list->items = (void **)xrealloc(list->items, sizeof(void *) * (size_t)next);
        list->kinds = (int *)xrealloc(list->kinds, sizeof(int) * (size_t)next);
        list->capacity = next;
    }
    list->items[list->count] = item;
    list->kinds[list->count] = kind;
    list->count++;
}

static Expr *alloc_expr(ExprKind kind) {
    Expr *expr = (Expr *)xmalloc(sizeof(Expr));
    memset(expr, 0, sizeof(Expr));
    expr->kind = kind;
    return expr;
}

LVal *make_lval(char *name, ExprList indices) {
    LVal *lval = (LVal *)xmalloc(sizeof(LVal));
    lval->name = name;
    lval->indices = indices;
    return lval;
}

Expr *make_number_expr(int value) {
    Expr *expr = alloc_expr(EXPR_NUMBER);
    expr->data.number = value;
    return expr;
}

Expr *make_float_number_expr(int value) {
    Expr *expr = alloc_expr(EXPR_FLOAT_NUMBER);
    expr->data.number = value;
    return expr;
}

Expr *make_lval_expr(LVal *lval) {
    Expr *expr = alloc_expr(EXPR_LVAL);
    expr->data.lval = lval;
    return expr;
}

Expr *make_call_expr(char *name, ExprList args) {
    Expr *expr = alloc_expr(EXPR_CALL);
    expr->data.call.name = name;
    expr->data.call.args = args;
    return expr;
}

Expr *make_getint_expr(void) {
    return alloc_expr(EXPR_GETINT);
}

Expr *make_unary_expr(UnaryOp op, Expr *operand) {
    Expr *expr = alloc_expr(EXPR_UNARY);
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    return expr;
}

Expr *make_binary_expr(BinaryOp op, Expr *lhs, Expr *rhs) {
    Expr *expr = alloc_expr(EXPR_BINARY);
    expr->data.binary.op = op;
    expr->data.binary.lhs = lhs;
    expr->data.binary.rhs = rhs;
    return expr;
}

InitVal *make_expr_init(Expr *expr) {
    InitVal *init = (InitVal *)xmalloc(sizeof(InitVal));
    memset(init, 0, sizeof(InitVal));
    init->is_expr = true;
    init->expr = expr;
    return init;
}

InitVal *make_list_init(InitValList children) {
    InitVal *init = (InitVal *)xmalloc(sizeof(InitVal));
    memset(init, 0, sizeof(InitVal));
    init->is_expr = false;
    init->children = children;
    return init;
}

DeclItem *make_decl_item(char *name, IntList dims, InitVal *init) {
    DeclItem *item = (DeclItem *)xmalloc(sizeof(DeclItem));
    item->name = name;
    item->dims = dims;
    item->init = init;
    return item;
}

Decl *make_decl(TypeSpec type, bool is_const, DeclItemList items) {
    Decl *decl = (Decl *)xmalloc(sizeof(Decl));
    decl->type = type;
    decl->is_const = is_const;
    decl->items = items;
    return decl;
}

Param *make_param(TypeSpec type, char *name, bool is_array, IntList dims) {
    Param *param = (Param *)xmalloc(sizeof(Param));
    param->type = type;
    param->name = name;
    param->is_array = is_array;
    param->dims = dims;
    return param;
}

Stmt *make_assign_stmt(LVal *lval, Expr *expr) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_ASSIGN;
    stmt->data.assign_stmt.lval = lval;
    stmt->data.assign_stmt.expr = expr;
    return stmt;
}

Stmt *make_expr_stmt(Expr *expr) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_EXPR;
    stmt->data.expr_stmt = expr;
    return stmt;
}

Stmt *make_block_stmt(Block *block) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_BLOCK;
    stmt->data.block_stmt = block;
    return stmt;
}

Stmt *make_if_stmt(Expr *cond, Stmt *then_stmt, Stmt *else_stmt) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_IF;
    stmt->data.if_stmt.cond = cond;
    stmt->data.if_stmt.then_stmt = then_stmt;
    stmt->data.if_stmt.else_stmt = else_stmt;
    return stmt;
}

Stmt *make_while_stmt(Expr *cond, Stmt *body) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_WHILE;
    stmt->data.while_stmt.cond = cond;
    stmt->data.while_stmt.body = body;
    return stmt;
}

Stmt *make_break_stmt(void) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_BREAK;
    return stmt;
}

Stmt *make_continue_stmt(void) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_CONTINUE;
    return stmt;
}

Stmt *make_return_stmt(Expr *expr) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_RETURN;
    stmt->data.return_expr = expr;
    return stmt;
}

Stmt *make_printf_stmt(char *format, ExprList args) {
    Stmt *stmt = (Stmt *)xmalloc(sizeof(Stmt));
    memset(stmt, 0, sizeof(Stmt));
    stmt->kind = STMT_PRINTF;
    stmt->data.printf_stmt.format = format;
    stmt->data.printf_stmt.args = args;
    return stmt;
}

Block *make_block(BlockItemList items) {
    Block *block = (Block *)xmalloc(sizeof(Block));
    block->items = items;
    return block;
}

FuncDef *make_func(TypeSpec ret_type, char *name, ParamList params, Block *block) {
    FuncDef *func = (FuncDef *)xmalloc(sizeof(FuncDef));
    func->ret_type = ret_type;
    func->name = name;
    func->params = params;
    func->block = block;
    return func;
}

TopLevelItem *make_top_decl(Decl *decl) {
    TopLevelItem *item = (TopLevelItem *)xmalloc(sizeof(TopLevelItem));
    item->kind = TOP_LEVEL_DECL;
    item->data.decl = decl;
    return item;
}

TopLevelItem *make_top_func(FuncDef *func) {
    TopLevelItem *item = (TopLevelItem *)xmalloc(sizeof(TopLevelItem));
    item->kind = TOP_LEVEL_FUNC;
    item->data.func = func;
    return item;
}

Program *make_program(TopLevelList items) {
    Program *program = (Program *)xmalloc(sizeof(Program));
    program->items = items;
    return program;
}

int parse_int_literal(const char *text) {
    int base = 10;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    } else if (text[0] == '0' && text[1] != '\0') {
        base = 8;
    }
    return (int)strtol(text, NULL, base);
}

static void sb_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_reserve(StrBuf *sb, size_t need) {
    if (sb->cap >= need) {
        return;
    }
    size_t next = sb->cap == 0 ? 256 : sb->cap * 2;
    while (next < need) {
        next *= 2;
    }
    sb->data = (char *)xrealloc(sb->data, next);
    sb->cap = next;
}

static void sb_append(StrBuf *sb, const char *text) {
    size_t n = strlen(text);
    sb_reserve(sb, sb->len + n + 1);
    memcpy(sb->data + sb->len, text, n + 1);
    sb->len += n;
}

static void sb_vappendf(StrBuf *sb, const char *fmt, va_list ap) {
    char stack_buf[256];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    if (n < 0) {
        va_end(ap2);
        return;
    }
    if ((size_t)n < sizeof(stack_buf)) {
        sb_reserve(sb, sb->len + (size_t)n + 1);
        memcpy(sb->data + sb->len, stack_buf, (size_t)n + 1);
        sb->len += (size_t)n;
        va_end(ap2);
        return;
    }
    sb_reserve(sb, sb->len + (size_t)n + 1);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap2);
    sb->len += (size_t)n;
    va_end(ap2);
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vappendf(sb, fmt, ap);
    va_end(ap);
}

static int product_dims(const int *dims, int start, int count) {
    int result = 1;
    for (int i = start; i < count; ++i) {
        result *= dims[i];
    }
    return result;
}

static int *copy_dims(const int *dims, int count) {
    if (count == 0) {
        return NULL;
    }
    int *copy = (int *)xmalloc(sizeof(int) * (size_t)count);
    memcpy(copy, dims, sizeof(int) * (size_t)count);
    return copy;
}

static const char *llvm_scalar_type(TypeSpec type) {
    switch (type) {
        case TYPE_INT:
            return "i32";
        case TYPE_FLOAT:
            return "float";
        case TYPE_VOID:
            return "void";
    }
    return "i32";
}

static char *llvm_type_from_dims_typed(TypeSpec type, const int *dims, int count) {
    if (count == 0) {
        return xstrdup(llvm_scalar_type(type));
    }
    char *sub = llvm_type_from_dims_typed(type, dims + 1, count - 1);
    char *res = str_printf("[%d x %s]", dims[0], sub);
    free(sub);
    return res;
}

static char *llvm_flat_array_type_typed(TypeSpec type, int total) {
    return str_printf("[%d x %s]", total, llvm_scalar_type(type));
}

static char *llvm_subarray_ptr_type_typed(TypeSpec type, const int *dims, int dim_count) {
    if (dim_count <= 1) {
        return str_printf("%s*", llvm_scalar_type(type));
    }
    char *sub = llvm_type_from_dims_typed(type, dims + 1, dim_count - 1);
    char *res = str_printf("%s*", sub);
    free(sub);
    return res;
}

static char *llvm_param_type(const Param *param) {
    if (!param->is_array) {
        return xstrdup(llvm_scalar_type(param->type));
    }
    char *sub = llvm_type_from_dims_typed(param->type, param->dims.data, param->dims.count);
    char *res = str_printf("%s*", sub);
    free(sub);
    return res;
}

static void push_scope(IRGen *gen) {
    Scope *scope = (Scope *)xmalloc(sizeof(Scope));
    scope->symbols = NULL;
    scope->buckets = (Symbol **)calloc(1024, sizeof(Symbol *));
    if (scope->buckets == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    scope->next = gen->scopes;
    gen->scopes = scope;
}

static void pop_scope(IRGen *gen) {
    Scope *scope = gen->scopes;
    gen->scopes = scope->next;
}

static Symbol *scope_add_symbol(IRGen *gen, const char *name) {
    Symbol *sym = (Symbol *)xmalloc(sizeof(Symbol));
    memset(sym, 0, sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->next = gen->scopes->symbols;
    gen->scopes->symbols = sym;
    unsigned idx = hash_string(name) % 1024;
    sym->hash_next = gen->scopes->buckets[idx];
    gen->scopes->buckets[idx] = sym;
    return sym;
}

static Symbol *lookup_symbol(IRGen *gen, const char *name) {
    unsigned idx = hash_string(name) % 1024;
    for (Scope *scope = gen->scopes; scope != NULL; scope = scope->next) {
        for (Symbol *sym = scope->buckets[idx]; sym != NULL; sym = sym->hash_next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

static FunctionSymbol *add_function_meta(IRGen *gen, FuncDef *func) {
    FunctionSymbol *meta = (FunctionSymbol *)xmalloc(sizeof(FunctionSymbol));
    meta->name = xstrdup(func->name);
    meta->llvm_name = strcmp(func->name, "main") == 0 ? xstrdup("main") : str_printf("f%d", gen->function_id++);
    meta->ret_type = func->ret_type;
    meta->params = func->params;
    meta->func = func;
    meta->next = gen->functions_meta;
    gen->functions_meta = meta;
    unsigned idx = hash_string(func->name) % 512;
    meta->hash_next = g_function_buckets[idx];
    g_function_buckets[idx] = meta;
    return meta;
}

static FunctionSymbol *lookup_function_meta(IRGen *gen, const char *name) {
    (void)gen;
    unsigned idx = hash_string(name) % 512;
    for (FunctionSymbol *meta = g_function_buckets[idx]; meta != NULL; meta = meta->hash_next) {
        if (strcmp(meta->name, name) == 0) {
            return meta;
        }
    }
    return NULL;
}

static const char *function_llvm_name(IRGen *gen, const char *name) {
    FunctionSymbol *meta = lookup_function_meta(gen, name);
    if (meta != NULL) {
        return meta->llvm_name;
    }
    return name;
}

static char *new_temp(IRGen *gen) {
    return str_printf("%%t%d", gen->temp_id++);
}

static char *new_label(IRGen *gen, const char *prefix) {
    return str_printf("%s%d", prefix, gen->label_id++);
}

static void emit_func(IRGen *gen, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (gen->current_body != NULL) {
        sb_vappendf(gen->current_body, fmt, ap);
    } else {
        sb_vappendf(&gen->functions, fmt, ap);
    }
    va_end(ap);
}

static void emit_global(IRGen *gen, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vappendf(&gen->globals, fmt, ap);
    va_end(ap);
}

static void emit_label(IRGen *gen, const char *label) {
    emit_func(gen, "%s:\n", label);
    gen->current_block_terminated = false;
}

static char *emit_alloca(IRGen *gen, const char *type) {
    char *tmp = new_temp(gen);
    if (gen->current_allocas != NULL) {
        sb_appendf(gen->current_allocas, "  %s = alloca %s\n", tmp, type);
    } else {
        emit_func(gen, "  %s = alloca %s\n", tmp, type);
    }
    return tmp;
}

static int eval_const_expr(IRGen *gen, Expr *expr);
Value gen_expr(IRGen *gen, Expr *expr);
void gen_cond(IRGen *gen, Expr *expr, const char *true_label, const char *false_label);

int eval_const_ast_expr(Expr *expr) {
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_FLOAT_NUMBER:
            return expr->data.number;
        case EXPR_LVAL:
            if (expr->data.lval->indices.count == 0) {
                int value = 0;
                if (lookup_parse_const(expr->data.lval->name, &value)) {
                    return value;
                }
            }
            return 0;
        case EXPR_UNARY: {
            int v = eval_const_ast_expr(expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                return v;
            }
            if (expr->data.unary.op == UNARY_MINUS) {
                return -v;
            }
            return !v;
        }
        case EXPR_BINARY: {
            int lhs = eval_const_ast_expr(expr->data.binary.lhs);
            int rhs = eval_const_ast_expr(expr->data.binary.rhs);
            switch (expr->data.binary.op) {
                case BIN_ADD: return lhs + rhs;
                case BIN_SUB: return lhs - rhs;
                case BIN_MUL: return lhs * rhs;
                case BIN_DIV: return lhs / rhs;
                case BIN_MOD: return lhs % rhs;
                case BIN_LT: return lhs < rhs;
                case BIN_GT: return lhs > rhs;
                case BIN_LE: return lhs <= rhs;
                case BIN_GE: return lhs >= rhs;
                case BIN_EQ: return lhs == rhs;
                case BIN_NE: return lhs != rhs;
                case BIN_AND: return lhs && rhs;
                case BIN_OR: return lhs || rhs;
            }
            return 0;
        }
        default:
            return 0;
    }
}

static int eval_const_lval(IRGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol(gen, lval->name);
    if (sym->info.dim_count == 0) {
        return sym->const_scalar;
    }
    int index = 0;
    for (int i = 0; i < lval->indices.count; ++i) {
        int idx = eval_const_expr(gen, lval->indices.items[i]);
        int stride = product_dims(sym->info.dims, i + 1, sym->info.dim_count);
        index += idx * stride;
    }
    return sym->const_flat[index];
}

static int eval_const_float_bits(IRGen *gen, Expr *expr);
static float host_float_from_bits(int bits);
static bool eval_const_truth(IRGen *gen, Expr *expr);

static TypeSpec eval_const_expr_type(IRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return TYPE_INT;
    }
    switch (expr->kind) {
        case EXPR_FLOAT_NUMBER:
            return TYPE_FLOAT;
        case EXPR_LVAL: {
            Symbol *sym = lookup_symbol(gen, expr->data.lval->name);
            return sym != NULL ? sym->value_type : TYPE_INT;
        }
        case EXPR_UNARY:
            return expr->data.unary.op == UNARY_NOT ? TYPE_INT
                                                    : eval_const_expr_type(gen, expr->data.unary.operand);
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_LT || op == BIN_GT || op == BIN_LE || op == BIN_GE ||
                op == BIN_EQ || op == BIN_NE || op == BIN_AND || op == BIN_OR) {
                return TYPE_INT;
            }
            return (eval_const_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT ||
                    eval_const_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
        }
        default:
            return TYPE_INT;
    }
}

static int eval_const_expr(IRGen *gen, Expr *expr) {
    if (eval_const_expr_type(gen, expr) == TYPE_FLOAT) {
        return (int)host_float_from_bits(eval_const_float_bits(gen, expr));
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return expr->data.number;
        case EXPR_LVAL:
            return eval_const_lval(gen, expr->data.lval);
        case EXPR_UNARY: {
            if (expr->data.unary.op == UNARY_NOT) {
                return !eval_const_truth(gen, expr->data.unary.operand);
            }
            int v = eval_const_expr(gen, expr->data.unary.operand);
            switch (expr->data.unary.op) {
                case UNARY_PLUS: return v;
                case UNARY_MINUS: return -v;
                case UNARY_NOT: return !v;
            }
            return 0;
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                bool lhs = eval_const_truth(gen, expr->data.binary.lhs);
                if (op == BIN_AND) {
                    return lhs && eval_const_truth(gen, expr->data.binary.rhs);
                }
                return lhs || eval_const_truth(gen, expr->data.binary.rhs);
            }
            bool has_float = eval_const_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT ||
                             eval_const_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT;
            if (has_float && op != BIN_MOD) {
                float lhs = eval_const_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT
                                ? host_float_from_bits(eval_const_float_bits(gen, expr->data.binary.lhs))
                                : (float)eval_const_expr(gen, expr->data.binary.lhs);
                float rhs = eval_const_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT
                                ? host_float_from_bits(eval_const_float_bits(gen, expr->data.binary.rhs))
                                : (float)eval_const_expr(gen, expr->data.binary.rhs);
                switch (op) {
                    case BIN_ADD: return (int)(lhs + rhs);
                    case BIN_SUB: return (int)(lhs - rhs);
                    case BIN_MUL: return (int)(lhs * rhs);
                    case BIN_DIV: return (int)(lhs / rhs);
                    case BIN_LT: return lhs < rhs;
                    case BIN_GT: return lhs > rhs;
                    case BIN_LE: return lhs <= rhs;
                    case BIN_GE: return lhs >= rhs;
                    case BIN_EQ: return lhs == rhs;
                    case BIN_NE: return lhs != rhs;
                    default: return 0;
                }
            }
            int lhs = eval_const_expr(gen, expr->data.binary.lhs);
            int rhs = eval_const_expr(gen, expr->data.binary.rhs);
            switch (op) {
                case BIN_ADD: return lhs + rhs;
                case BIN_SUB: return lhs - rhs;
                case BIN_MUL: return lhs * rhs;
                case BIN_DIV: return lhs / rhs;
                case BIN_MOD: return lhs % rhs;
                case BIN_LT: return lhs < rhs;
                case BIN_GT: return lhs > rhs;
                case BIN_LE: return lhs <= rhs;
                case BIN_GE: return lhs >= rhs;
                case BIN_EQ: return lhs == rhs;
                case BIN_NE: return lhs != rhs;
                case BIN_AND: return lhs && rhs;
                case BIN_OR: return lhs || rhs;
            }
            return 0;
        }
        default:
            return 0;
    }
}

static bool eval_const_truth(IRGen *gen, Expr *expr) {
    if (eval_const_expr_type(gen, expr) == TYPE_FLOAT) {
        float value = host_float_from_bits(eval_const_float_bits(gen, expr));
        return value != 0.0f;
    }
    return eval_const_expr(gen, expr) != 0;
}

static int object_slot_count(const int *dims, int dim_count) {
    if (dim_count <= 0) {
        return 1;
    }
    return product_dims(dims, 0, dim_count);
}

static int subobject_slot_count(const int *dims, int dim_count) {
    if (dim_count <= 1) {
        return 1;
    }
    return product_dims(dims, 1, dim_count);
}

static void expand_init_to_slots(InitVal *init, const int *dims, int dim_count,
                                 Expr **slots, int total, int base, int limit, int *cursor) {
    if (init == NULL || *cursor >= limit) {
        return;
    }
    if (init->is_expr) {
        if (*cursor < total) {
            slots[*cursor] = init->expr;
        }
        (*cursor)++;
        return;
    }
    if (dim_count == 0) {
        for (int i = 0; i < init->children.count && *cursor < limit; ++i) {
            expand_init_to_slots(init->children.items[i], dims, 0, slots, total, base, limit, cursor);
        }
        return;
    }
    if (dim_count == 1) {
        for (int i = 0; i < init->children.count && *cursor < limit; ++i) {
            expand_init_to_slots(init->children.items[i], dims + 1, 0, slots, total, base, limit, cursor);
        }
        return;
    }

    int block = subobject_slot_count(dims, dim_count);
    for (int i = 0; i < init->children.count && *cursor < limit; ++i) {
        InitVal *child = init->children.items[i];
        if (child->is_expr) {
            expand_init_to_slots(child, dims + 1, dim_count - 1, slots, total, base, limit, cursor);
            continue;
        }
        int rel = *cursor - base;
        if (rel % block != 0) {
            *cursor += block - (rel % block);
        }
        if (*cursor >= limit) {
            break;
        }
        int child_base = *cursor;
        int child_limit = child_base + block;
        expand_init_to_slots(child, dims + 1, dim_count - 1, slots, total, child_base, child_limit, cursor);
        if (*cursor < child_limit) {
            *cursor = child_limit;
        }
    }
}

static Expr **init_to_expr_slots(InitVal *init, const int *dims, int dim_count, int total) {
    Expr **slots = (Expr **)calloc((size_t)total, sizeof(Expr *));
    if (slots == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    int cursor = 0;
    expand_init_to_slots(init, dims, dim_count, slots, total, 0, total, &cursor);
    return slots;
}

static int float_bits_from_host(float value) {
    int bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float host_float_from_bits(int bits) {
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int eval_const_float_bits(IRGen *gen, Expr *expr) {
    switch (expr->kind) {
        case EXPR_FLOAT_NUMBER:
            return expr->data.number;
        case EXPR_NUMBER:
            return float_bits_from_host((float)expr->data.number);
        case EXPR_LVAL:
        {
            Symbol *sym = lookup_symbol(gen, expr->data.lval->name);
            int value = eval_const_lval(gen, expr->data.lval);
            return (sym != NULL && sym->value_type == TYPE_FLOAT)
                       ? value
                       : float_bits_from_host((float)value);
        }
        case EXPR_UNARY: {
            int bits = eval_const_float_bits(gen, expr->data.unary.operand);
            float v = host_float_from_bits(bits);
            if (expr->data.unary.op == UNARY_MINUS) {
                return float_bits_from_host(-v);
            }
            if (expr->data.unary.op == UNARY_NOT) {
                return float_bits_from_host(!v);
            }
            return bits;
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                bool lhs = eval_const_truth(gen, expr->data.binary.lhs);
                bool rhs = eval_const_truth(gen, expr->data.binary.rhs);
                return float_bits_from_host(op == BIN_AND ? (lhs && rhs) : (lhs || rhs));
            }
            float lhs = host_float_from_bits(eval_const_float_bits(gen, expr->data.binary.lhs));
            float rhs = host_float_from_bits(eval_const_float_bits(gen, expr->data.binary.rhs));
            switch (op) {
                case BIN_ADD: return float_bits_from_host(lhs + rhs);
                case BIN_SUB: return float_bits_from_host(lhs - rhs);
                case BIN_MUL: return float_bits_from_host(lhs * rhs);
                case BIN_DIV: return float_bits_from_host(lhs / rhs);
                case BIN_LT: return float_bits_from_host(lhs < rhs);
                case BIN_GT: return float_bits_from_host(lhs > rhs);
                case BIN_LE: return float_bits_from_host(lhs <= rhs);
                case BIN_GE: return float_bits_from_host(lhs >= rhs);
                case BIN_EQ: return float_bits_from_host(lhs == rhs);
                case BIN_NE: return float_bits_from_host(lhs != rhs);
                default: return float_bits_from_host(0.0f);
            }
        }
        default:
            return float_bits_from_host(0.0f);
    }
}

static int *const_init_to_flat_typed(IRGen *gen, InitVal *init, const int *dims, int dim_count, TypeSpec type) {
    int total = object_slot_count(dims, dim_count);
    int *flat = (int *)xmalloc(sizeof(int) * (size_t)total);
    for (int i = 0; i < total; ++i) {
        flat[i] = type == TYPE_FLOAT ? float_bits_from_host(0.0f) : 0;
    }
    Expr **slots = init_to_expr_slots(init, dims, dim_count, total);
    for (int i = 0; i < total; ++i) {
        if (slots[i] != NULL) {
            flat[i] = type == TYPE_FLOAT ? eval_const_float_bits(gen, slots[i]) : eval_const_expr(gen, slots[i]);
        }
    }
    free(slots);
    return flat;
}

static char *llvm_float_const_from_bits(int bits) {
    float value = host_float_from_bits(bits);
    return str_printf("%.9e", (double)value);
}

static char *const_scalar_to_text_typed(int value, TypeSpec type) {
    if (type == TYPE_FLOAT) {
        return llvm_float_const_from_bits(value);
    }
    return str_printf("%d", value);
}

static char *const_flat_array_to_text_typed(const int *flat, int total, TypeSpec type) {
    StrBuf sb;
    sb_init(&sb);
    sb_append(&sb, "[");
    for (int i = 0; i < total; ++i) {
        if (i > 0) {
            sb_append(&sb, ", ");
        }
        char *text = const_scalar_to_text_typed(flat[i], type);
        sb_appendf(&sb, "%s %s", llvm_scalar_type(type), text);
        free(text);
    }
    sb_append(&sb, "]");
    return sb.data;
}

static char *ensure_loaded(IRGen *gen, Value v) {
    if (!v.is_pointer_value) {
        return xstrdup(v.value);
    }
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = load %s, %s* %s\n",
              tmp, llvm_scalar_type(v.type), llvm_scalar_type(v.type), v.value);
    return tmp;
}

static char *ensure_type(IRGen *gen, Value v, TypeSpec target) {
    char *raw = ensure_loaded(gen, v);
    if (v.type == target) {
        return raw;
    }
    char *tmp = new_temp(gen);
    if (v.type == TYPE_INT && target == TYPE_FLOAT) {
        emit_func(gen, "  %s = sitofp i32 %s to float\n", tmp, raw);
        return tmp;
    }
    if (v.type == TYPE_FLOAT && target == TYPE_INT) {
        emit_func(gen, "  %s = fptosi float %s to i32\n", tmp, raw);
        return tmp;
    }
    return raw;
}

static char *ensure_i32(IRGen *gen, Value v) {
    return ensure_type(gen, v, TYPE_INT);
}

static char *ensure_float(IRGen *gen, Value v) {
    return ensure_type(gen, v, TYPE_FLOAT);
}

static char *emit_icmp_to_i32(IRGen *gen, const char *pred, const char *lhs, const char *rhs) {
    char *icmp = new_temp(gen);
    char *res = new_temp(gen);
    emit_func(gen, "  %s = icmp %s i32 %s, %s\n", icmp, pred, lhs, rhs);
    emit_func(gen, "  %s = zext i1 %s to i32\n", res, icmp);
    return res;
}

static char *emit_fcmp_to_i32(IRGen *gen, const char *pred, const char *lhs, const char *rhs) {
    char *fcmp = new_temp(gen);
    char *res = new_temp(gen);
    emit_func(gen, "  %s = fcmp %s float %s, %s\n", fcmp, pred, lhs, rhs);
    emit_func(gen, "  %s = zext i1 %s to i32\n", res, fcmp);
    return res;
}

static char *emit_truth_i1(IRGen *gen, Value v) {
    char *tmp = new_temp(gen);
    if (v.type == TYPE_FLOAT) {
        char *f = ensure_float(gen, v);
        emit_func(gen, "  %s = fcmp one float %s, 0.000000000e+00\n", tmp, f);
    } else {
        char *i = ensure_i32(gen, v);
        emit_func(gen, "  %s = icmp ne i32 %s, 0\n", tmp, i);
    }
    return tmp;
}

static Value make_value(char *value, TypeSpec type, int dim_count, int *dims, bool is_pointer_value, bool is_param_array) {
    Value v;
    v.value = value;
    v.type = type;
    v.dim_count = dim_count;
    v.dims = dims;
    v.is_pointer_value = is_pointer_value;
    v.is_param_array = is_param_array;
    return v;
}

static Address make_address(char *ptr, TypeSpec type, int dim_count, int *dims, bool is_param_array) {
    Address a;
    a.ptr = ptr;
    a.type = type;
    a.dim_count = dim_count;
    a.dims = dims;
    a.is_param_array = is_param_array;
    return a;
}

static char *emit_linear_index(IRGen *gen, const int *dims, int dim_count, ExprList *indices) {
    char *acc = xstrdup("0");
    for (int i = 0; i < indices->count; ++i) {
        Value idx_v = gen_expr(gen, indices->items[i]);
        char *idx = ensure_i32(gen, idx_v);
        int stride = product_dims(dims, i + 1, dim_count);
        char *term = idx;
        if (stride != 1) {
            char *mul = new_temp(gen);
            emit_func(gen, "  %s = mul i32 %s, %d\n", mul, idx, stride);
            term = mul;
        }
        if (strcmp(acc, "0") == 0) {
            acc = term;
        } else {
            char *sum = new_temp(gen);
            emit_func(gen, "  %s = add i32 %s, %s\n", sum, acc, term);
            acc = sum;
        }
    }
    return acc;
}

static char *emit_flat_const_ptr(IRGen *gen, Symbol *sym, int index) {
    char *ptr = new_temp(gen);
    emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 %d\n",
              ptr, sym->flat_type, sym->flat_type, sym->llvm_name, index);
    return ptr;
}

static char *emit_flat_element_ptr(IRGen *gen, Symbol *sym, ExprList *indices) {
    char *linear = emit_linear_index(gen, sym->info.dims, sym->info.dim_count, indices);
    char *ptr = new_temp(gen);
    emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 %s\n",
              ptr, sym->flat_type, sym->flat_type, sym->llvm_name, linear);
    return ptr;
}

static Address gen_lval_address(IRGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol(gen, lval->name);
    if (sym->info.is_flat_storage) {
        char *ptr = emit_flat_element_ptr(gen, sym, &lval->indices);
        int remain = sym->info.dim_count - lval->indices.count;
        if (remain < 0) {
            remain = 0;
        }
        return make_address(ptr, sym->value_type, remain, sym->info.dims + lval->indices.count, false);
    }
    char *ptr = sym->llvm_name;
    int dim_count = sym->info.dim_count;
    int *dims = sym->info.dims;
    bool is_param_array = sym->info.is_param_array;
    for (int i = 0; i < lval->indices.count; ++i) {
        Expr *idx_expr = lval->indices.items[i];
        Value idx_v = gen_expr(gen, idx_expr);
        char *idx = ensure_i32(gen, idx_v);
        if (is_param_array && i == 0) {
            char *elem_type = llvm_type_from_dims_typed(sym->value_type, dims, dim_count);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 %s\n", tmp, elem_type, elem_type, ptr, idx);
            ptr = tmp;
            free(elem_type);
        } else {
            char *agg_type = llvm_type_from_dims_typed(sym->value_type, dims, dim_count);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 %s\n", tmp, agg_type, agg_type, ptr, idx);
            ptr = tmp;
            free(agg_type);
            dims++;
            dim_count--;
        }
        is_param_array = false;
    }
    return make_address(ptr, sym->value_type, dim_count, dims, is_param_array);
}

Value gen_expr(IRGen *gen, Expr *expr);

static Value gen_lval_expr(IRGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol(gen, lval->name);
    if (sym->info.is_flat_storage) {
        int remain = sym->info.dim_count - lval->indices.count;
        ExprList indices = lval->indices;
        if (remain == sym->info.dim_count) {
            ExprList empty = {0};
            char *ptr = emit_flat_element_ptr(gen, sym, &empty);
            if (remain == 1) {
                return make_value(ptr, sym->value_type, 0, NULL, false, true);
            }
            char *sub_type = llvm_type_from_dims_typed(sym->value_type, sym->info.dims + 1, remain - 1);
            char *cast = new_temp(gen);
            emit_func(gen, "  %s = bitcast %s* %s to %s*\n",
                      cast, llvm_scalar_type(sym->value_type), ptr, sub_type);
            free(sub_type);
            return make_value(cast, sym->value_type, remain - 1, sym->info.dims + 1, false, true);
        }
        char *ptr = emit_flat_element_ptr(gen, sym, &indices);
        if (remain <= 0) {
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = load %s, %s* %s\n",
                      tmp, llvm_scalar_type(sym->value_type), llvm_scalar_type(sym->value_type), ptr);
            return make_value(tmp, sym->value_type, 0, NULL, false, false);
        }
        if (remain == 1) {
            return make_value(ptr, sym->value_type, 0, NULL, false, true);
        }
        char *sub_type = llvm_subarray_ptr_type_typed(sym->value_type, sym->info.dims + lval->indices.count, remain);
        char *cast = new_temp(gen);
        emit_func(gen, "  %s = bitcast %s* %s to %s\n",
                  cast, llvm_scalar_type(sym->value_type), ptr, sub_type);
        free(sub_type);
        return make_value(cast, sym->value_type, remain - 1, sym->info.dims + lval->indices.count + 1, false, true);
    }
    if (lval->indices.count == 0 && sym->info.is_param_array) {
        return make_value(sym->llvm_name, sym->value_type, sym->info.dim_count, sym->info.dims, false, true);
    }
    if (lval->indices.count == 0 && sym->info.dim_count > 0) {
        if (sym->info.is_param_array) {
            return make_value(sym->llvm_name, sym->value_type, sym->info.dim_count, sym->info.dims, false, true);
        }
        char *agg_type = llvm_type_from_dims_typed(sym->value_type, sym->info.dims, sym->info.dim_count);
        char *tmp = new_temp(gen);
        emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 0\n", tmp, agg_type, agg_type, sym->llvm_name);
        free(agg_type);
        return make_value(tmp, sym->value_type, sym->info.dim_count - 1, sym->info.dims + 1, false, true);
    }
    Address addr = gen_lval_address(gen, lval);
    if (addr.dim_count == 0) {
        char *tmp = new_temp(gen);
        emit_func(gen, "  %s = load %s, %s* %s\n",
                  tmp, llvm_scalar_type(addr.type), llvm_scalar_type(addr.type), addr.ptr);
        return make_value(tmp, addr.type, 0, NULL, false, false);
    }
    char *agg_type = llvm_type_from_dims_typed(addr.type, addr.dims, addr.dim_count);
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 0\n", tmp, agg_type, agg_type, addr.ptr);
    free(agg_type);
    return make_value(tmp, addr.type, addr.dim_count - 1, addr.dims + 1, false, true);
}

static Value gen_short_circuit_expr(IRGen *gen, Expr *expr) {
    char *slot = emit_alloca(gen, "i32");
    char *true_label = new_label(gen, "logic_true");
    char *false_label = new_label(gen, "logic_false");
    char *end_label = new_label(gen, "logic_end");
    emit_func(gen, "  store i32 0, i32* %s\n", slot);
    gen_cond(gen, expr, true_label, false_label);
    emit_label(gen, true_label);
    emit_func(gen, "  store i32 1, i32* %s\n", slot);
    emit_func(gen, "  br label %%%s\n", end_label);
    gen->current_block_terminated = true;
    emit_label(gen, false_label);
    emit_func(gen, "  br label %%%s\n", end_label);
    gen->current_block_terminated = true;
    emit_label(gen, end_label);
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = load i32, i32* %s\n", tmp, slot);
    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
}

static char *emit_array_arg_as(IRGen *gen, Expr *expr, TypeSpec elem_type) {
    Value arg = gen_expr(gen, expr);
    bool needs_float_bitcast = false;
    if (elem_type == TYPE_FLOAT && expr->kind == EXPR_LVAL) {
        Symbol *sym = lookup_symbol(gen, expr->data.lval->name);
        needs_float_bitcast = sym != NULL && sym->info.is_flat_storage &&
                              sym->flat_type != NULL && strstr(sym->flat_type, "float") == NULL;
    }
    if (needs_float_bitcast) {
        char *cast = new_temp(gen);
        emit_func(gen, "  %s = bitcast i32* %s to float*\n", cast, arg.value);
        return cast;
    }
    return xstrdup(arg.value);
}

Value gen_expr(IRGen *gen, Expr *expr) {
    switch (expr->kind) {
        case EXPR_NUMBER:
            return make_value(str_printf("%d", expr->data.number), TYPE_INT, 0, NULL, false, false);
        case EXPR_FLOAT_NUMBER:
            return make_value(llvm_float_const_from_bits(expr->data.number), TYPE_FLOAT, 0, NULL, false, false);
        case EXPR_LVAL:
            return gen_lval_expr(gen, expr->data.lval);
        case EXPR_GETINT: {
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = call i32 @getint()\n", tmp);
            return make_value(tmp, TYPE_INT, 0, NULL, false, false);
        }
        case EXPR_CALL: {
            if (strcmp(expr->data.call.name, "starttime") == 0) {
                emit_func(gen, "  call void @_sysy_starttime(i32 0)\n");
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "stoptime") == 0) {
                emit_func(gen, "  call void @_sysy_stoptime(i32 0)\n");
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getch") == 0) {
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call i32 @getch()\n", tmp);
                return make_value(tmp, TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getfloat") == 0) {
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call float @getfloat()\n", tmp);
                return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getarray") == 0) {
                char *arr = emit_array_arg_as(gen, expr->data.call.args.items[0], TYPE_INT);
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call i32 @getarray(i32* %s)\n", tmp, arr);
                return make_value(tmp, TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getfarray") == 0) {
                char *arr = emit_array_arg_as(gen, expr->data.call.args.items[0], TYPE_FLOAT);
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call i32 @getfarray(float* %s)\n", tmp, arr);
                return make_value(tmp, TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putint") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *i32v = ensure_i32(gen, arg);
                emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putint(i32 %s)\n", i32v);
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putch") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *i32v = ensure_i32(gen, arg);
                char *is_nl = new_temp(gen);
                char *nl_i32 = new_temp(gen);
                emit_func(gen, "  %s = icmp eq i32 %s, 10\n", is_nl, i32v);
                emit_func(gen, "  %s = zext i1 %s to i32\n", nl_i32, is_nl);
                emit_func(gen, "  store i32 %s, i32* @__sysy_output_state\n", nl_i32);
                emit_func(gen, "  call void @putch(i32 %s)\n", i32v);
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putfloat") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *fv = ensure_float(gen, arg);
                emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putfloat(float %s)\n", fv);
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putarray") == 0) {
                Value n = gen_expr(gen, expr->data.call.args.items[0]);
                char *arr = emit_array_arg_as(gen, expr->data.call.args.items[1], TYPE_INT);
                char *i32v = ensure_i32(gen, n);
                emit_func(gen, "  store i32 1, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putarray(i32 %s, i32* %s)\n", i32v, arr);
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putfarray") == 0) {
                Value n = gen_expr(gen, expr->data.call.args.items[0]);
                char *arr = emit_array_arg_as(gen, expr->data.call.args.items[1], TYPE_FLOAT);
                char *i32v = ensure_i32(gen, n);
                emit_func(gen, "  store i32 1, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putfarray(i32 %s, float* %s)\n", i32v, arr);
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            FunctionSymbol *meta = lookup_function_meta(gen, expr->data.call.name);
            const char *callee = function_llvm_name(gen, expr->data.call.name);
            StrBuf sb;
            sb_init(&sb);
            for (int i = 0; i < expr->data.call.args.count; ++i) {
                if (i > 0) {
                    sb_append(&sb, ", ");
                }
                Value arg = gen_expr(gen, expr->data.call.args.items[i]);
                char *type = NULL;
                if (meta != NULL && i < meta->params.count) {
                    Param *param = meta->params.items[i];
                    type = llvm_param_type(param);
                    if (!param->is_array) {
                        arg.value = ensure_type(gen, arg, param->type);
                    }
                } else {
                    type = xstrdup("i32");
                    arg.value = ensure_i32(gen, arg);
                }
                sb_appendf(&sb, "%s %s", type, arg.value);
                free(type);
            }
            if (meta != NULL && meta->ret_type == TYPE_VOID) {
                emit_func(gen, "  call void @%s(%s)\n", callee, sb.data ? sb.data : "");
                return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
            char *tmp = new_temp(gen);
            TypeSpec ret_type = meta != NULL ? meta->ret_type : TYPE_INT;
            emit_func(gen, "  %s = call %s @%s(%s)\n",
                      tmp, llvm_scalar_type(ret_type), callee, sb.data ? sb.data : "");
            return make_value(tmp, ret_type, 0, NULL, false, false);
        }
        case EXPR_UNARY: {
            Value operand = gen_expr(gen, expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                char *op = ensure_type(gen, operand, operand.type);
                return make_value(op, operand.type, 0, NULL, false, false);
            }
            if (expr->data.unary.op == UNARY_MINUS) {
                char *tmp = new_temp(gen);
                if (operand.type == TYPE_FLOAT) {
                    char *op = ensure_float(gen, operand);
                    emit_func(gen, "  %s = fneg float %s\n", tmp, op);
                    return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
                }
                char *op = ensure_i32(gen, operand);
                emit_func(gen, "  %s = sub i32 0, %s\n", tmp, op);
                return make_value(tmp, TYPE_INT, 0, NULL, false, false);
            }
            char *truth = emit_truth_i1(gen, operand);
            char *not_i1 = new_temp(gen);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = xor i1 %s, true\n", not_i1, truth);
            emit_func(gen, "  %s = zext i1 %s to i32\n", tmp, not_i1);
            return make_value(tmp, TYPE_INT, 0, NULL, false, false);
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                return gen_short_circuit_expr(gen, expr);
            }
            Value lhs_v = gen_expr(gen, expr->data.binary.lhs);
            Value rhs_v = gen_expr(gen, expr->data.binary.rhs);
            bool use_float = (lhs_v.type == TYPE_FLOAT || rhs_v.type == TYPE_FLOAT) && op != BIN_MOD;
            char *tmp = new_temp(gen);
            if (use_float) {
                char *lhs = ensure_float(gen, lhs_v);
                char *rhs = ensure_float(gen, rhs_v);
                switch (op) {
                    case BIN_ADD:
                        emit_func(gen, "  %s = fadd float %s, %s\n", tmp, lhs, rhs);
                        return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_SUB:
                        emit_func(gen, "  %s = fsub float %s, %s\n", tmp, lhs, rhs);
                        return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_MUL:
                        emit_func(gen, "  %s = fmul float %s, %s\n", tmp, lhs, rhs);
                        return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_DIV:
                        emit_func(gen, "  %s = fdiv float %s, %s\n", tmp, lhs, rhs);
                        return make_value(tmp, TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_LT:
                        return make_value(emit_fcmp_to_i32(gen, "olt", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_GT:
                        return make_value(emit_fcmp_to_i32(gen, "ogt", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_LE:
                        return make_value(emit_fcmp_to_i32(gen, "ole", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_GE:
                        return make_value(emit_fcmp_to_i32(gen, "oge", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_EQ:
                        return make_value(emit_fcmp_to_i32(gen, "oeq", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_NE:
                        return make_value(emit_fcmp_to_i32(gen, "one", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    default:
                        return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
                }
            }
            char *lhs = ensure_i32(gen, lhs_v);
            char *rhs = ensure_i32(gen, rhs_v);
            switch (op) {
                case BIN_ADD:
                    emit_func(gen, "  %s = add i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
                case BIN_SUB:
                    emit_func(gen, "  %s = sub i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
                case BIN_MUL:
                    emit_func(gen, "  %s = mul i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
                case BIN_DIV:
                    emit_func(gen, "  %s = sdiv i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
                case BIN_MOD:
                    emit_func(gen, "  %s = srem i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, TYPE_INT, 0, NULL, false, false);
                case BIN_LT:
                    return make_value(emit_icmp_to_i32(gen, "slt", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_GT:
                    return make_value(emit_icmp_to_i32(gen, "sgt", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_LE:
                    return make_value(emit_icmp_to_i32(gen, "sle", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_GE:
                    return make_value(emit_icmp_to_i32(gen, "sge", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_EQ:
                    return make_value(emit_icmp_to_i32(gen, "eq", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_NE:
                    return make_value(emit_icmp_to_i32(gen, "ne", lhs, rhs), TYPE_INT, 0, NULL, false, false);
                default:
                    return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
            }
        }
    }
    return make_value(xstrdup("0"), TYPE_INT, 0, NULL, false, false);
}

void gen_cond(IRGen *gen, Expr *expr, const char *true_label, const char *false_label) {
    if (expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_OR) {
        char *mid = new_label(gen, "lor_rhs");
        gen_cond(gen, expr->data.binary.lhs, true_label, mid);
        emit_label(gen, mid);
        gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    if (expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_AND) {
        char *mid = new_label(gen, "land_rhs");
        gen_cond(gen, expr->data.binary.lhs, mid, false_label);
        emit_label(gen, mid);
        gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    Value cond = gen_expr(gen, expr);
    char *i1 = emit_truth_i1(gen, cond);
    emit_func(gen, "  br i1 %s, label %%%s, label %%%s\n", i1, true_label, false_label);
    gen->current_block_terminated = true;
}

static void emit_putch(IRGen *gen, int ch) {
    emit_func(gen, "  store i32 %d, i32* @__sysy_output_state\n", ch == 10 ? 1 : 0);
    emit_func(gen, "  call void @putch(i32 %d)\n", ch);
}

static void gen_printf(IRGen *gen, char *format, ExprList *args) {
    int arg_index = 0;
    int len = (int)strlen(format);
    for (int i = 1; i < len - 1; ++i) {
        if (format[i] == '%' && i + 1 < len - 1) {
            char spec = format[++i];
            if (spec == 'd' || spec == 'c') {
                Value v = gen_expr(gen, args->items[arg_index++]);
                char *i32v = ensure_i32(gen, v);
                emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @%s(i32 %s)\n", spec == 'd' ? "putint" : "putch", i32v);
            } else if (spec == 'f' || spec == 'a') {
                Value v = gen_expr(gen, args->items[arg_index++]);
                char *fv = ensure_float(gen, v);
                emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putfloat(float %s)\n", fv);
            } else if (spec == '%') {
                emit_putch(gen, '%');
            } else {
                emit_putch(gen, '%');
                emit_putch(gen, (unsigned char)spec);
            }
        } else if (format[i] == '\\' && i + 1 < len - 1) {
            if (format[i + 1] == 'n') {
                emit_putch(gen, 10);
            } else if (format[i + 1] == 't') {
                emit_putch(gen, 9);
            } else {
                emit_putch(gen, format[i + 1]);
            }
            ++i;
        } else {
            emit_putch(gen, (unsigned char)format[i]);
        }
    }
}

static void gen_decl(IRGen *gen, Decl *decl, bool is_global);
static void gen_stmt(IRGen *gen, Stmt *stmt);

static void gen_block(IRGen *gen, Block *block, bool new_scope) {
    if (new_scope) {
        push_scope(gen);
    }
    for (int i = 0; i < block->items.count; ++i) {
        if (block->items.kinds[i] == BLOCK_ITEM_DECL) {
            gen_decl(gen, (Decl *)block->items.items[i], false);
        } else {
            gen_stmt(gen, (Stmt *)block->items.items[i]);
        }
    }
    if (new_scope) {
        pop_scope(gen);
    }
}

static void gen_stmt(IRGen *gen, Stmt *stmt) {
    switch (stmt->kind) {
        case STMT_ASSIGN: {
            Address addr = gen_lval_address(gen, stmt->data.assign_stmt.lval);
            Value v = gen_expr(gen, stmt->data.assign_stmt.expr);
            char *value = ensure_type(gen, v, addr.type);
            emit_func(gen, "  store %s %s, %s* %s\n",
                      llvm_scalar_type(addr.type), value, llvm_scalar_type(addr.type), addr.ptr);
            return;
        }
        case STMT_EXPR:
            if (stmt->data.expr_stmt != NULL) {
                gen_expr(gen, stmt->data.expr_stmt);
            }
            return;
        case STMT_BLOCK:
            gen_block(gen, stmt->data.block_stmt, true);
            return;
        case STMT_IF: {
            char *then_label = new_label(gen, "if_then");
            char *else_label = new_label(gen, "if_else");
            char *end_label = new_label(gen, "if_end");
            gen_cond(gen, stmt->data.if_stmt.cond, then_label,
                     stmt->data.if_stmt.else_stmt ? else_label : end_label);
            emit_label(gen, then_label);
            gen_stmt(gen, stmt->data.if_stmt.then_stmt);
            if (!gen->current_block_terminated) {
                emit_func(gen, "  br label %%%s\n", end_label);
                gen->current_block_terminated = true;
            }
            if (stmt->data.if_stmt.else_stmt != NULL) {
                emit_label(gen, else_label);
                gen_stmt(gen, stmt->data.if_stmt.else_stmt);
                if (!gen->current_block_terminated) {
                    emit_func(gen, "  br label %%%s\n", end_label);
                    gen->current_block_terminated = true;
                }
            }
            emit_label(gen, end_label);
            return;
        }
        case STMT_WHILE: {
            char *cond_label = new_label(gen, "while_cond");
            char *body_label = new_label(gen, "while_body");
            char *end_label = new_label(gen, "while_end");
            emit_func(gen, "  br label %%%s\n", cond_label);
            gen->current_block_terminated = true;
            emit_label(gen, cond_label);
            string_list_push(&gen->break_labels, end_label);
            string_list_push(&gen->continue_labels, cond_label);
            gen_cond(gen, stmt->data.while_stmt.cond, body_label, end_label);
            emit_label(gen, body_label);
            gen_stmt(gen, stmt->data.while_stmt.body);
            if (!gen->current_block_terminated) {
                emit_func(gen, "  br label %%%s\n", cond_label);
                gen->current_block_terminated = true;
            }
            gen->break_labels.count--;
            gen->continue_labels.count--;
            emit_label(gen, end_label);
            return;
        }
        case STMT_BREAK:
            emit_func(gen, "  br label %%%s\n", gen->break_labels.items[gen->break_labels.count - 1]);
            gen->current_block_terminated = true;
            return;
        case STMT_CONTINUE:
            emit_func(gen, "  br label %%%s\n", gen->continue_labels.items[gen->continue_labels.count - 1]);
            gen->current_block_terminated = true;
            return;
        case STMT_RETURN:
            if (stmt->data.return_expr == NULL) {
                emit_func(gen, "  ret void\n");
            } else {
                Value v = gen_expr(gen, stmt->data.return_expr);
                char *ret = ensure_type(gen, v, gen->current_ret_type);
                emit_func(gen, "  ret %s %s\n", llvm_scalar_type(gen->current_ret_type), ret);
            }
            gen->current_block_terminated = true;
            return;
        case STMT_PRINTF:
            gen_printf(gen, stmt->data.printf_stmt.format, &stmt->data.printf_stmt.args);
            return;
    }
}

static void emit_local_flat_array_init(IRGen *gen, Symbol *sym, InitVal *init) {
    char *raw_ptr = new_temp(gen);
    emit_func(gen, "  %s = bitcast %s* %s to i8*\n",
              raw_ptr, sym->flat_type, sym->llvm_name);
    emit_func(gen, "  call i8* @memset(i8* %s, i32 0, i64 %d)\n",
              raw_ptr, sym->info.total_slots * 4);
    if (init == NULL) {
        return;
    }
    Expr **slots = init_to_expr_slots(init, sym->info.dims, sym->info.dim_count, sym->info.total_slots);
    for (int i = 0; i < sym->info.total_slots; ++i) {
        if (slots[i] == NULL) {
            continue;
        }
        char *elem_ptr = emit_flat_const_ptr(gen, sym, i);
        Value v = gen_expr(gen, slots[i]);
        char *value = ensure_type(gen, v, sym->value_type);
        emit_func(gen, "  store %s %s, %s* %s\n",
                  llvm_scalar_type(sym->value_type), value, llvm_scalar_type(sym->value_type), elem_ptr);
    }
    free(slots);
}

static void gen_decl(IRGen *gen, Decl *decl, bool is_global) {
    for (int i = 0; i < decl->items.count; ++i) {
        DeclItem *item = decl->items.items[i];
        Symbol *sym = scope_add_symbol(gen, item->name);
        sym->is_global = is_global;
        sym->value_type = decl->type;
        sym->info.dim_count = item->dims.count;
        sym->info.dims = copy_dims(item->dims.data, item->dims.count);
        sym->info.total_slots = object_slot_count(item->dims.data, item->dims.count);
        sym->info.is_const = decl->is_const;
        sym->info.is_param_array = false;
        sym->info.is_flat_storage = false;
        if (item->dims.count == 0) {
            if (decl->is_const && item->init != NULL) {
                sym->is_const_scalar = true;
                sym->const_scalar = decl->type == TYPE_FLOAT
                                        ? eval_const_float_bits(gen, item->init->expr)
                                        : eval_const_expr(gen, item->init->expr);
            }
            if (is_global) {
                int init_val = 0;
                if (item->init != NULL) {
                    init_val = decl->type == TYPE_FLOAT
                                   ? eval_const_float_bits(gen, item->init->expr)
                                   : eval_const_expr(gen, item->init->expr);
                }
                sym->llvm_name = str_printf("@g%d", gen->global_id++);
                char *text = const_scalar_to_text_typed(init_val, decl->type);
                emit_global(gen, "%s = dso_local global %s %s\n",
                            sym->llvm_name, llvm_scalar_type(decl->type), text);
                free(text);
            } else {
                sym->llvm_name = emit_alloca(gen, llvm_scalar_type(decl->type));
                if (item->init != NULL) {
                    Value v = gen_expr(gen, item->init->expr);
                    char *value = ensure_type(gen, v, decl->type);
                    emit_func(gen, "  store %s %s, %s* %s\n",
                              llvm_scalar_type(decl->type), value, llvm_scalar_type(decl->type), sym->llvm_name);
                } else {
                    char *zero = const_scalar_to_text_typed(decl->type == TYPE_FLOAT ? float_bits_from_host(0.0f) : 0, decl->type);
                    emit_func(gen, "  store %s %s, %s* %s\n",
                              llvm_scalar_type(decl->type), zero, llvm_scalar_type(decl->type), sym->llvm_name);
                    free(zero);
                }
            }
        } else {
            sym->flat_type = llvm_flat_array_type_typed(decl->type, sym->info.total_slots);
            if (decl->is_const && item->init != NULL) {
                sym->const_flat = const_init_to_flat_typed(gen, item->init, item->dims.data, item->dims.count, decl->type);
            }
            if (is_global) {
                sym->info.is_flat_storage = true;
                sym->llvm_name = str_printf("@g%d", gen->global_id++);
                if (item->init == NULL) {
                    emit_global(gen, "%s = dso_local global %s zeroinitializer\n", sym->llvm_name, sym->flat_type);
                } else {
                    int *flat = const_init_to_flat_typed(gen, item->init, item->dims.data, item->dims.count, decl->type);
                    char *text = const_flat_array_to_text_typed(flat, sym->info.total_slots, decl->type);
                    emit_global(gen, "%s = dso_local global %s %s\n", sym->llvm_name, sym->flat_type, text);
                }
            } else {
                sym->info.is_flat_storage = true;
                sym->llvm_name = emit_alloca(gen, sym->flat_type);
                emit_local_flat_array_init(gen, sym, item->init);
            }
        }
    }
}

static void gen_function(IRGen *gen, FuncDef *func) {
    StrBuf allocas;
    StrBuf body;
    sb_init(&allocas);
    sb_init(&body);
    gen->current_allocas = &allocas;
    gen->current_body = &body;
    push_scope(gen);
    gen->current_ret_type = func->ret_type;
    gen->current_block_terminated = false;
    StrBuf header;
    sb_init(&header);
    sb_appendf(&header, "define dso_local %s @%s(",
               llvm_scalar_type(func->ret_type), function_llvm_name(gen, func->name));
    for (int i = 0; i < func->params.count; ++i) {
        if (i > 0) {
            sb_append(&header, ", ");
        }
        char *type = llvm_param_type(func->params.items[i]);
        sb_appendf(&header, "%s %%p%d", type, i);
        free(type);
    }
    sb_append(&header, ") {\n");
    gen->current_block_terminated = false;
    for (int i = 0; i < func->params.count; ++i) {
        Param *param = func->params.items[i];
        Symbol *sym = scope_add_symbol(gen, param->name);
        sym->info.is_const = false;
        sym->value_type = param->type;
        sym->info.dim_count = param->dims.count;
        sym->info.dims = copy_dims(param->dims.data, param->dims.count);
        sym->info.is_param_array = param->is_array;
        if (!param->is_array) {
            sym->llvm_name = emit_alloca(gen, llvm_scalar_type(param->type));
            emit_func(gen, "  store %s %%p%d, %s* %s\n",
                      llvm_scalar_type(param->type), i, llvm_scalar_type(param->type), sym->llvm_name);
        } else {
            sym->llvm_name = str_printf("%%p%d", i);
        }
    }
    gen_block(gen, func->block, false);
    if (!gen->current_block_terminated) {
        if (func->ret_type == TYPE_INT) {
            emit_func(gen, "  ret i32 0\n");
        } else if (func->ret_type == TYPE_FLOAT) {
            emit_func(gen, "  ret float 0.000000000e+00\n");
        } else {
            emit_func(gen, "  ret void\n");
        }
    }
    sb_append(&gen->functions, header.data);
    sb_append(&gen->functions, "entry:\n");
    sb_append(&gen->functions, allocas.data ? allocas.data : "");
    sb_append(&gen->functions, body.data ? body.data : "");
    sb_append(&gen->functions, "}\n\n");
    pop_scope(gen);
    gen->current_allocas = NULL;
    gen->current_body = NULL;
}

static void generate_program_llvm_text(Program *program, FILE *out) {
    IRGen gen;
    memset(&gen, 0, sizeof(gen));
    gen.out = out;
    sb_init(&gen.globals);
    sb_init(&gen.functions);
    memset(g_function_buckets, 0, sizeof(g_function_buckets));
    push_scope(&gen);
    sb_append(&gen.globals, "source_filename = \"sysy.ll\"\n");
    sb_append(&gen.globals, "target triple = \"riscv64-unknown-linux-gnu\"\n\n");
    sb_append(&gen.globals, "@__sysy_output_state = dso_local global i32 2\n\n");
    sb_append(&gen.globals, "declare i32 @getint()\n");
    sb_append(&gen.globals, "declare i32 @getch()\n");
    sb_append(&gen.globals, "declare float @getfloat()\n");
    sb_append(&gen.globals, "declare i32 @getarray(i32*)\n");
    sb_append(&gen.globals, "declare i32 @getfarray(float*)\n");
    sb_append(&gen.globals, "declare void @putint(i32)\n");
    sb_append(&gen.globals, "declare void @putch(i32)\n");
    sb_append(&gen.globals, "declare void @putarray(i32, i32*)\n");
    sb_append(&gen.globals, "declare void @putfloat(float)\n");
    sb_append(&gen.globals, "declare void @putfarray(i32, float*)\n");
    sb_append(&gen.globals, "declare void @putf(i8*, ...)\n");
    sb_append(&gen.globals, "declare void @_sysy_starttime(i32)\n");
    sb_append(&gen.globals, "declare void @_sysy_stoptime(i32)\n");
    sb_append(&gen.globals, "declare i8* @memset(i8*, i32, i64)\n\n");

    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            add_function_meta(&gen, item->data.func);
        }
    }
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_DECL) {
            gen_decl(&gen, item->data.decl, true);
        }
    }
    if (gen.globals.len > 0 && gen.globals.data[gen.globals.len - 1] != '\n') {
        sb_append(&gen.globals, "\n");
    }
    sb_append(&gen.globals, "\n");
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            gen_function(&gen, item->data.func);
        }
    }
    fputs(gen.globals.data ? gen.globals.data : "", out);
    fputs(gen.functions.data ? gen.functions.data : "", out);
}

typedef struct MemSymbol MemSymbol;
typedef struct MemScope MemScope;
typedef struct MemFunctionMeta MemFunctionMeta;

typedef struct {
    IRValue *value;
    TypeSpec type;
    int dim_count;
    int *dims;
    bool is_pointer;
    bool is_param_array;
} MemValue;

typedef struct {
    IRValue *ptr;
    TypeSpec type;
    int dim_count;
    int *dims;
    bool is_param_array;
    IRType *object_type;
} MemAddress;

struct MemSymbol {
    char *name;
    TypeSpec value_type;
    int dim_count;
    int *dims;
    int total_slots;
    bool is_const;
    bool is_const_scalar;
    bool is_global;
    bool is_param_array;
    bool is_flat_storage;
    int const_scalar;
    int *const_flat;
    IRValue *addr;
    IRType *object_type;
    bool is_inline_value;
    MemValue inline_value;
    MemSymbol *next;
};

struct MemScope {
    MemSymbol *symbols;
    MemScope *next;
};

struct MemFunctionMeta {
    char *name;
    IRFunction *function;
    TypeSpec ret_type;
    ParamList params;
    bool has_sysy_params;
    FuncDef *ast_func;
    MemFunctionMeta *next;
};

typedef struct {
    IRModule *module;
    IRFunction *current_function;
    IRBasicBlock *current_block;
    MemScope *scopes;
    MemFunctionMeta *functions;
    int temp_id;
    int label_id;
    int global_id;
    TypeSpec current_ret_type;
    IRBasicBlock **break_blocks;
    int break_count;
    int break_capacity;
    IRBasicBlock **continue_blocks;
    int continue_count;
    int continue_capacity;
    int inline_depth;
} MemIRGen;

static void mem_value_list_push(IRValueList *list, IRValue *value) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(IRValue *), list->count + 1);
    list->items[list->count++] = value;
}

static void mem_global_list_push(IRGlobalList *list, IRGlobal *global) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(IRGlobal *), list->count + 1);
    if (list->count > 0) {
        list->items[list->count - 1]->next = global;
    }
    list->items[list->count++] = global;
}

static void mem_param_list_push(IRParameterList *list, IRParameter *param) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(IRParameter *), list->count + 1);
    list->items[list->count++] = param;
}

static void mem_block_list_push(IRBasicBlockList *list, IRBasicBlock *block) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(IRBasicBlock *), list->count + 1);
    list->items[list->count++] = block;
}

static void mem_function_list_push(IRFunctionList *list, IRFunction *function) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(IRFunction *), list->count + 1);
    if (list->count > 0) {
        list->items[list->count - 1]->next = function;
    }
    list->items[list->count++] = function;
}

static void mem_bb_ptr_push(IRBasicBlock ***items, int *count, int *capacity, IRBasicBlock *block) {
    ensure_capacity((void **)items, capacity, sizeof(IRBasicBlock *), *count + 1);
    (*items)[(*count)++] = block;
}

static IRType *mem_new_type(IRTypeKind kind, TypeSpec sysy_type) {
    IRType *type = (IRType *)xmalloc(sizeof(IRType));
    memset(type, 0, sizeof(IRType));
    type->kind = kind;
    type->sysy_type = sysy_type;
    return type;
}

static IRType *mem_scalar_type(IRModule *module, TypeSpec type) {
    switch (type) {
        case TYPE_INT: return module->i32_type;
        case TYPE_FLOAT: return module->float_type;
        case TYPE_VOID: return module->void_type;
    }
    return module->i32_type;
}

static IRType *mem_pointer_type(IRType *pointee) {
    IRType *type = mem_new_type(IR_TYPE_POINTER, pointee->sysy_type);
    type->data.pointer.pointee = pointee;
    return type;
}

static IRType *mem_array_type(IRType *element, int length) {
    IRType *type = mem_new_type(IR_TYPE_ARRAY, element->sysy_type);
    type->data.array.length = length;
    type->data.array.element = element;
    return type;
}

static IRType *mem_array_type_from_dims(IRModule *module, TypeSpec elem_type, const int *dims, int dim_count) {
    IRType *type = mem_scalar_type(module, elem_type);
    for (int i = dim_count - 1; i >= 0; --i) {
        type = mem_array_type(type, dims[i]);
    }
    return type;
}

static IRType *mem_function_type(IRType *ret, IRType **params, int param_count, bool is_variadic) {
    IRType *type = mem_new_type(IR_TYPE_FUNCTION, ret->sysy_type);
    type->data.function.ret = ret;
    type->data.function.params = params;
    type->data.function.param_count = param_count;
    type->data.function.is_variadic = is_variadic;
    return type;
}

static IRType *mem_param_type(IRModule *module, Param *param) {
    if (!param->is_array) {
        return mem_scalar_type(module, param->type);
    }
    IRType *pointee = param->dims.count == 0
                          ? mem_scalar_type(module, param->type)
                          : mem_array_type_from_dims(module, param->type, param->dims.data, param->dims.count);
    return mem_pointer_type(pointee);
}

static TypeSpec mem_ir_base_type(IRType *type) {
    if (type == NULL) {
        return TYPE_INT;
    }
    while (type->kind == IR_TYPE_POINTER || type->kind == IR_TYPE_ARRAY) {
        if (type->kind == IR_TYPE_POINTER) {
            type = type->data.pointer.pointee;
        } else {
            type = type->data.array.element;
        }
        if (type == NULL) {
            return TYPE_INT;
        }
    }
    return type->sysy_type;
}

static int mem_ir_ptr_level(IRType *type) {
    int level = 0;
    while (type != NULL && type->kind == IR_TYPE_POINTER) {
        level++;
        type = type->data.pointer.pointee;
    }
    return level;
}

static void mem_set_value_type(IRValue *value, IRType *type) {
    value->type = type;
    value->base_type = mem_ir_base_type(type);
    value->ptr_level = mem_ir_ptr_level(type);
}

static IRModule *mem_new_module(const char *name) {
    IRModule *module = (IRModule *)xmalloc(sizeof(IRModule));
    memset(module, 0, sizeof(IRModule));
    module->name = xstrdup(name);
    module->void_type = mem_new_type(IR_TYPE_VOID, TYPE_VOID);
    module->i1_type = mem_new_type(IR_TYPE_I1, TYPE_INT);
    module->i32_type = mem_new_type(IR_TYPE_I32, TYPE_INT);
    module->float_type = mem_new_type(IR_TYPE_FLOAT, TYPE_FLOAT);
    return module;
}

static IRValue *mem_new_value(IRValueKind kind, IRType *type, const char *name) {
    IRValue *value = (IRValue *)xmalloc(sizeof(IRValue));
    memset(value, 0, sizeof(IRValue));
    value->kind = kind;
    mem_set_value_type(value, type);
    value->name = name != NULL ? xstrdup(name) : NULL;
    return value;
}

static IRValue *mem_const_int(MemIRGen *gen, int value) {
    IRValue *ir = mem_new_value(IR_VALUE_CONST_INT, gen->module->i32_type, NULL);
    ir->data.int_value = value;
    return ir;
}

static IRValue *mem_const_i1(MemIRGen *gen, int value) {
    IRValue *ir = mem_new_value(IR_VALUE_CONST_INT, gen->module->i1_type, NULL);
    ir->data.int_value = value != 0;
    return ir;
}

static IRValue *mem_const_float(MemIRGen *gen, int bits) {
    IRValue *ir = mem_new_value(IR_VALUE_CONST_FLOAT, gen->module->float_type, NULL);
    ir->data.float_bits = bits;
    return ir;
}

static IRValue *mem_const_zero(MemIRGen *gen, IRType *type) {
    IRValue *ir = mem_new_value(IR_VALUE_CONST_ZERO, type, NULL);
    (void)gen;
    return ir;
}

static char *mem_new_temp(MemIRGen *gen) {
    return str_printf("%%t%d", gen->temp_id++);
}

static char *mem_new_label(MemIRGen *gen, const char *prefix) {
    return str_printf("%s%d", prefix, gen->label_id++);
}

static bool mem_block_terminated(MemIRGen *gen) {
    if (gen->current_block == NULL || gen->current_block->last_inst == NULL) {
        return false;
    }
    IRInstructionKind kind = gen->current_block->last_inst->kind;
    return kind == IR_INST_BR || kind == IR_INST_RET;
}

static IRBasicBlock *mem_create_block(MemIRGen *gen, const char *name) {
    IRBasicBlock *block = (IRBasicBlock *)xmalloc(sizeof(IRBasicBlock));
    memset(block, 0, sizeof(IRBasicBlock));
    block->name = xstrdup(name);
    block->parent = gen->current_function;
    if (gen->current_function != NULL) {
        if (gen->current_function->blocks.count > 0) {
            gen->current_function->blocks.items[gen->current_function->blocks.count - 1]->next = block;
        }
        mem_block_list_push(&gen->current_function->blocks, block);
        if (gen->current_function->entry == NULL) {
            gen->current_function->entry = block;
        }
    }
    return block;
}

static void mem_position_at(MemIRGen *gen, IRBasicBlock *block) {
    gen->current_block = block;
}

static IRInstruction *mem_emit_inst(MemIRGen *gen, IRInstructionKind kind, IRType *result_type) {
    IRInstruction *inst = (IRInstruction *)xmalloc(sizeof(IRInstruction));
    memset(inst, 0, sizeof(IRInstruction));
    inst->kind = kind;
    inst->result_type = result_type;
    inst->parent = gen->current_block;
    if (result_type != NULL && result_type->kind != IR_TYPE_VOID) {
        inst->result.kind = IR_VALUE_INSTRUCTION;
        mem_set_value_type(&inst->result, result_type);
        inst->result.name = mem_new_temp(gen);
        inst->result.data.instruction = inst;
    }
    if (gen->current_block != NULL) {
        inst->prev = gen->current_block->last_inst;
        if (gen->current_block->last_inst != NULL) {
            gen->current_block->last_inst->next = inst;
        } else {
            gen->current_block->first_inst = inst;
        }
        gen->current_block->last_inst = inst;
    }
    return inst;
}

static IRValue *mem_inst_result(IRInstruction *inst) {
    return &inst->result;
}

static void mem_add_cfg_edge(IRBasicBlock *from, IRBasicBlock *to) {
    if (from == NULL || to == NULL) {
        return;
    }
    for (int i = 0; i < from->succ_count; ++i) {
        if (from->succs[i] == to) {
            return;
        }
    }
    mem_bb_ptr_push(&from->succs, &from->succ_count, &from->succ_capacity, to);
    mem_bb_ptr_push(&to->preds, &to->pred_count, &to->pred_capacity, from);
}

static IRValue *mem_emit_alloca(MemIRGen *gen, IRType *allocated_type) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_ALLOCA, mem_pointer_type(allocated_type));
    inst->data.alloca_inst.allocated_type = allocated_type;
    inst->data.alloca_inst.alignment = 4;
    return mem_inst_result(inst);
}

static IRValue *mem_emit_load(MemIRGen *gen, IRType *value_type, IRValue *ptr) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_LOAD, value_type);
    inst->data.load_inst.ptr = ptr;
    inst->data.load_inst.value_type = value_type;
    inst->data.load_inst.alignment = 4;
    return mem_inst_result(inst);
}

static void mem_emit_store(MemIRGen *gen, IRValue *value, IRValue *ptr) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_STORE, gen->module->void_type);
    inst->data.store_inst.value = value;
    inst->data.store_inst.ptr = ptr;
    inst->data.store_inst.alignment = 4;
}

static IRValue *mem_emit_binary(MemIRGen *gen, IRInstructionKind kind, IRType *type, IRValue *lhs, IRValue *rhs) {
    IRInstruction *inst = mem_emit_inst(gen, kind, type);
    inst->data.binary_inst.lhs = lhs;
    inst->data.binary_inst.rhs = rhs;
    return mem_inst_result(inst);
}

static IRValue *mem_emit_icmp(MemIRGen *gen, IRIcmpPredicate pred, IRValue *lhs, IRValue *rhs) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_ICMP, gen->module->i1_type);
    inst->data.icmp_inst.pred = pred;
    inst->data.icmp_inst.lhs = lhs;
    inst->data.icmp_inst.rhs = rhs;
    return mem_inst_result(inst);
}

static IRValue *mem_emit_fcmp(MemIRGen *gen, IRFcmpPredicate pred, IRValue *lhs, IRValue *rhs) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_FCMP, gen->module->i1_type);
    inst->data.fcmp_inst.pred = pred;
    inst->data.fcmp_inst.lhs = lhs;
    inst->data.fcmp_inst.rhs = rhs;
    return mem_inst_result(inst);
}

static IRValue *mem_emit_cast(MemIRGen *gen, IRInstructionKind kind, IRValue *value, IRType *to_type) {
    IRInstruction *inst = mem_emit_inst(gen, kind, to_type);
    if (kind == IR_INST_BITCAST) {
        inst->data.bitcast_inst.value = value;
        inst->data.bitcast_inst.to_type = to_type;
    } else {
        inst->data.cast_inst.value = value;
        inst->data.cast_inst.to_type = to_type;
    }
    return mem_inst_result(inst);
}

static void mem_emit_br(MemIRGen *gen, IRValue *cond, IRBasicBlock *true_block, IRBasicBlock *false_block) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_BR, gen->module->void_type);
    inst->data.br_inst.is_conditional = cond != NULL;
    inst->data.br_inst.condition = cond;
    inst->data.br_inst.true_block = true_block;
    inst->data.br_inst.false_block = false_block;
    mem_add_cfg_edge(gen->current_block, true_block);
    if (cond != NULL) {
        mem_add_cfg_edge(gen->current_block, false_block);
    }
}

static void mem_emit_ret(MemIRGen *gen, IRValue *value) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_RET, gen->module->void_type);
    inst->data.ret_inst.value = value;
}

static IRValue *mem_emit_call(MemIRGen *gen, IRFunction *callee, IRType *ret_type, IRValueList args) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_CALL, ret_type);
    inst->data.call_inst.callee = callee;
    inst->data.call_inst.ret_type = ret_type;
    inst->data.call_inst.args = args;
    return ret_type != NULL && ret_type->kind != IR_TYPE_VOID ? mem_inst_result(inst) : NULL;
}

static IRValue *mem_emit_gep(MemIRGen *gen, IRValue *base_ptr, IRType *source_type,
                             IRValueList indices, IRType *result_pointee, bool inbounds) {
    IRInstruction *inst = mem_emit_inst(gen, IR_INST_GETELEMENTPTR, mem_pointer_type(result_pointee));
    inst->data.gep_inst.base_ptr = base_ptr;
    inst->data.gep_inst.source_element_type = source_type;
    inst->data.gep_inst.indices = indices;
    inst->data.gep_inst.inbounds = inbounds;
    return mem_inst_result(inst);
}

static void mem_push_scope(MemIRGen *gen) {
    MemScope *scope = (MemScope *)xmalloc(sizeof(MemScope));
    memset(scope, 0, sizeof(MemScope));
    scope->next = gen->scopes;
    gen->scopes = scope;
}

static void mem_pop_scope(MemIRGen *gen) {
    if (gen->scopes != NULL) {
        gen->scopes = gen->scopes->next;
    }
}

static MemSymbol *mem_add_symbol(MemIRGen *gen, const char *name) {
    MemSymbol *sym = (MemSymbol *)xmalloc(sizeof(MemSymbol));
    memset(sym, 0, sizeof(MemSymbol));
    sym->name = xstrdup(name);
    sym->next = gen->scopes->symbols;
    gen->scopes->symbols = sym;
    return sym;
}

static MemSymbol *mem_lookup_symbol(MemIRGen *gen, const char *name) {
    for (MemScope *scope = gen->scopes; scope != NULL; scope = scope->next) {
        for (MemSymbol *sym = scope->symbols; sym != NULL; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

static IRFunction *mem_create_function(MemIRGen *gen, const char *name, TypeSpec ret_type,
                                       ParamList params, bool has_sysy_params, bool is_external,
                                       bool is_variadic) {
    IRFunction *function = (IRFunction *)xmalloc(sizeof(IRFunction));
    memset(function, 0, sizeof(IRFunction));
    function->name = xstrdup(name);
    function->ret_type = mem_scalar_type(gen->module, ret_type);
    function->sysy_ret_type = ret_type;
    function->is_external = is_external;
    IRType **param_types = NULL;
    int param_count = has_sysy_params ? params.count : 0;
    if (param_count > 0) {
        param_types = (IRType **)xmalloc(sizeof(IRType *) * (size_t)param_count);
    }
    for (int i = 0; i < param_count; ++i) {
        Param *param = params.items[i];
        IRParameter *ir_param = (IRParameter *)xmalloc(sizeof(IRParameter));
        memset(ir_param, 0, sizeof(IRParameter));
        ir_param->name = str_printf("%%p%d", i);
        ir_param->type = mem_param_type(gen->module, param);
        ir_param->sysy_type = param->type;
        ir_param->is_array = param->is_array;
        ir_param->dims = param->dims;
        ir_param->value.kind = IR_VALUE_PARAM;
        mem_set_value_type(&ir_param->value, ir_param->type);
        ir_param->value.name = xstrdup(ir_param->name);
        ir_param->value.data.param = ir_param;
        param_types[i] = ir_param->type;
        mem_param_list_push(&function->params, ir_param);
    }
    function->type = mem_function_type(function->ret_type, param_types, param_count, is_variadic);
    mem_function_list_push(&gen->module->functions, function);
    return function;
}

static MemFunctionMeta *mem_add_function_meta(MemIRGen *gen, const char *name, IRFunction *function,
                                              TypeSpec ret_type, ParamList params, bool has_sysy_params) {
    MemFunctionMeta *meta = (MemFunctionMeta *)xmalloc(sizeof(MemFunctionMeta));
    memset(meta, 0, sizeof(MemFunctionMeta));
    meta->name = xstrdup(name);
    meta->function = function;
    meta->ret_type = ret_type;
    meta->params = params;
    meta->has_sysy_params = has_sysy_params;
    meta->ast_func = NULL;
    meta->next = gen->functions;
    gen->functions = meta;
    return meta;
}

static MemFunctionMeta *mem_lookup_function(MemIRGen *gen, const char *name) {
    for (MemFunctionMeta *meta = gen->functions; meta != NULL; meta = meta->next) {
        if (strcmp(meta->name, name) == 0) {
            return meta;
        }
    }
    return NULL;
}

static void mem_add_runtime_function(MemIRGen *gen, const char *name, TypeSpec ret_type) {
    ParamList empty = {0};
    IRFunction *function = mem_create_function(gen, name, ret_type, empty, false, true, false);
    mem_add_function_meta(gen, name, function, ret_type, empty, false);
}

static Param *mem_runtime_param(TypeSpec type, bool is_array) {
    IntList dims = {0};
    return make_param(type, xstrdup("_"), is_array, dims);
}

static void mem_add_runtime_function_with_params(MemIRGen *gen, const char *name,
                                                 TypeSpec ret_type, ParamList params) {
    IRFunction *function = mem_create_function(gen, name, ret_type, params, true, true, false);
    mem_add_function_meta(gen, name, function, ret_type, params, true);
}

static void mem_add_runtime_functions(MemIRGen *gen) {
    ParamList empty = {0};
    mem_add_runtime_function_with_params(gen, "getint", TYPE_INT, empty);
    mem_add_runtime_function_with_params(gen, "getch", TYPE_INT, empty);
    mem_add_runtime_function_with_params(gen, "getfloat", TYPE_FLOAT, empty);

    ParamList getarray_params = {0};
    param_list_push(&getarray_params, mem_runtime_param(TYPE_INT, true));
    mem_add_runtime_function_with_params(gen, "getarray", TYPE_INT, getarray_params);

    ParamList getfarray_params = {0};
    param_list_push(&getfarray_params, mem_runtime_param(TYPE_FLOAT, true));
    mem_add_runtime_function_with_params(gen, "getfarray", TYPE_INT, getfarray_params);

    ParamList putint_params = {0};
    param_list_push(&putint_params, mem_runtime_param(TYPE_INT, false));
    mem_add_runtime_function_with_params(gen, "putint", TYPE_VOID, putint_params);

    ParamList putch_params = {0};
    param_list_push(&putch_params, mem_runtime_param(TYPE_INT, false));
    mem_add_runtime_function_with_params(gen, "putch", TYPE_VOID, putch_params);

    ParamList putarray_params = {0};
    param_list_push(&putarray_params, mem_runtime_param(TYPE_INT, false));
    param_list_push(&putarray_params, mem_runtime_param(TYPE_INT, true));
    mem_add_runtime_function_with_params(gen, "putarray", TYPE_VOID, putarray_params);

    ParamList putfloat_params = {0};
    param_list_push(&putfloat_params, mem_runtime_param(TYPE_FLOAT, false));
    mem_add_runtime_function_with_params(gen, "putfloat", TYPE_VOID, putfloat_params);

    ParamList putfarray_params = {0};
    param_list_push(&putfarray_params, mem_runtime_param(TYPE_INT, false));
    param_list_push(&putfarray_params, mem_runtime_param(TYPE_FLOAT, true));
    mem_add_runtime_function_with_params(gen, "putfarray", TYPE_VOID, putfarray_params);

    mem_add_runtime_function(gen, "putf", TYPE_VOID);
    mem_add_runtime_function(gen, "_sysy_starttime", TYPE_VOID);
    mem_add_runtime_function(gen, "_sysy_stoptime", TYPE_VOID);
    mem_add_runtime_function(gen, "memset", TYPE_INT);
}

static bool mem_runtime_param_is_float_scalar(const char *name, int index) {
    return strcmp(name, "putfloat") == 0 && index == 0;
}

static bool mem_runtime_param_is_pointer(const char *name, int index) {
    return (strcmp(name, "getarray") == 0 && index == 0) ||
           (strcmp(name, "getfarray") == 0 && index == 0) ||
           (strcmp(name, "putarray") == 0 && index == 1) ||
           (strcmp(name, "putfarray") == 0 && index == 1) ||
           (strcmp(name, "memset") == 0 && index == 0);
}

static MemValue mem_make_value(IRValue *value, TypeSpec type, int dim_count, int *dims,
                               bool is_pointer, bool is_param_array) {
    MemValue result;
    result.value = value;
    result.type = type;
    result.dim_count = dim_count;
    result.dims = dims;
    result.is_pointer = is_pointer;
    result.is_param_array = is_param_array;
    return result;
}

static MemAddress mem_make_address(IRValue *ptr, TypeSpec type, int dim_count, int *dims,
                                   bool is_param_array, IRType *object_type) {
    MemAddress addr;
    addr.ptr = ptr;
    addr.type = type;
    addr.dim_count = dim_count;
    addr.dims = dims;
    addr.is_param_array = is_param_array;
    addr.object_type = object_type;
    return addr;
}

static void mem_loop_push(MemIRGen *gen, IRBasicBlock *break_block, IRBasicBlock *continue_block) {
    mem_bb_ptr_push(&gen->break_blocks, &gen->break_count, &gen->break_capacity, break_block);
    mem_bb_ptr_push(&gen->continue_blocks, &gen->continue_count, &gen->continue_capacity, continue_block);
}

static void mem_loop_pop(MemIRGen *gen) {
    if (gen->break_count > 0) {
        gen->break_count--;
    }
    if (gen->continue_count > 0) {
        gen->continue_count--;
    }
}

static int mem_eval_const_int(MemIRGen *gen, Expr *expr);
static int mem_eval_const_float_bits(MemIRGen *gen, Expr *expr);

static TypeSpec mem_const_expr_type(MemIRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return TYPE_INT;
    }
    switch (expr->kind) {
        case EXPR_FLOAT_NUMBER:
            return TYPE_FLOAT;
        case EXPR_LVAL: {
            MemSymbol *sym = mem_lookup_symbol(gen, expr->data.lval->name);
            return sym != NULL ? sym->value_type : TYPE_INT;
        }
        case EXPR_UNARY:
            return expr->data.unary.op == UNARY_NOT ? TYPE_INT
                                                    : mem_const_expr_type(gen, expr->data.unary.operand);
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_LT || op == BIN_GT || op == BIN_LE || op == BIN_GE ||
                op == BIN_EQ || op == BIN_NE || op == BIN_AND || op == BIN_OR) {
                return TYPE_INT;
            }
            return (mem_const_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT ||
                    mem_const_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
        }
        default:
            return TYPE_INT;
    }
}

static int mem_eval_const_lval(MemIRGen *gen, LVal *lval) {
    MemSymbol *sym = mem_lookup_symbol(gen, lval->name);
    if (sym == NULL) {
        return 0;
    }
    if (sym->dim_count == 0) {
        return sym->const_scalar;
    }
    int index = 0;
    for (int i = 0; i < lval->indices.count; ++i) {
        int idx = mem_eval_const_int(gen, lval->indices.items[i]);
        int stride = product_dims(sym->dims, i + 1, sym->dim_count);
        index += idx * stride;
    }
    return sym->const_flat != NULL ? sym->const_flat[index] : 0;
}

static bool mem_eval_const_truth(MemIRGen *gen, Expr *expr) {
    if (mem_const_expr_type(gen, expr) == TYPE_FLOAT) {
        return host_float_from_bits(mem_eval_const_float_bits(gen, expr)) != 0.0f;
    }
    return mem_eval_const_int(gen, expr) != 0;
}

static int mem_eval_const_int(MemIRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return 0;
    }
    if (mem_const_expr_type(gen, expr) == TYPE_FLOAT) {
        return (int)host_float_from_bits(mem_eval_const_float_bits(gen, expr));
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_FLOAT_NUMBER:
            return expr->data.number;
        case EXPR_LVAL:
            return mem_eval_const_lval(gen, expr->data.lval);
        case EXPR_UNARY: {
            int v = mem_eval_const_int(gen, expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_MINUS) {
                return -v;
            }
            if (expr->data.unary.op == UNARY_NOT) {
                return !mem_eval_const_truth(gen, expr->data.unary.operand);
            }
            return v;
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND) {
                return mem_eval_const_truth(gen, expr->data.binary.lhs) &&
                       mem_eval_const_truth(gen, expr->data.binary.rhs);
            }
            if (op == BIN_OR) {
                return mem_eval_const_truth(gen, expr->data.binary.lhs) ||
                       mem_eval_const_truth(gen, expr->data.binary.rhs);
            }
            int lhs = mem_eval_const_int(gen, expr->data.binary.lhs);
            int rhs = mem_eval_const_int(gen, expr->data.binary.rhs);
            switch (op) {
                case BIN_ADD: return lhs + rhs;
                case BIN_SUB: return lhs - rhs;
                case BIN_MUL: return lhs * rhs;
                case BIN_DIV: return rhs == 0 ? 0 : lhs / rhs;
                case BIN_MOD: return rhs == 0 ? 0 : lhs % rhs;
                case BIN_LT: return lhs < rhs;
                case BIN_GT: return lhs > rhs;
                case BIN_LE: return lhs <= rhs;
                case BIN_GE: return lhs >= rhs;
                case BIN_EQ: return lhs == rhs;
                case BIN_NE: return lhs != rhs;
                case BIN_AND:
                case BIN_OR:
                    return 0;
            }
        }
        default:
            return 0;
    }
}

static int mem_eval_const_float_bits(MemIRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return float_bits_from_host(0.0f);
    }
    switch (expr->kind) {
        case EXPR_FLOAT_NUMBER:
            return expr->data.number;
        case EXPR_NUMBER:
            return float_bits_from_host((float)expr->data.number);
        case EXPR_LVAL: {
            MemSymbol *sym = mem_lookup_symbol(gen, expr->data.lval->name);
            int value = mem_eval_const_lval(gen, expr->data.lval);
            return sym != NULL && sym->value_type == TYPE_FLOAT ? value
                                                                 : float_bits_from_host((float)value);
        }
        case EXPR_UNARY: {
            float v = host_float_from_bits(mem_eval_const_float_bits(gen, expr->data.unary.operand));
            if (expr->data.unary.op == UNARY_MINUS) {
                return float_bits_from_host(-v);
            }
            if (expr->data.unary.op == UNARY_NOT) {
                return float_bits_from_host(!v);
            }
            return float_bits_from_host(v);
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                return float_bits_from_host((float)mem_eval_const_int(gen, expr));
            }
            float lhs = host_float_from_bits(mem_eval_const_float_bits(gen, expr->data.binary.lhs));
            float rhs = host_float_from_bits(mem_eval_const_float_bits(gen, expr->data.binary.rhs));
            switch (op) {
                case BIN_ADD: return float_bits_from_host(lhs + rhs);
                case BIN_SUB: return float_bits_from_host(lhs - rhs);
                case BIN_MUL: return float_bits_from_host(lhs * rhs);
                case BIN_DIV: return float_bits_from_host(lhs / rhs);
                case BIN_LT: return float_bits_from_host(lhs < rhs);
                case BIN_GT: return float_bits_from_host(lhs > rhs);
                case BIN_LE: return float_bits_from_host(lhs <= rhs);
                case BIN_GE: return float_bits_from_host(lhs >= rhs);
                case BIN_EQ: return float_bits_from_host(lhs == rhs);
                case BIN_NE: return float_bits_from_host(lhs != rhs);
                default: return float_bits_from_host(0.0f);
            }
        }
        default:
            return float_bits_from_host(0.0f);
    }
}

static IRInitializer *mem_new_initializer(IRInitializerKind kind, IRType *type) {
    IRInitializer *init = (IRInitializer *)xmalloc(sizeof(IRInitializer));
    memset(init, 0, sizeof(IRInitializer));
    init->kind = kind;
    init->type = type;
    return init;
}

static IRInitializer *mem_zero_initializer(IRType *type) {
    return mem_new_initializer(IR_INIT_ZERO, type);
}

static IRInitializer *mem_scalar_initializer(MemIRGen *gen, TypeSpec type, int value) {
    IRInitializer *init = mem_new_initializer(type == TYPE_FLOAT ? IR_INIT_FLOAT : IR_INIT_INT,
                                             mem_scalar_type(gen->module, type));
    if (type == TYPE_FLOAT) {
        init->data.float_bits = value;
    } else {
        init->data.int_value = value;
    }
    return init;
}

static IRInitializer *mem_array_initializer(MemIRGen *gen, TypeSpec elem_type, int *flat, int total) {
    IRType *array_type = mem_array_type(mem_scalar_type(gen->module, elem_type), total);
    IRInitializer *init = mem_new_initializer(IR_INIT_ARRAY, array_type);
    for (int i = 0; i < total; ++i) {
        ensure_capacity((void **)&init->data.array.items, &init->data.array.capacity,
                        sizeof(IRInitializer *), init->data.array.count + 1);
        init->data.array.items[init->data.array.count++] = mem_scalar_initializer(gen, elem_type, flat[i]);
    }
    return init;
}

static MemValue mem_gen_expr(MemIRGen *gen, Expr *expr);
static void mem_gen_cond(MemIRGen *gen, Expr *expr, IRBasicBlock *true_block, IRBasicBlock *false_block);
static void mem_gen_stmt(MemIRGen *gen, Stmt *stmt);
static void mem_gen_decl(MemIRGen *gen, Decl *decl, bool is_global);

static IRValue *mem_ensure_type(MemIRGen *gen, MemValue value, TypeSpec target) {
    if (value.type == target) {
        return value.value;
    }
    if (value.type == TYPE_INT && target == TYPE_FLOAT) {
        return mem_emit_cast(gen, IR_INST_SITOFP, value.value, gen->module->float_type);
    }
    if (value.type == TYPE_FLOAT && target == TYPE_INT) {
        return mem_emit_cast(gen, IR_INST_FPTOSI, value.value, gen->module->i32_type);
    }
    return value.value;
}

static IRValue *mem_ensure_i32(MemIRGen *gen, MemValue value) {
    return mem_ensure_type(gen, value, TYPE_INT);
}

static IRValue *mem_ensure_float(MemIRGen *gen, MemValue value) {
    return mem_ensure_type(gen, value, TYPE_FLOAT);
}

static IRValue *mem_emit_truth_i1(MemIRGen *gen, MemValue value) {
    if (value.value->type == gen->module->i1_type) {
        return value.value;
    }
    if (value.type == TYPE_FLOAT) {
        IRValue *raw = mem_ensure_float(gen, value);
        return mem_emit_fcmp(gen, IR_FCMP_ONE, raw, mem_const_float(gen, float_bits_from_host(0.0f)));
    }
    IRValue *raw = mem_ensure_i32(gen, value);
    return mem_emit_icmp(gen, IR_ICMP_NE, raw, mem_const_int(gen, 0));
}

static IRValue *mem_emit_compare_i32(MemIRGen *gen, IRIcmpPredicate pred, IRValue *lhs, IRValue *rhs) {
    IRValue *i1 = mem_emit_icmp(gen, pred, lhs, rhs);
    return mem_emit_cast(gen, IR_INST_ZEXT, i1, gen->module->i32_type);
}

static IRValue *mem_emit_compare_float(MemIRGen *gen, IRFcmpPredicate pred, IRValue *lhs, IRValue *rhs) {
    IRValue *i1 = mem_emit_fcmp(gen, pred, lhs, rhs);
    return mem_emit_cast(gen, IR_INST_ZEXT, i1, gen->module->i32_type);
}

static IRValue *mem_emit_linear_index(MemIRGen *gen, const int *dims, int dim_count, ExprList *indices) {
    IRValue *acc = mem_const_int(gen, 0);
    bool has_term = false;
    for (int i = 0; i < indices->count; ++i) {
        MemValue idx_v = mem_gen_expr(gen, indices->items[i]);
        IRValue *term = mem_ensure_i32(gen, idx_v);
        int stride = product_dims(dims, i + 1, dim_count);
        if (stride != 1) {
            term = mem_emit_binary(gen, IR_INST_MUL, gen->module->i32_type, term, mem_const_int(gen, stride));
        }
        if (!has_term) {
            acc = term;
            has_term = true;
        } else {
            acc = mem_emit_binary(gen, IR_INST_ADD, gen->module->i32_type, acc, term);
        }
    }
    return acc;
}

static MemAddress mem_flat_lval_address(MemIRGen *gen, MemSymbol *sym, ExprList *indices) {
    IRValueList gep_indices = {0};
    mem_value_list_push(&gep_indices, mem_const_int(gen, 0));
    mem_value_list_push(&gep_indices, mem_emit_linear_index(gen, sym->dims, sym->dim_count, indices));
    IRType *elem_type = mem_scalar_type(gen->module, sym->value_type);
    IRValue *ptr = mem_emit_gep(gen, sym->addr, sym->object_type, gep_indices, elem_type, true);
    int remain = sym->dim_count - indices->count;
    if (remain < 0) {
        remain = 0;
    }
    return mem_make_address(ptr, sym->value_type, remain, sym->dims + indices->count, false, elem_type);
}

static MemAddress mem_lval_address(MemIRGen *gen, LVal *lval) {
    MemSymbol *sym = mem_lookup_symbol(gen, lval->name);
    if (sym == NULL) {
        return mem_make_address(mem_const_zero(gen, mem_pointer_type(gen->module->i32_type)),
                                TYPE_INT, 0, NULL, false, gen->module->i32_type);
    }
    if (sym->is_flat_storage) {
        return mem_flat_lval_address(gen, sym, &lval->indices);
    }
    IRValue *ptr = sym->addr;
    int dim_count = sym->dim_count;
    int *dims = sym->dims;
    bool param_array = sym->is_param_array;
    IRType *current_type = sym->object_type != NULL ? sym->object_type : mem_scalar_type(gen->module, sym->value_type);
    for (int i = 0; i < lval->indices.count; ++i) {
        MemValue idx_v = mem_gen_expr(gen, lval->indices.items[i]);
        IRValue *idx = mem_ensure_i32(gen, idx_v);
        IRValueList gep_indices = {0};
        IRType *result_pointee = current_type;
        if (param_array && i == 0) {
            mem_value_list_push(&gep_indices, idx);
            if (dim_count == 0) {
                result_pointee = mem_scalar_type(gen->module, sym->value_type);
            }
            ptr = mem_emit_gep(gen, ptr, current_type, gep_indices, result_pointee, true);
            param_array = false;
        } else if (dim_count > 0) {
            mem_value_list_push(&gep_indices, mem_const_int(gen, 0));
            mem_value_list_push(&gep_indices, idx);
            result_pointee = dim_count == 1
                                  ? mem_scalar_type(gen->module, sym->value_type)
                                  : mem_array_type_from_dims(gen->module, sym->value_type, dims + 1, dim_count - 1);
            ptr = mem_emit_gep(gen, ptr, current_type, gep_indices, result_pointee, true);
            dims++;
            dim_count--;
            current_type = result_pointee;
        } else {
            mem_value_list_push(&gep_indices, idx);
            result_pointee = mem_scalar_type(gen->module, sym->value_type);
            ptr = mem_emit_gep(gen, ptr, current_type, gep_indices, result_pointee, true);
            current_type = result_pointee;
        }
    }
    return mem_make_address(ptr, sym->value_type, dim_count, dims, param_array, current_type);
}

static MemValue mem_lval_expr(MemIRGen *gen, LVal *lval) {
    MemSymbol *sym = mem_lookup_symbol(gen, lval->name);
    if (sym != NULL && sym->is_inline_value && lval->indices.count == 0) {
        return sym->inline_value;
    }
    if (sym != NULL && sym->is_const_scalar && lval->indices.count == 0) {
        if (sym->value_type == TYPE_FLOAT) {
            return mem_make_value(mem_const_float(gen, sym->const_scalar), TYPE_FLOAT, 0, NULL, false, false);
        }
        return mem_make_value(mem_const_int(gen, sym->const_scalar), TYPE_INT, 0, NULL, false, false);
    }
    if (sym != NULL && sym->is_param_array && lval->indices.count == 0) {
        return mem_make_value(sym->addr, sym->value_type, sym->dim_count, sym->dims, true, true);
    }
    MemAddress addr = mem_lval_address(gen, lval);
    if (addr.dim_count == 0) {
        IRValue *loaded = mem_emit_load(gen, mem_scalar_type(gen->module, addr.type), addr.ptr);
        return mem_make_value(loaded, addr.type, 0, NULL, false, false);
    }
    return mem_make_value(addr.ptr, addr.type, addr.dim_count, addr.dims, true, addr.is_param_array);
}

static IRValue *mem_array_arg_as(MemIRGen *gen, Expr *expr, Param *param) {
    MemValue arg = mem_gen_expr(gen, expr);
    IRType *expected_pointee = param->dims.count == 0
                                   ? mem_scalar_type(gen->module, param->type)
                                   : mem_array_type_from_dims(gen->module, param->type,
                                                              param->dims.data, param->dims.count);
    return mem_emit_cast(gen, IR_INST_BITCAST, arg.value, mem_pointer_type(expected_pointee));
}

static MemValue mem_short_circuit_value(MemIRGen *gen, Expr *expr) {
    IRValue *slot = mem_emit_alloca(gen, gen->module->i32_type);
    IRBasicBlock *true_block = mem_create_block(gen, mem_new_label(gen, "logic_true"));
    IRBasicBlock *false_block = mem_create_block(gen, mem_new_label(gen, "logic_false"));
    IRBasicBlock *end_block = mem_create_block(gen, mem_new_label(gen, "logic_end"));
    mem_gen_cond(gen, expr, true_block, false_block);
    mem_position_at(gen, true_block);
    mem_emit_store(gen, mem_const_int(gen, 1), slot);
    mem_emit_br(gen, NULL, end_block, NULL);
    mem_position_at(gen, false_block);
    mem_emit_store(gen, mem_const_int(gen, 0), slot);
    mem_emit_br(gen, NULL, end_block, NULL);
    mem_position_at(gen, end_block);
    IRValue *loaded = mem_emit_load(gen, gen->module->i32_type, slot);
    return mem_make_value(loaded, TYPE_INT, 0, NULL, false, false);
}

static Expr *mem_simple_return_expr(FuncDef *func) {
    if (func == NULL || func->ret_type == TYPE_VOID || func->block == NULL ||
        func->block->items.count != 1 ||
        func->block->items.kinds[0] != BLOCK_ITEM_STMT) {
        return NULL;
    }
    Stmt *stmt = (Stmt *)func->block->items.items[0];
    if (stmt == NULL || stmt->kind != STMT_RETURN || stmt->data.return_expr == NULL) {
        return NULL;
    }
    return stmt->data.return_expr;
}

static Expr *mem_stmt_direct_return_expr(Stmt *stmt) {
    if (stmt == NULL) {
        return NULL;
    }
    if (stmt->kind == STMT_RETURN) {
        return stmt->data.return_expr;
    }
    if (stmt->kind == STMT_BLOCK &&
        stmt->data.block_stmt != NULL &&
        stmt->data.block_stmt->items.count == 1 &&
        stmt->data.block_stmt->items.kinds[0] == BLOCK_ITEM_STMT) {
        return mem_stmt_direct_return_expr((Stmt *)stmt->data.block_stmt->items.items[0]);
    }
    return NULL;
}

static Stmt *mem_simple_if_return_stmt(FuncDef *func) {
    if (func == NULL || func->ret_type == TYPE_VOID || func->block == NULL ||
        func->block->items.count != 1 ||
        func->block->items.kinds[0] != BLOCK_ITEM_STMT) {
        return NULL;
    }
    Stmt *stmt = (Stmt *)func->block->items.items[0];
    if (stmt == NULL || stmt->kind != STMT_IF || stmt->data.if_stmt.else_stmt == NULL ||
        mem_stmt_direct_return_expr(stmt->data.if_stmt.then_stmt) == NULL ||
        mem_stmt_direct_return_expr(stmt->data.if_stmt.else_stmt) == NULL) {
        return NULL;
    }
    return stmt;
}

static bool mem_decl_has_only_scalar_items(Decl *decl) {
    if (decl == NULL) {
        return false;
    }
    for (int i = 0; i < decl->items.count; ++i) {
        DeclItem *item = decl->items.items[i];
        if (item == NULL || item->dims.count != 0) {
            return false;
        }
    }
    return true;
}

static Expr *mem_straightline_return_expr(FuncDef *func) {
    if (func == NULL || func->ret_type == TYPE_VOID || func->block == NULL ||
        func->block->items.count < 2 || func->block->items.count > 8) {
        return NULL;
    }
    int last = func->block->items.count - 1;
    for (int i = 0; i < last; ++i) {
        if (func->block->items.kinds[i] != BLOCK_ITEM_DECL ||
            !mem_decl_has_only_scalar_items((Decl *)func->block->items.items[i])) {
            return NULL;
        }
    }
    if (func->block->items.kinds[last] != BLOCK_ITEM_STMT) {
        return NULL;
    }
    return mem_stmt_direct_return_expr((Stmt *)func->block->items.items[last]);
}

typedef enum {
    MEM_INLINE_NONE,
    MEM_INLINE_RETURN_EXPR,
    MEM_INLINE_IF_RETURN
} MemInlineKind;

typedef struct {
    MemInlineKind kind;
    Expr *return_expr;
    Stmt *if_stmt;
    int prefix_count;
    int cost;
} MemInlinePlan;

static bool mem_func_has_scalar_param_named(FuncDef *func, const char *name) {
    if (func == NULL || name == NULL) {
        return false;
    }
    for (int i = 0; i < func->params.count; ++i) {
        Param *param = func->params.items[i];
        if (param != NULL && !param->is_array && strcmp(param->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static int mem_expr_inline_cost(Expr *expr);

static int mem_lval_inline_cost(LVal *lval) {
    if (lval == NULL) {
        return -1;
    }
    int cost = 1;
    for (int i = 0; i < lval->indices.count; ++i) {
        int index_cost = mem_expr_inline_cost(lval->indices.items[i]);
        if (index_cost < 0) {
            return -1;
        }
        cost += index_cost;
    }
    return cost;
}

static int mem_expr_inline_cost(Expr *expr) {
    if (expr == NULL) {
        return 0;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return 1;
        case EXPR_LVAL:
            return mem_lval_inline_cost(expr->data.lval);
        case EXPR_UNARY: {
            int inner = mem_expr_inline_cost(expr->data.unary.operand);
            return inner < 0 ? -1 : inner + 1;
        }
        case EXPR_BINARY: {
            int lhs = mem_expr_inline_cost(expr->data.binary.lhs);
            int rhs = mem_expr_inline_cost(expr->data.binary.rhs);
            return lhs < 0 || rhs < 0 ? -1 : lhs + rhs + 1;
        }
        case EXPR_CALL:
        case EXPR_GETINT:
        default:
            return -1;
    }
}

static int mem_initval_inline_cost(InitVal *init) {
    if (init == NULL) {
        return 0;
    }
    if (!init->is_expr) {
        return -1;
    }
    return mem_expr_inline_cost(init->expr);
}

static int mem_decl_inline_cost(Decl *decl) {
    if (!mem_decl_has_only_scalar_items(decl)) {
        return -1;
    }
    int cost = 1;
    for (int i = 0; i < decl->items.count; ++i) {
        int init_cost = mem_initval_inline_cost(decl->items.items[i]->init);
        if (init_cost < 0) {
            return -1;
        }
        cost += init_cost + 1;
    }
    return cost;
}

static int mem_stmt_inline_prefix_cost(FuncDef *func, Stmt *stmt) {
    if (stmt == NULL) {
        return -1;
    }
    switch (stmt->kind) {
        case STMT_ASSIGN: {
            if (stmt->data.assign_stmt.lval == NULL) {
                return -1;
            }
            if (stmt->data.assign_stmt.lval->indices.count == 0
                    && mem_func_has_scalar_param_named(func, stmt->data.assign_stmt.lval->name)) {
                return -1;
            }
            int lhs = mem_lval_inline_cost(stmt->data.assign_stmt.lval);
            int rhs = mem_expr_inline_cost(stmt->data.assign_stmt.expr);
            return lhs < 0 || rhs < 0 ? -1 : lhs + rhs + 1;
        }
        case STMT_EXPR: {
            int expr_cost = mem_expr_inline_cost(stmt->data.expr_stmt);
            return expr_cost < 0 ? -1 : expr_cost + 1;
        }
        case STMT_BLOCK: {
            if (stmt->data.block_stmt == NULL) {
                return 1;
            }
            int cost = 1;
            for (int i = 0; i < stmt->data.block_stmt->items.count; ++i) {
                int item_cost = -1;
                if (stmt->data.block_stmt->items.kinds[i] == BLOCK_ITEM_DECL) {
                    item_cost = mem_decl_inline_cost((Decl *)stmt->data.block_stmt->items.items[i]);
                } else if (stmt->data.block_stmt->items.kinds[i] == BLOCK_ITEM_STMT) {
                    item_cost = mem_stmt_inline_prefix_cost(func, (Stmt *)stmt->data.block_stmt->items.items[i]);
                }
                if (item_cost < 0) {
                    return -1;
                }
                cost += item_cost;
            }
            return cost;
        }
        case STMT_IF: {
            int cond_cost = mem_expr_inline_cost(stmt->data.if_stmt.cond);
            int then_cost = mem_stmt_inline_prefix_cost(func, stmt->data.if_stmt.then_stmt);
            int else_cost = stmt->data.if_stmt.else_stmt != NULL
                                ? mem_stmt_inline_prefix_cost(func, stmt->data.if_stmt.else_stmt)
                                : 0;
            return cond_cost < 0 || then_cost < 0 || else_cost < 0
                       ? -1
                       : cond_cost + then_cost + else_cost + 2;
        }
        case STMT_WHILE:
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_RETURN:
        case STMT_PRINTF:
        default:
            return -1;
    }
}

static int mem_if_return_inline_cost(Stmt *stmt) {
    if (stmt == NULL || stmt->kind != STMT_IF || stmt->data.if_stmt.else_stmt == NULL) {
        return -1;
    }
    Expr *then_expr = mem_stmt_direct_return_expr(stmt->data.if_stmt.then_stmt);
    Expr *else_expr = mem_stmt_direct_return_expr(stmt->data.if_stmt.else_stmt);
    if (then_expr == NULL || else_expr == NULL) {
        return -1;
    }
    int cond_cost = mem_expr_inline_cost(stmt->data.if_stmt.cond);
    int then_cost = mem_expr_inline_cost(then_expr);
    int else_cost = mem_expr_inline_cost(else_expr);
    return cond_cost < 0 || then_cost < 0 || else_cost < 0
               ? -1
               : cond_cost + then_cost + else_cost + 2;
}

static bool mem_build_inline_plan(FuncDef *func, MemInlinePlan *plan) {
    if (plan == NULL) {
        return false;
    }
    memset(plan, 0, sizeof(MemInlinePlan));
    plan->kind = MEM_INLINE_NONE;
    if (func == NULL || func->ret_type == TYPE_VOID || func->block == NULL
            || func->block->items.count <= 0 || func->block->items.count > 12) {
        return false;
    }
    int last = func->block->items.count - 1;
    int total_cost = 0;
    for (int i = 0; i < last; ++i) {
        int item_cost = -1;
        if (func->block->items.kinds[i] == BLOCK_ITEM_DECL) {
            item_cost = mem_decl_inline_cost((Decl *)func->block->items.items[i]);
        } else if (func->block->items.kinds[i] == BLOCK_ITEM_STMT) {
            item_cost = mem_stmt_inline_prefix_cost(func, (Stmt *)func->block->items.items[i]);
        }
        if (item_cost < 0) {
            return false;
        }
        total_cost += item_cost;
    }
    if (func->block->items.kinds[last] != BLOCK_ITEM_STMT) {
        return false;
    }
    Stmt *tail_stmt = (Stmt *)func->block->items.items[last];
    Expr *tail_return = mem_stmt_direct_return_expr(tail_stmt);
    if (tail_return != NULL) {
        int tail_cost = mem_expr_inline_cost(tail_return);
        if (tail_cost < 0) {
            return false;
        }
        total_cost += tail_cost;
        plan->kind = MEM_INLINE_RETURN_EXPR;
        plan->return_expr = tail_return;
    } else {
        int if_cost = mem_if_return_inline_cost(tail_stmt);
        if (if_cost < 0) {
            return false;
        }
        total_cost += if_cost;
        plan->kind = MEM_INLINE_IF_RETURN;
        plan->if_stmt = tail_stmt;
    }
    if (total_cost > 32) {
        return false;
    }
    plan->prefix_count = last;
    plan->cost = total_cost;
    return true;
}

static bool mem_can_inline_call(MemIRGen *gen, MemFunctionMeta *meta, ExprList *args) {
    MemInlinePlan plan;
    if (gen == NULL || meta == NULL || meta->ast_func == NULL || meta->function == NULL ||
        meta->function->is_external || meta->ret_type == TYPE_VOID ||
        meta->ast_func == NULL || args->count != meta->params.count ||
        gen->inline_depth >= 16 || meta->params.count > 8) {
        return false;
    }
    if (gen->current_function == meta->function) {
        return false;
    }
    if (mem_simple_return_expr(meta->ast_func) != NULL ||
            mem_simple_if_return_stmt(meta->ast_func) != NULL ||
            mem_straightline_return_expr(meta->ast_func) != NULL) {
        return true;
    }
    return mem_build_inline_plan(meta->ast_func, &plan);
}

static MemValue mem_inline_if_return_value(MemIRGen *gen, MemFunctionMeta *meta, Stmt *stmt) {
    IRType *ret_type = mem_scalar_type(gen->module, meta->ret_type);
    IRValue *slot = mem_emit_alloca(gen, ret_type);
    IRBasicBlock *then_block = mem_create_block(gen, mem_new_label(gen, "inline_then"));
    IRBasicBlock *else_block = mem_create_block(gen, mem_new_label(gen, "inline_else"));
    IRBasicBlock *end_block = mem_create_block(gen, mem_new_label(gen, "inline_end"));
    mem_gen_cond(gen, stmt->data.if_stmt.cond, then_block, else_block);
    mem_position_at(gen, then_block);
    MemValue then_value = mem_gen_expr(gen, mem_stmt_direct_return_expr(stmt->data.if_stmt.then_stmt));
    mem_emit_store(gen, mem_ensure_type(gen, then_value, meta->ret_type), slot);
    mem_emit_br(gen, NULL, end_block, NULL);
    mem_position_at(gen, else_block);
    MemValue else_value = mem_gen_expr(gen, mem_stmt_direct_return_expr(stmt->data.if_stmt.else_stmt));
    mem_emit_store(gen, mem_ensure_type(gen, else_value, meta->ret_type), slot);
    mem_emit_br(gen, NULL, end_block, NULL);
    mem_position_at(gen, end_block);
    IRValue *loaded = mem_emit_load(gen, ret_type, slot);
    return mem_make_value(loaded, meta->ret_type, 0, NULL, false, false);
}

static MemValue mem_try_inline_call(MemIRGen *gen, MemFunctionMeta *meta, ExprList *args, bool *inlined) {
    *inlined = false;
    if (!mem_can_inline_call(gen, meta, args)) {
        return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
    }
    MemInlinePlan plan = {0};
    bool have_plan = mem_build_inline_plan(meta->ast_func, &plan);
    MemValue *arg_values = NULL;
    if (args->count > 0) {
        arg_values = (MemValue *)xmalloc(sizeof(MemValue) * args->count);
    }
    for (int i = 0; i < args->count; ++i) {
        Param *param = meta->params.items[i];
        if (param->is_array) {
            IRValue *ptr = mem_array_arg_as(gen, args->items[i], param);
            int dim_count = param->dims.count;
            int *dims = copy_dims(param->dims.data, dim_count);
            arg_values[i] = mem_make_value(ptr, param->type, dim_count, dims, true, true);
        } else {
            MemValue raw = mem_gen_expr(gen, args->items[i]);
            IRValue *value = mem_ensure_type(gen, raw, param->type);
            arg_values[i] = mem_make_value(value, param->type, 0, NULL, false, false);
        }
    }

    mem_push_scope(gen);
    for (int i = 0; i < meta->params.count; ++i) {
        Param *param = meta->params.items[i];
        MemSymbol *sym = mem_add_symbol(gen, param->name);
        sym->value_type = param->type;
        sym->dim_count = param->dims.count;
        sym->dims = copy_dims(param->dims.data, param->dims.count);
        sym->total_slots = 1;
        sym->is_param_array = param->is_array;
        sym->is_inline_value = true;
        sym->inline_value = arg_values[i];
        if (param->is_array) {
            sym->addr = arg_values[i].value;
            sym->object_type = arg_values[i].value != NULL && arg_values[i].value->type != NULL &&
                               arg_values[i].value->type->kind == IR_TYPE_POINTER
                                   ? arg_values[i].value->type->data.pointer.pointee
                                   : mem_scalar_type(gen->module, param->type);
        }
    }
    gen->inline_depth++;
    Expr *return_expr = mem_simple_return_expr(meta->ast_func);
    Stmt *if_return = mem_simple_if_return_stmt(meta->ast_func);
    Expr *straightline_return = mem_straightline_return_expr(meta->ast_func);
    MemValue result;
    if (return_expr != NULL) {
        result = mem_gen_expr(gen, return_expr);
    } else if (if_return != NULL) {
        result = mem_inline_if_return_value(gen, meta, if_return);
    } else if (straightline_return != NULL) {
        int last = meta->ast_func->block->items.count - 1;
        for (int i = 0; i < last; ++i) {
            mem_gen_decl(gen, (Decl *)meta->ast_func->block->items.items[i], false);
        }
        result = mem_gen_expr(gen, straightline_return);
    } else if (have_plan && plan.kind != MEM_INLINE_NONE) {
        for (int i = 0; i < plan.prefix_count; ++i) {
            if (meta->ast_func->block->items.kinds[i] == BLOCK_ITEM_DECL) {
                mem_gen_decl(gen, (Decl *)meta->ast_func->block->items.items[i], false);
            } else if (meta->ast_func->block->items.kinds[i] == BLOCK_ITEM_STMT) {
                mem_gen_stmt(gen, (Stmt *)meta->ast_func->block->items.items[i]);
            }
        }
        if (plan.kind == MEM_INLINE_IF_RETURN) {
            result = mem_inline_if_return_value(gen, meta, plan.if_stmt);
        } else {
            result = mem_gen_expr(gen, plan.return_expr);
        }
    } else {
        gen->inline_depth--;
        mem_pop_scope(gen);
        free(arg_values);
        return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
    }
    gen->inline_depth--;
    mem_pop_scope(gen);
    free(arg_values);
    *inlined = true;
    return result;
}

static MemValue mem_gen_call(MemIRGen *gen, const char *name, ExprList *args) {
    const char *callee_name = name;
    if (strcmp(name, "starttime") == 0) {
        callee_name = "_sysy_starttime";
    } else if (strcmp(name, "stoptime") == 0) {
        callee_name = "_sysy_stoptime";
    }
    MemFunctionMeta *meta = mem_lookup_function(gen, callee_name);
    bool inlined = false;
    MemValue inline_result = mem_try_inline_call(gen, meta, args, &inlined);
    if (inlined) {
        return mem_make_value(mem_ensure_type(gen, inline_result,
                                              meta != NULL ? meta->ret_type : inline_result.type),
                              meta != NULL ? meta->ret_type : inline_result.type,
                              0, NULL, false, false);
    }
    IRValueList ir_args = {0};
    if (meta != NULL && meta->has_sysy_params) {
        for (int i = 0; i < args->count; ++i) {
            Param *param = i < meta->params.count ? meta->params.items[i] : NULL;
            if (param != NULL && param->is_array) {
                mem_value_list_push(&ir_args, mem_array_arg_as(gen, args->items[i], param));
            } else {
                MemValue arg = mem_gen_expr(gen, args->items[i]);
                mem_value_list_push(&ir_args, mem_ensure_type(gen, arg, param != NULL ? param->type : TYPE_INT));
            }
        }
    } else {
        for (int i = 0; i < args->count; ++i) {
            MemValue arg = mem_gen_expr(gen, args->items[i]);
            bool raw_array_arg =
                ((strcmp(callee_name, "getarray") == 0 || strcmp(callee_name, "getfarray") == 0) && i == 0) ||
                ((strcmp(callee_name, "putarray") == 0 || strcmp(callee_name, "putfarray") == 0) && i == 1);
            if (raw_array_arg) {
                mem_value_list_push(&ir_args, arg.value);
            } else if (strcmp(callee_name, "putfloat") == 0) {
                mem_value_list_push(&ir_args, mem_ensure_float(gen, arg));
            } else {
                mem_value_list_push(&ir_args, mem_ensure_i32(gen, arg));
            }
        }
        if ((strcmp(callee_name, "_sysy_starttime") == 0 ||
             strcmp(callee_name, "_sysy_stoptime") == 0) &&
            ir_args.count == 0) {
            mem_value_list_push(&ir_args, mem_const_int(gen, 0));
        }
    }
    IRType *ret_type = meta != NULL ? meta->function->ret_type : gen->module->i32_type;
    IRFunction *callee = meta != NULL ? meta->function : NULL;
    IRValue *result = mem_emit_call(gen, callee, ret_type, ir_args);
    TypeSpec sysy_ret = meta != NULL ? meta->ret_type : TYPE_INT;
    if (sysy_ret == TYPE_VOID) {
        return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
    }
    return mem_make_value(result, sysy_ret, 0, NULL, false, false);
}

static MemValue mem_gen_expr(MemIRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return mem_make_value(mem_const_int(gen, expr->data.number), TYPE_INT, 0, NULL, false, false);
        case EXPR_FLOAT_NUMBER:
            return mem_make_value(mem_const_float(gen, expr->data.number), TYPE_FLOAT, 0, NULL, false, false);
        case EXPR_LVAL:
            return mem_lval_expr(gen, expr->data.lval);
        case EXPR_GETINT:
            return mem_gen_call(gen, "getint", &(ExprList){0});
        case EXPR_CALL:
            return mem_gen_call(gen, expr->data.call.name, &expr->data.call.args);
        case EXPR_UNARY: {
            MemValue operand = mem_gen_expr(gen, expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                return operand;
            }
            if (expr->data.unary.op == UNARY_MINUS) {
                if (operand.type == TYPE_FLOAT) {
                    IRValue *zero = mem_const_float(gen, float_bits_from_host(0.0f));
                    IRValue *raw = mem_ensure_float(gen, operand);
                    return mem_make_value(mem_emit_binary(gen, IR_INST_FSUB, gen->module->float_type, zero, raw),
                                          TYPE_FLOAT, 0, NULL, false, false);
                }
                IRValue *raw = mem_ensure_i32(gen, operand);
                return mem_make_value(mem_emit_binary(gen, IR_INST_SUB, gen->module->i32_type,
                                                      mem_const_int(gen, 0), raw),
                                      TYPE_INT, 0, NULL, false, false);
            }
            IRValue *truth = mem_emit_truth_i1(gen, operand);
            IRValue *not_i1 = mem_emit_icmp(gen, IR_ICMP_EQ, truth, mem_const_i1(gen, 0));
            return mem_make_value(mem_emit_cast(gen, IR_INST_ZEXT, not_i1, gen->module->i32_type),
                                  TYPE_INT, 0, NULL, false, false);
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                return mem_short_circuit_value(gen, expr);
            }
            MemValue lhs_v = mem_gen_expr(gen, expr->data.binary.lhs);
            MemValue rhs_v = mem_gen_expr(gen, expr->data.binary.rhs);
            bool use_float = (lhs_v.type == TYPE_FLOAT || rhs_v.type == TYPE_FLOAT) && op != BIN_MOD;
            if (use_float) {
                IRValue *lhs = mem_ensure_float(gen, lhs_v);
                IRValue *rhs = mem_ensure_float(gen, rhs_v);
                switch (op) {
                    case BIN_ADD: return mem_make_value(mem_emit_binary(gen, IR_INST_FADD, gen->module->float_type, lhs, rhs), TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_SUB: return mem_make_value(mem_emit_binary(gen, IR_INST_FSUB, gen->module->float_type, lhs, rhs), TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_MUL: return mem_make_value(mem_emit_binary(gen, IR_INST_FMUL, gen->module->float_type, lhs, rhs), TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_DIV: return mem_make_value(mem_emit_binary(gen, IR_INST_FDIV, gen->module->float_type, lhs, rhs), TYPE_FLOAT, 0, NULL, false, false);
                    case BIN_LT: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_OLT, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_GT: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_OGT, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_LE: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_OLE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_GE: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_OGE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_EQ: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_OEQ, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    case BIN_NE: return mem_make_value(mem_emit_compare_float(gen, IR_FCMP_ONE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                    default: return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
                }
            }
            IRValue *lhs = mem_ensure_i32(gen, lhs_v);
            IRValue *rhs = mem_ensure_i32(gen, rhs_v);
            switch (op) {
                case BIN_ADD: return mem_make_value(mem_emit_binary(gen, IR_INST_ADD, gen->module->i32_type, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_SUB: return mem_make_value(mem_emit_binary(gen, IR_INST_SUB, gen->module->i32_type, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_MUL: return mem_make_value(mem_emit_binary(gen, IR_INST_MUL, gen->module->i32_type, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_DIV: return mem_make_value(mem_emit_binary(gen, IR_INST_SDIV, gen->module->i32_type, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_MOD: return mem_make_value(mem_emit_binary(gen, IR_INST_SREM, gen->module->i32_type, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_LT: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_SLT, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_GT: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_SGT, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_LE: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_SLE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_GE: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_SGE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_EQ: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_EQ, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                case BIN_NE: return mem_make_value(mem_emit_compare_i32(gen, IR_ICMP_NE, lhs, rhs), TYPE_INT, 0, NULL, false, false);
                default: return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
            }
        }
    }
    return mem_make_value(mem_const_int(gen, 0), TYPE_INT, 0, NULL, false, false);
}

static void mem_gen_cond(MemIRGen *gen, Expr *expr, IRBasicBlock *true_block, IRBasicBlock *false_block) {
    if (expr != NULL && expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_OR) {
        IRBasicBlock *rhs_block = mem_create_block(gen, mem_new_label(gen, "lor_rhs"));
        mem_gen_cond(gen, expr->data.binary.lhs, true_block, rhs_block);
        mem_position_at(gen, rhs_block);
        mem_gen_cond(gen, expr->data.binary.rhs, true_block, false_block);
        return;
    }
    if (expr != NULL && expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_AND) {
        IRBasicBlock *rhs_block = mem_create_block(gen, mem_new_label(gen, "land_rhs"));
        mem_gen_cond(gen, expr->data.binary.lhs, rhs_block, false_block);
        mem_position_at(gen, rhs_block);
        mem_gen_cond(gen, expr->data.binary.rhs, true_block, false_block);
        return;
    }
    MemValue value = mem_gen_expr(gen, expr);
    IRValue *truth = mem_emit_truth_i1(gen, value);
    mem_emit_br(gen, truth, true_block, false_block);
}

static void mem_gen_printf(MemIRGen *gen, char *format, ExprList *args) {
    int arg_index = 0;
    int len = (int)strlen(format);
    for (int i = 1; i < len - 1; ++i) {
        ExprList call_args = {0};
        if (format[i] == '%' && i + 1 < len - 1) {
            char spec = format[++i];
            if ((spec == 'd' || spec == 'c') && arg_index < args->count) {
                expr_list_push(&call_args, args->items[arg_index++]);
                mem_gen_call(gen, spec == 'd' ? "putint" : "putch", &call_args);
            } else if ((spec == 'f' || spec == 'a') && arg_index < args->count) {
                expr_list_push(&call_args, args->items[arg_index++]);
                mem_gen_call(gen, "putfloat", &call_args);
            } else {
                Expr literal;
                memset(&literal, 0, sizeof(literal));
                literal.kind = EXPR_NUMBER;
                literal.data.number = spec == '%' ? '%' : (unsigned char)spec;
                expr_list_push(&call_args, &literal);
                mem_gen_call(gen, "putch", &call_args);
            }
        } else {
            int ch = (unsigned char)format[i];
            if (format[i] == '\\' && i + 1 < len - 1) {
                ch = format[i + 1] == 'n' ? 10 : (format[i + 1] == 't' ? 9 : (unsigned char)format[i + 1]);
                ++i;
            }
            Expr literal;
            memset(&literal, 0, sizeof(literal));
            literal.kind = EXPR_NUMBER;
            literal.data.number = ch;
            expr_list_push(&call_args, &literal);
            mem_gen_call(gen, "putch", &call_args);
        }
    }
}

static void mem_gen_block(MemIRGen *gen, Block *block, bool new_scope) {
    if (new_scope) {
        mem_push_scope(gen);
    }
    for (int i = 0; i < block->items.count; ++i) {
        if (block->items.kinds[i] == BLOCK_ITEM_DECL) {
            mem_gen_decl(gen, (Decl *)block->items.items[i], false);
        } else {
            mem_gen_stmt(gen, (Stmt *)block->items.items[i]);
        }
    }
    if (new_scope) {
        mem_pop_scope(gen);
    }
}

static void mem_gen_stmt(MemIRGen *gen, Stmt *stmt) {
    if (stmt == NULL || mem_block_terminated(gen)) {
        return;
    }
    switch (stmt->kind) {
        case STMT_ASSIGN: {
            MemAddress addr = mem_lval_address(gen, stmt->data.assign_stmt.lval);
            MemValue value = mem_gen_expr(gen, stmt->data.assign_stmt.expr);
            mem_emit_store(gen, mem_ensure_type(gen, value, addr.type), addr.ptr);
            return;
        }
        case STMT_EXPR:
            if (stmt->data.expr_stmt != NULL) {
                mem_gen_expr(gen, stmt->data.expr_stmt);
            }
            return;
        case STMT_BLOCK:
            mem_gen_block(gen, stmt->data.block_stmt, true);
            return;
        case STMT_IF: {
            IRBasicBlock *then_block = mem_create_block(gen, mem_new_label(gen, "if_then"));
            IRBasicBlock *else_block = stmt->data.if_stmt.else_stmt != NULL
                                           ? mem_create_block(gen, mem_new_label(gen, "if_else"))
                                           : NULL;
            IRBasicBlock *end_block = mem_create_block(gen, mem_new_label(gen, "if_end"));
            mem_gen_cond(gen, stmt->data.if_stmt.cond, then_block,
                         else_block != NULL ? else_block : end_block);
            mem_position_at(gen, then_block);
            mem_gen_stmt(gen, stmt->data.if_stmt.then_stmt);
            if (!mem_block_terminated(gen)) {
                mem_emit_br(gen, NULL, end_block, NULL);
            }
            if (else_block != NULL) {
                mem_position_at(gen, else_block);
                mem_gen_stmt(gen, stmt->data.if_stmt.else_stmt);
                if (!mem_block_terminated(gen)) {
                    mem_emit_br(gen, NULL, end_block, NULL);
                }
            }
            mem_position_at(gen, end_block);
            return;
        }
        case STMT_WHILE: {
            IRBasicBlock *cond_block = mem_create_block(gen, mem_new_label(gen, "while_cond"));
            IRBasicBlock *body_block = mem_create_block(gen, mem_new_label(gen, "while_body"));
            IRBasicBlock *end_block = mem_create_block(gen, mem_new_label(gen, "while_end"));
            mem_emit_br(gen, NULL, cond_block, NULL);
            mem_position_at(gen, cond_block);
            mem_loop_push(gen, end_block, cond_block);
            mem_gen_cond(gen, stmt->data.while_stmt.cond, body_block, end_block);
            mem_position_at(gen, body_block);
            mem_gen_stmt(gen, stmt->data.while_stmt.body);
            if (!mem_block_terminated(gen)) {
                mem_emit_br(gen, NULL, cond_block, NULL);
            }
            mem_loop_pop(gen);
            mem_position_at(gen, end_block);
            return;
        }
        case STMT_BREAK:
            if (gen->break_count > 0) {
                mem_emit_br(gen, NULL, gen->break_blocks[gen->break_count - 1], NULL);
            }
            return;
        case STMT_CONTINUE:
            if (gen->continue_count > 0) {
                mem_emit_br(gen, NULL, gen->continue_blocks[gen->continue_count - 1], NULL);
            }
            return;
        case STMT_RETURN:
            if (stmt->data.return_expr == NULL) {
                mem_emit_ret(gen, NULL);
            } else {
                MemValue value = mem_gen_expr(gen, stmt->data.return_expr);
                mem_emit_ret(gen, mem_ensure_type(gen, value, gen->current_ret_type));
            }
            return;
        case STMT_PRINTF:
            mem_gen_printf(gen, stmt->data.printf_stmt.format, &stmt->data.printf_stmt.args);
            return;
    }
}

static int *mem_init_to_flat(MemIRGen *gen, InitVal *init, const int *dims, int dim_count, TypeSpec type) {
    int total = object_slot_count(dims, dim_count);
    int *flat = (int *)xmalloc(sizeof(int) * (size_t)total);
    for (int i = 0; i < total; ++i) {
        flat[i] = type == TYPE_FLOAT ? float_bits_from_host(0.0f) : 0;
    }
    if (init == NULL) {
        return flat;
    }
    Expr **slots = init_to_expr_slots(init, dims, dim_count, total);
    for (int i = 0; i < total; ++i) {
        if (slots[i] != NULL) {
            flat[i] = type == TYPE_FLOAT ? mem_eval_const_float_bits(gen, slots[i])
                                         : mem_eval_const_int(gen, slots[i]);
        }
    }
    free(slots);
    return flat;
}

static void mem_zero_local_array(MemIRGen *gen, MemSymbol *sym) {
    if (sym->total_slots <= 0) {
        return;
    }
    MemFunctionMeta *memset_meta = mem_lookup_function(gen, "memset");
    if (memset_meta != NULL) {
        IRValueList args = {0};
        mem_value_list_push(&args, sym->addr);
        mem_value_list_push(&args, mem_const_int(gen, 0));
        mem_value_list_push(&args, mem_const_int(gen, sym->total_slots * 4));
        mem_emit_call(gen, memset_meta->function, memset_meta->function->ret_type, args);
        return;
    }
    IRValue *index_slot = mem_emit_alloca(gen, gen->module->i32_type);
    mem_emit_store(gen, mem_const_int(gen, 0), index_slot);
    IRBasicBlock *cond_block = mem_create_block(gen, mem_new_label(gen, "zero_cond"));
    IRBasicBlock *body_block = mem_create_block(gen, mem_new_label(gen, "zero_body"));
    IRBasicBlock *end_block = mem_create_block(gen, mem_new_label(gen, "zero_end"));
    mem_emit_br(gen, NULL, cond_block, NULL);
    mem_position_at(gen, cond_block);
    IRValue *index = mem_emit_load(gen, gen->module->i32_type, index_slot);
    IRValue *in_range = mem_emit_icmp(gen, IR_ICMP_SLT, index, mem_const_int(gen, sym->total_slots));
    mem_emit_br(gen, in_range, body_block, end_block);
    mem_position_at(gen, body_block);
    IRValueList indices = {0};
    mem_value_list_push(&indices, mem_const_int(gen, 0));
    mem_value_list_push(&indices, index);
    IRValue *ptr = mem_emit_gep(gen, sym->addr, sym->object_type, indices,
                                mem_scalar_type(gen->module, sym->value_type), true);
    mem_emit_store(gen, mem_const_zero(gen, mem_scalar_type(gen->module, sym->value_type)), ptr);
    IRValue *next = mem_emit_binary(gen, IR_INST_ADD, gen->module->i32_type, index, mem_const_int(gen, 1));
    mem_emit_store(gen, next, index_slot);
    mem_emit_br(gen, NULL, cond_block, NULL);
    mem_position_at(gen, end_block);
}

static void mem_init_local_array(MemIRGen *gen, MemSymbol *sym, InitVal *init) {
    mem_zero_local_array(gen, sym);
    if (init == NULL) {
        return;
    }
    Expr **slots = init_to_expr_slots(init, sym->dims, sym->dim_count, sym->total_slots);
    for (int i = 0; i < sym->total_slots; ++i) {
        if (slots[i] == NULL) {
            continue;
        }
        IRValueList indices = {0};
        mem_value_list_push(&indices, mem_const_int(gen, 0));
        mem_value_list_push(&indices, mem_const_int(gen, i));
        IRValue *ptr = mem_emit_gep(gen, sym->addr, sym->object_type, indices,
                                    mem_scalar_type(gen->module, sym->value_type), true);
        MemValue value = mem_gen_expr(gen, slots[i]);
        mem_emit_store(gen, mem_ensure_type(gen, value, sym->value_type), ptr);
    }
    free(slots);
}

static void mem_gen_decl(MemIRGen *gen, Decl *decl, bool is_global) {
    for (int i = 0; i < decl->items.count; ++i) {
        DeclItem *item = decl->items.items[i];
        MemSymbol *sym = mem_add_symbol(gen, item->name);
        sym->value_type = decl->type;
        sym->dim_count = item->dims.count;
        sym->dims = copy_dims(item->dims.data, item->dims.count);
        sym->total_slots = object_slot_count(item->dims.data, item->dims.count);
        sym->is_const = decl->is_const;
        sym->is_global = is_global;
        sym->is_flat_storage = item->dims.count > 0;
        if (decl->is_const && item->init != NULL && item->dims.count == 0) {
            sym->is_const_scalar = true;
            sym->const_scalar = decl->type == TYPE_FLOAT
                                    ? mem_eval_const_float_bits(gen, item->init->expr)
                                    : mem_eval_const_int(gen, item->init->expr);
        }
        if (decl->is_const && item->init != NULL && item->dims.count > 0) {
            sym->const_flat = mem_init_to_flat(gen, item->init, item->dims.data, item->dims.count, decl->type);
        }
        IRType *object_type = item->dims.count == 0
                                  ? mem_scalar_type(gen->module, decl->type)
                                  : mem_array_type(mem_scalar_type(gen->module, decl->type), sym->total_slots);
        sym->object_type = object_type;
        if (is_global) {
            IRGlobal *global = (IRGlobal *)xmalloc(sizeof(IRGlobal));
            memset(global, 0, sizeof(IRGlobal));
            global->name = str_printf("@g%d", gen->global_id++);
            global->type = object_type;
            global->elem_type = decl->type;
            global->is_const = decl->is_const;
            if (item->dims.count == 0) {
                int init_value = 0;
                if (item->init != NULL) {
                    init_value = decl->type == TYPE_FLOAT
                                     ? mem_eval_const_float_bits(gen, item->init->expr)
                                     : mem_eval_const_int(gen, item->init->expr);
                }
                global->initializer = item->init == NULL ? mem_zero_initializer(object_type)
                                                         : mem_scalar_initializer(gen, decl->type, init_value);
            } else {
                int *flat = mem_init_to_flat(gen, item->init, item->dims.data, item->dims.count, decl->type);
                global->initializer = item->init == NULL ? mem_zero_initializer(object_type)
                                                         : mem_array_initializer(gen, decl->type, flat, sym->total_slots);
            }
            mem_global_list_push(&gen->module->globals, global);
            sym->addr = mem_new_value(IR_VALUE_GLOBAL, mem_pointer_type(object_type), global->name);
            sym->addr->data.global = global;
        } else {
            sym->addr = mem_emit_alloca(gen, object_type);
            if (item->dims.count == 0) {
                IRValue *init_value = decl->type == TYPE_FLOAT
                                          ? mem_const_float(gen, float_bits_from_host(0.0f))
                                          : mem_const_int(gen, 0);
                if (item->init != NULL) {
                    MemValue value = mem_gen_expr(gen, item->init->expr);
                    init_value = mem_ensure_type(gen, value, decl->type);
                }
                mem_emit_store(gen, init_value, sym->addr);
            } else {
                mem_init_local_array(gen, sym, item->init);
            }
        }
    }
}

static void mem_gen_function(MemIRGen *gen, FuncDef *func) {
    MemFunctionMeta *meta = mem_lookup_function(gen, func->name);
    if (meta == NULL) {
        return;
    }
    gen->current_function = meta->function;
    gen->current_ret_type = func->ret_type;
    gen->current_block = mem_create_block(gen, "entry");
    mem_push_scope(gen);
    for (int i = 0; i < func->params.count; ++i) {
        Param *param = func->params.items[i];
        IRParameter *ir_param = gen->current_function->params.items[i];
        MemSymbol *sym = mem_add_symbol(gen, param->name);
        sym->value_type = param->type;
        sym->dim_count = param->dims.count;
        sym->dims = copy_dims(param->dims.data, param->dims.count);
        sym->total_slots = 1;
        sym->is_param_array = param->is_array;
        if (param->is_array) {
            sym->addr = &ir_param->value;
            sym->object_type = ir_param->type->data.pointer.pointee;
        } else {
            sym->object_type = mem_scalar_type(gen->module, param->type);
            sym->addr = mem_emit_alloca(gen, sym->object_type);
            mem_emit_store(gen, &ir_param->value, sym->addr);
        }
    }
    mem_gen_block(gen, func->block, false);
    if (!mem_block_terminated(gen)) {
        if (func->ret_type == TYPE_VOID) {
            mem_emit_ret(gen, NULL);
        } else if (func->ret_type == TYPE_FLOAT) {
            mem_emit_ret(gen, mem_const_float(gen, float_bits_from_host(0.0f)));
        } else {
            mem_emit_ret(gen, mem_const_int(gen, 0));
        }
    }
    mem_pop_scope(gen);
    gen->current_function = NULL;
    gen->current_block = NULL;
}

IRModule *ast_to_ir(Program *program) {
    MemIRGen gen;
    memset(&gen, 0, sizeof(gen));
    gen.module = mem_new_module("sysy.ir");
    mem_add_runtime_functions(&gen);
    mem_push_scope(&gen);
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            FuncDef *func = item->data.func;
            IRFunction *ir_func = mem_create_function(&gen, func->name, func->ret_type,
                                                      func->params, true, false, false);
            MemFunctionMeta *meta = mem_add_function_meta(&gen, func->name, ir_func,
                                                          func->ret_type, func->params, true);
            meta->ast_func = func;
        }
    }
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_DECL) {
            mem_gen_decl(&gen, item->data.decl, true);
        }
    }
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            mem_gen_function(&gen, item->data.func);
        }
    }
    return gen.module;
}

static const char *mem_dump_inst_name(IRInstructionKind kind) {
    switch (kind) {
        case IR_INST_ALLOCA: return "alloca";
        case IR_INST_LOAD: return "load";
        case IR_INST_STORE: return "store";
        case IR_INST_PHI: return "phi";
        case IR_INST_ADD: return "add";
        case IR_INST_SUB: return "sub";
        case IR_INST_MUL: return "mul";
        case IR_INST_SDIV: return "sdiv";
        case IR_INST_SREM: return "srem";
        case IR_INST_FADD: return "fadd";
        case IR_INST_FSUB: return "fsub";
        case IR_INST_FMUL: return "fmul";
        case IR_INST_FDIV: return "fdiv";
        case IR_INST_ICMP: return "icmp";
        case IR_INST_FCMP: return "fcmp";
        case IR_INST_ZEXT: return "zext";
        case IR_INST_SITOFP: return "sitofp";
        case IR_INST_FPTOSI: return "fptosi";
        case IR_INST_BR: return "br";
        case IR_INST_RET: return "ret";
        case IR_INST_CALL: return "call";
        case IR_INST_GETELEMENTPTR: return "gep";
        case IR_INST_BITCAST: return "bitcast";
    }
    return "inst";
}

static const char *mem_dump_icmp_name(IRIcmpPredicate pred) {
    switch (pred) {
        case IR_ICMP_EQ: return "eq";
        case IR_ICMP_NE: return "ne";
        case IR_ICMP_SLT: return "slt";
        case IR_ICMP_SLE: return "sle";
        case IR_ICMP_SGT: return "sgt";
        case IR_ICMP_SGE: return "sge";
    }
    return "icmp";
}

static const char *mem_dump_fcmp_name(IRFcmpPredicate pred) {
    switch (pred) {
        case IR_FCMP_OEQ: return "oeq";
        case IR_FCMP_ONE: return "one";
        case IR_FCMP_OLT: return "olt";
        case IR_FCMP_OLE: return "ole";
        case IR_FCMP_OGT: return "ogt";
        case IR_FCMP_OGE: return "oge";
    }
    return "fcmp";
}

static void mem_dump_type(FILE *out, IRType *type) {
    if (type == NULL) {
        fputs("<null>", out);
        return;
    }
    switch (type->kind) {
        case IR_TYPE_VOID:
            fputs("void", out);
            return;
        case IR_TYPE_I1:
            fputs("i1", out);
            return;
        case IR_TYPE_I32:
            fputs("i32", out);
            return;
        case IR_TYPE_FLOAT:
            fputs("float", out);
            return;
        case IR_TYPE_POINTER:
            mem_dump_type(out, type->data.pointer.pointee);
            fputc('*', out);
            return;
        case IR_TYPE_ARRAY:
            fprintf(out, "[%d x ", type->data.array.length);
            mem_dump_type(out, type->data.array.element);
            fputc(']', out);
            return;
        case IR_TYPE_FUNCTION:
            mem_dump_type(out, type->data.function.ret);
            fputs(" (", out);
            for (int i = 0; i < type->data.function.param_count; ++i) {
                if (i > 0) {
                    fputs(", ", out);
                }
                mem_dump_type(out, type->data.function.params[i]);
            }
            if (type->data.function.is_variadic) {
                fputs(type->data.function.param_count > 0 ? ", ..." : "...", out);
            }
            fputc(')', out);
            return;
    }
}

static void mem_dump_value(FILE *out, IRValue *value) {
    if (value == NULL) {
        fputs("void", out);
        return;
    }
    switch (value->kind) {
        case IR_VALUE_CONST_INT:
            fprintf(out, "%d", value->data.int_value);
            return;
        case IR_VALUE_CONST_FLOAT:
            fprintf(out, "0x%08x", (unsigned)value->data.float_bits);
            return;
        case IR_VALUE_CONST_ZERO:
            fputs("zero", out);
            return;
        case IR_VALUE_GLOBAL:
        case IR_VALUE_PARAM:
        case IR_VALUE_FUNCTION:
        case IR_VALUE_INSTRUCTION:
            fputs(value->name ? value->name : "<unnamed>", out);
            return;
        case IR_VALUE_BASIC_BLOCK:
            fputs(value->data.basic_block != NULL ? value->data.basic_block->name : "<block>", out);
            return;
        case IR_VALUE_NONE:
        case IR_VALUE_LOCAL:
            fputs(value->name ? value->name : "<value>", out);
            return;
    }
}

static void mem_dump_initializer(FILE *out, IRInitializer *init) {
    if (init == NULL) {
        fputs("zero", out);
        return;
    }
    switch (init->kind) {
        case IR_INIT_ZERO:
            fputs("zero", out);
            return;
        case IR_INIT_INT:
            fprintf(out, "%d", init->data.int_value);
            return;
        case IR_INIT_FLOAT:
            fprintf(out, "0x%08x", (unsigned)init->data.float_bits);
            return;
        case IR_INIT_STRING:
            fprintf(out, "string[%d]", init->data.string.length);
            return;
        case IR_INIT_ARRAY:
            fprintf(out, "array[%d]", init->data.array.count);
            return;
    }
}

static void mem_dump_instruction(FILE *out, IRInstruction *inst) {
    fputs("  ", out);
    if (inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID) {
        mem_dump_value(out, &inst->result);
        fputs(" = ", out);
    }
    fputs(mem_dump_inst_name(inst->kind), out);
    switch (inst->kind) {
        case IR_INST_ALLOCA:
            fputc(' ', out);
            mem_dump_type(out, inst->data.alloca_inst.allocated_type);
            break;
        case IR_INST_LOAD:
            fputc(' ', out);
            mem_dump_type(out, inst->data.load_inst.value_type);
            fputs(", ", out);
            mem_dump_value(out, inst->data.load_inst.ptr);
            break;
        case IR_INST_STORE:
            fputc(' ', out);
            mem_dump_value(out, inst->data.store_inst.value);
            fputs(", ", out);
            mem_dump_value(out, inst->data.store_inst.ptr);
            break;
        case IR_INST_PHI:
            for (int i = 0; i < inst->data.phi_inst.count; ++i) {
                if (i > 0) {
                    fputs(", ", out);
                } else {
                    fputc(' ', out);
                }
                fputc('[', out);
                mem_dump_value(out, inst->data.phi_inst.values.items[i]);
                fprintf(out, ", %s]", inst->data.phi_inst.blocks[i]->name);
            }
            break;
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            fputc(' ', out);
            mem_dump_value(out, inst->data.binary_inst.lhs);
            fputs(", ", out);
            mem_dump_value(out, inst->data.binary_inst.rhs);
            break;
        case IR_INST_ICMP:
            fprintf(out, " %s ", mem_dump_icmp_name(inst->data.icmp_inst.pred));
            mem_dump_value(out, inst->data.icmp_inst.lhs);
            fputs(", ", out);
            mem_dump_value(out, inst->data.icmp_inst.rhs);
            break;
        case IR_INST_FCMP:
            fprintf(out, " %s ", mem_dump_fcmp_name(inst->data.fcmp_inst.pred));
            mem_dump_value(out, inst->data.fcmp_inst.lhs);
            fputs(", ", out);
            mem_dump_value(out, inst->data.fcmp_inst.rhs);
            break;
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            fputc(' ', out);
            mem_dump_value(out, inst->data.cast_inst.value);
            fputs(" to ", out);
            mem_dump_type(out, inst->data.cast_inst.to_type);
            break;
        case IR_INST_BR:
            if (inst->data.br_inst.is_conditional) {
                fputc(' ', out);
                mem_dump_value(out, inst->data.br_inst.condition);
                fprintf(out, ", %s, %s",
                        inst->data.br_inst.true_block->name,
                        inst->data.br_inst.false_block->name);
            } else {
                fprintf(out, " %s", inst->data.br_inst.true_block->name);
            }
            break;
        case IR_INST_RET:
            if (inst->data.ret_inst.value != NULL) {
                fputc(' ', out);
                mem_dump_value(out, inst->data.ret_inst.value);
            }
            break;
        case IR_INST_CALL:
            fprintf(out, " %s(", inst->data.call_inst.callee != NULL
                                  ? inst->data.call_inst.callee->name : "<unknown>");
            for (int i = 0; i < inst->data.call_inst.args.count; ++i) {
                if (i > 0) {
                    fputs(", ", out);
                }
                mem_dump_value(out, inst->data.call_inst.args.items[i]);
            }
            fputc(')', out);
            break;
        case IR_INST_GETELEMENTPTR:
            fputc(' ', out);
            mem_dump_value(out, inst->data.gep_inst.base_ptr);
            for (int i = 0; i < inst->data.gep_inst.indices.count; ++i) {
                fputc('[', out);
                mem_dump_value(out, inst->data.gep_inst.indices.items[i]);
                fputc(']', out);
            }
            break;
        case IR_INST_BITCAST:
            fputc(' ', out);
            mem_dump_value(out, inst->data.bitcast_inst.value);
            fputs(" to ", out);
            mem_dump_type(out, inst->data.bitcast_inst.to_type);
            break;
    }
    fputc('\n', out);
}

static void dump_mem_ir(IRModule *module, FILE *out) {
    fprintf(out, "module %s\n\n", module->name);
    for (int i = 0; i < module->globals.count; ++i) {
        IRGlobal *global = module->globals.items[i];
        fputs("global ", out);
        mem_dump_type(out, global->type);
        fprintf(out, " %s", global->name);
        if (global->is_external) {
            fputs(" external", out);
        } else {
            fputs(" = ", out);
            mem_dump_initializer(out, global->initializer);
        }
        fputc('\n', out);
    }
    fputc('\n', out);
    for (int i = 0; i < module->functions.count; ++i) {
        IRFunction *function = module->functions.items[i];
        fputs(function->is_external ? "declare " : "func ", out);
        mem_dump_type(out, function->ret_type);
        fprintf(out, " %s(", function->name);
        for (int j = 0; j < function->params.count; ++j) {
            IRParameter *param = function->params.items[j];
            if (j > 0) {
                fputs(", ", out);
            }
            mem_dump_type(out, param->type);
            fprintf(out, " %s", param->name);
        }
        fputs(")\n", out);
        if (!function->is_external) {
            for (int bi = 0; bi < function->blocks.count; ++bi) {
                IRBasicBlock *block = function->blocks.items[bi];
                fprintf(out, "%s:\n", block->name);
                for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
                    mem_dump_instruction(out, inst);
                }
            }
            fputs("endfunc\n", out);
        }
        fputc('\n', out);
    }
}

static bool opt_type_equal(IRType *a, IRType *b) {
    if (a == b) {
        return true;
    }
    if (a == NULL || b == NULL || a->kind != b->kind) {
        return false;
    }
    switch (a->kind) {
        case IR_TYPE_VOID:
        case IR_TYPE_I1:
        case IR_TYPE_I32:
        case IR_TYPE_FLOAT:
            return true;
        case IR_TYPE_POINTER:
            return opt_type_equal(a->data.pointer.pointee, b->data.pointer.pointee);
        case IR_TYPE_ARRAY:
            return a->data.array.length == b->data.array.length
                && opt_type_equal(a->data.array.element, b->data.array.element);
        case IR_TYPE_FUNCTION:
            if (!opt_type_equal(a->data.function.ret, b->data.function.ret)
                    || a->data.function.param_count != b->data.function.param_count
                    || a->data.function.is_variadic != b->data.function.is_variadic) {
                return false;
            }
            for (int i = 0; i < a->data.function.param_count; ++i) {
                if (!opt_type_equal(a->data.function.params[i], b->data.function.params[i])) {
                    return false;
                }
            }
            return true;
    }
    return false;
}

static bool opt_const_int_value(IRValue *value, int *out_value) {
    if (value == NULL) {
        return false;
    }
    if (value->kind == IR_VALUE_CONST_INT) {
        *out_value = value->data.int_value;
        return true;
    }
    if (value->kind == IR_VALUE_CONST_ZERO) {
        *out_value = 0;
        return true;
    }
    return false;
}

static IRValue *opt_new_const_int(IRType *type, int value) {
    IRValue *ir = mem_new_value(IR_VALUE_CONST_INT, type, NULL);
    ir->data.int_value = value;
    return ir;
}

static bool opt_value_equal(IRValue *a, IRValue *b) {
    if (a == b) {
        return true;
    }
    if (a == NULL || b == NULL) {
        return false;
    }
    int av = 0;
    int bv = 0;
    if (opt_const_int_value(a, &av) && opt_const_int_value(b, &bv)) {
        return av == bv && opt_type_equal(a->type, b->type);
    }
    if (a->kind == IR_VALUE_CONST_FLOAT && b->kind == IR_VALUE_CONST_FLOAT) {
        return a->data.float_bits == b->data.float_bits && opt_type_equal(a->type, b->type);
    }
    return false;
}

static bool opt_i32_result(IRInstruction *inst) {
    return inst->result_type != NULL
        && (inst->result_type->kind == IR_TYPE_I32 || inst->result_type->kind == IR_TYPE_I1);
}

static bool opt_binary_fold(IRInstruction *inst, IRValue **replacement) {
    if (!opt_i32_result(inst)) {
        return false;
    }
    int lhs = 0;
    int rhs = 0;
    if (!opt_const_int_value(inst->data.binary_inst.lhs, &lhs)
            || !opt_const_int_value(inst->data.binary_inst.rhs, &rhs)) {
        return false;
    }
    long long result = 0;
    switch (inst->kind) {
        case IR_INST_ADD:
            result = (long long)lhs + rhs;
            break;
        case IR_INST_SUB:
            result = (long long)lhs - rhs;
            break;
        case IR_INST_MUL:
            result = (long long)lhs * rhs;
            break;
        case IR_INST_SDIV:
            if (rhs == 0 || (lhs == INT_MIN && rhs == -1)) {
                return false;
            }
            result = lhs / rhs;
            break;
        case IR_INST_SREM:
            if (rhs == 0 || (lhs == INT_MIN && rhs == -1)) {
                return false;
            }
            result = lhs % rhs;
            break;
        default:
            return false;
    }
    if (result < INT_MIN || result > INT_MAX) {
        return false;
    }
    *replacement = opt_new_const_int(inst->result_type, (int)result);
    return true;
}

static bool opt_binary_algebra(IRInstruction *inst, IRValue **replacement) {
    if (!opt_i32_result(inst)) {
        return false;
    }
    IRValue *lhs = inst->data.binary_inst.lhs;
    IRValue *rhs = inst->data.binary_inst.rhs;
    int lhs_const = 0;
    int rhs_const = 0;
    bool lhs_is_const = opt_const_int_value(lhs, &lhs_const);
    bool rhs_is_const = opt_const_int_value(rhs, &rhs_const);
    switch (inst->kind) {
        case IR_INST_ADD:
            if (rhs_is_const && rhs_const == 0) {
                *replacement = lhs;
                return true;
            }
            if (lhs_is_const && lhs_const == 0) {
                *replacement = rhs;
                return true;
            }
            break;
        case IR_INST_SUB:
            if (rhs_is_const && rhs_const == 0) {
                *replacement = lhs;
                return true;
            }
            if (opt_value_equal(lhs, rhs)) {
                *replacement = opt_new_const_int(inst->result_type, 0);
                return true;
            }
            break;
        case IR_INST_MUL:
            if ((rhs_is_const && rhs_const == 0) || (lhs_is_const && lhs_const == 0)) {
                *replacement = opt_new_const_int(inst->result_type, 0);
                return true;
            }
            if (rhs_is_const && rhs_const == 1) {
                *replacement = lhs;
                return true;
            }
            if (lhs_is_const && lhs_const == 1) {
                *replacement = rhs;
                return true;
            }
            break;
        case IR_INST_SDIV:
            if (rhs_is_const && rhs_const == 1) {
                *replacement = lhs;
                return true;
            }
            if (lhs_is_const && lhs_const == 0 && !(rhs_is_const && rhs_const == 0)) {
                *replacement = opt_new_const_int(inst->result_type, 0);
                return true;
            }
            break;
        case IR_INST_SREM:
            if (rhs_is_const && rhs_const == 1) {
                *replacement = opt_new_const_int(inst->result_type, 0);
                return true;
            }
            if (lhs_is_const && lhs_const == 0 && !(rhs_is_const && rhs_const == 0)) {
                *replacement = opt_new_const_int(inst->result_type, 0);
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

static bool opt_invert_icmp_pred(IRIcmpPredicate pred, IRIcmpPredicate *inverted) {
    switch (pred) {
        case IR_ICMP_EQ:
            *inverted = IR_ICMP_NE;
            return true;
        case IR_ICMP_NE:
            *inverted = IR_ICMP_EQ;
            return true;
        case IR_ICMP_SLT:
            *inverted = IR_ICMP_SGE;
            return true;
        case IR_ICMP_SLE:
            *inverted = IR_ICMP_SGT;
            return true;
        case IR_ICMP_SGT:
            *inverted = IR_ICMP_SLE;
            return true;
        case IR_ICMP_SGE:
            *inverted = IR_ICMP_SLT;
            return true;
    }
    return false;
}

static bool opt_zext_i1_source(IRValue *value, IRValue **source) {
    if (value == NULL || value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *inst = value->data.instruction;
    if (inst == NULL || inst->kind != IR_INST_ZEXT) {
        return false;
    }
    IRValue *cast_value = inst->data.cast_inst.value;
    if (cast_value == NULL || cast_value->type == NULL
            || cast_value->type->kind != IR_TYPE_I1
            || inst->result_type == NULL
            || inst->result_type->kind != IR_TYPE_I32) {
        return false;
    }
    *source = cast_value;
    return true;
}

static bool opt_icmp_zext_bool_zero(IRInstruction *inst, IRValue **replacement, bool *rewritten) {
    if (inst->data.icmp_inst.pred != IR_ICMP_EQ && inst->data.icmp_inst.pred != IR_ICMP_NE) {
        return false;
    }
    IRValue *bool_value = NULL;
    int const_value = 0;
    if (opt_zext_i1_source(inst->data.icmp_inst.lhs, &bool_value)) {
        if (!opt_const_int_value(inst->data.icmp_inst.rhs, &const_value) || const_value != 0) {
            return false;
        }
    } else if (opt_zext_i1_source(inst->data.icmp_inst.rhs, &bool_value)) {
        if (!opt_const_int_value(inst->data.icmp_inst.lhs, &const_value) || const_value != 0) {
            return false;
        }
    } else {
        return false;
    }

    if (inst->data.icmp_inst.pred == IR_ICMP_NE) {
        *replacement = bool_value;
        return true;
    }

    if (bool_value->kind == IR_VALUE_INSTRUCTION) {
        IRInstruction *bool_inst = bool_value->data.instruction;
        IRIcmpPredicate inverted = IR_ICMP_EQ;
        if (bool_inst != NULL && bool_inst->kind == IR_INST_ICMP
                && opt_invert_icmp_pred(bool_inst->data.icmp_inst.pred, &inverted)) {
            inst->data.icmp_inst.pred = inverted;
            inst->data.icmp_inst.lhs = bool_inst->data.icmp_inst.lhs;
            inst->data.icmp_inst.rhs = bool_inst->data.icmp_inst.rhs;
            *rewritten = true;
            return false;
        }
    }

    inst->data.icmp_inst.lhs = bool_value;
    inst->data.icmp_inst.rhs = opt_new_const_int(bool_value->type, 0);
    *rewritten = true;
    return false;
}

static bool opt_icmp_fold(IRInstruction *inst, IRValue **replacement, bool *rewritten) {
    if (opt_icmp_zext_bool_zero(inst, replacement, rewritten)) {
        return true;
    }
    if (*rewritten) {
        return false;
    }
    int lhs = 0;
    int rhs = 0;
    bool known = false;
    bool result = false;
    if (opt_const_int_value(inst->data.icmp_inst.lhs, &lhs)
            && opt_const_int_value(inst->data.icmp_inst.rhs, &rhs)) {
        known = true;
        switch (inst->data.icmp_inst.pred) {
            case IR_ICMP_EQ: result = lhs == rhs; break;
            case IR_ICMP_NE: result = lhs != rhs; break;
            case IR_ICMP_SLT: result = lhs < rhs; break;
            case IR_ICMP_SLE: result = lhs <= rhs; break;
            case IR_ICMP_SGT: result = lhs > rhs; break;
            case IR_ICMP_SGE: result = lhs >= rhs; break;
        }
    } else if (opt_value_equal(inst->data.icmp_inst.lhs, inst->data.icmp_inst.rhs)) {
        known = true;
        switch (inst->data.icmp_inst.pred) {
            case IR_ICMP_EQ:
            case IR_ICMP_SLE:
            case IR_ICMP_SGE:
                result = true;
                break;
            case IR_ICMP_NE:
            case IR_ICMP_SLT:
            case IR_ICMP_SGT:
                result = false;
                break;
        }
    }
    if (!known) {
        return false;
    }
    *replacement = opt_new_const_int(inst->result_type, result ? 1 : 0);
    return true;
}

static bool opt_cast_fold(IRInstruction *inst, IRValue **replacement) {
    if (inst->kind == IR_INST_ZEXT) {
        IRValue *value = inst->data.cast_inst.value;
        int const_value = 0;
        if (opt_type_equal(value->type, inst->result_type)) {
            *replacement = value;
            return true;
        }
        if (opt_const_int_value(value, &const_value)) {
            *replacement = opt_new_const_int(inst->result_type, const_value & 1);
            return true;
        }
    } else if (inst->kind == IR_INST_BITCAST) {
        IRValue *value = inst->data.bitcast_inst.value;
        if (opt_type_equal(value->type, inst->result_type)
                || (value->ptr_level > 0 && inst->result.ptr_level > 0)) {
            *replacement = value;
            return true;
        }
    }
    return false;
}

static bool opt_is_pure_inst(IRInstruction *inst) {
    switch (inst->kind) {
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
        case IR_INST_ICMP:
        case IR_INST_FCMP:
        case IR_INST_PHI:
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
        case IR_INST_GETELEMENTPTR:
        case IR_INST_BITCAST:
            return inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID;
        default:
            return false;
    }
}

static bool opt_inst_simplify(IRInstruction *inst, IRValue **replacement, bool *rewritten) {
    switch (inst->kind) {
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
            return opt_binary_fold(inst, replacement) || opt_binary_algebra(inst, replacement);
        case IR_INST_ICMP:
            return opt_icmp_fold(inst, replacement, rewritten);
        case IR_INST_ZEXT:
        case IR_INST_BITCAST:
            return opt_cast_fold(inst, replacement);
        default:
            return false;
    }
}

static void opt_replace_value_in_list(IRValueList *list, IRValue *old_value, IRValue *new_value) {
    for (int i = 0; i < list->count; ++i) {
        if (list->items[i] == old_value) {
            list->items[i] = new_value;
        }
    }
}

static void opt_replace_value_in_inst(IRInstruction *inst, IRValue *old_value, IRValue *new_value) {
    switch (inst->kind) {
        case IR_INST_LOAD:
            if (inst->data.load_inst.ptr == old_value) inst->data.load_inst.ptr = new_value;
            break;
        case IR_INST_STORE:
            if (inst->data.store_inst.value == old_value) inst->data.store_inst.value = new_value;
            if (inst->data.store_inst.ptr == old_value) inst->data.store_inst.ptr = new_value;
            break;
        case IR_INST_PHI:
            opt_replace_value_in_list(&inst->data.phi_inst.values, old_value, new_value);
            break;
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            if (inst->data.binary_inst.lhs == old_value) inst->data.binary_inst.lhs = new_value;
            if (inst->data.binary_inst.rhs == old_value) inst->data.binary_inst.rhs = new_value;
            break;
        case IR_INST_ICMP:
            if (inst->data.icmp_inst.lhs == old_value) inst->data.icmp_inst.lhs = new_value;
            if (inst->data.icmp_inst.rhs == old_value) inst->data.icmp_inst.rhs = new_value;
            break;
        case IR_INST_FCMP:
            if (inst->data.fcmp_inst.lhs == old_value) inst->data.fcmp_inst.lhs = new_value;
            if (inst->data.fcmp_inst.rhs == old_value) inst->data.fcmp_inst.rhs = new_value;
            break;
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            if (inst->data.cast_inst.value == old_value) inst->data.cast_inst.value = new_value;
            break;
        case IR_INST_BR:
            if (inst->data.br_inst.condition == old_value) inst->data.br_inst.condition = new_value;
            break;
        case IR_INST_RET:
            if (inst->data.ret_inst.value == old_value) inst->data.ret_inst.value = new_value;
            break;
        case IR_INST_CALL:
            opt_replace_value_in_list(&inst->data.call_inst.args, old_value, new_value);
            break;
        case IR_INST_GETELEMENTPTR:
            if (inst->data.gep_inst.base_ptr == old_value) inst->data.gep_inst.base_ptr = new_value;
            opt_replace_value_in_list(&inst->data.gep_inst.indices, old_value, new_value);
            break;
        case IR_INST_BITCAST:
            if (inst->data.bitcast_inst.value == old_value) inst->data.bitcast_inst.value = new_value;
            break;
        case IR_INST_ALLOCA:
            break;
    }
}

static void opt_replace_all_uses(IRFunction *function, IRValue *old_value, IRValue *new_value) {
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            opt_replace_value_in_inst(inst, old_value, new_value);
        }
    }
}

static int opt_count_uses_in_inst(IRInstruction *inst, IRValue *value) {
    int count = 0;
    switch (inst->kind) {
        case IR_INST_LOAD:
            return inst->data.load_inst.ptr == value;
        case IR_INST_STORE:
            return (inst->data.store_inst.value == value) + (inst->data.store_inst.ptr == value);
        case IR_INST_PHI:
            for (int i = 0; i < inst->data.phi_inst.values.count; ++i) {
                count += inst->data.phi_inst.values.items[i] == value;
            }
            return count;
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            return (inst->data.binary_inst.lhs == value) + (inst->data.binary_inst.rhs == value);
        case IR_INST_ICMP:
            return (inst->data.icmp_inst.lhs == value) + (inst->data.icmp_inst.rhs == value);
        case IR_INST_FCMP:
            return (inst->data.fcmp_inst.lhs == value) + (inst->data.fcmp_inst.rhs == value);
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            return inst->data.cast_inst.value == value;
        case IR_INST_BR:
            return inst->data.br_inst.condition == value;
        case IR_INST_RET:
            return inst->data.ret_inst.value == value;
        case IR_INST_CALL:
            for (int i = 0; i < inst->data.call_inst.args.count; ++i) {
                count += inst->data.call_inst.args.items[i] == value;
            }
            return count;
        case IR_INST_GETELEMENTPTR:
            count += inst->data.gep_inst.base_ptr == value;
            for (int i = 0; i < inst->data.gep_inst.indices.count; ++i) {
                count += inst->data.gep_inst.indices.items[i] == value;
            }
            return count;
        case IR_INST_BITCAST:
            return inst->data.bitcast_inst.value == value;
        case IR_INST_ALLOCA:
            return 0;
    }
    return 0;
}

static int opt_count_uses(IRFunction *function, IRValue *value) {
    int count = 0;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            count += opt_count_uses_in_inst(inst, value);
        }
    }
    return count;
}

static void opt_remove_inst(IRBasicBlock *block, IRInstruction *inst) {
    if (inst->prev != NULL) {
        inst->prev->next = inst->next;
    } else {
        block->first_inst = inst->next;
    }
    if (inst->next != NULL) {
        inst->next->prev = inst->prev;
    } else {
        block->last_inst = inst->prev;
    }
    inst->prev = NULL;
    inst->next = NULL;
}

typedef struct {
    IRInstruction *inst;
} OptCSEntry;

typedef struct {
    OptCSEntry *items;
    int count;
    int capacity;
} OptCSEntryList;

static void opt_cse_list_push(OptCSEntryList *list, IRInstruction *inst) {
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(OptCSEntry), list->count + 1);
    list->items[list->count++].inst = inst;
}

static bool opt_value_list_equal(IRValueList *a, IRValueList *b) {
    if (a->count != b->count) {
        return false;
    }
    for (int i = 0; i < a->count; ++i) {
        if (!opt_value_equal(a->items[i], b->items[i])) {
            return false;
        }
    }
    return true;
}

static bool opt_inst_same_expr(IRInstruction *a, IRInstruction *b) {
    if (a->kind != b->kind || !opt_type_equal(a->result_type, b->result_type)) {
        return false;
    }
    switch (a->kind) {
        case IR_INST_ADD:
        case IR_INST_MUL:
            return (opt_value_equal(a->data.binary_inst.lhs, b->data.binary_inst.lhs)
                        && opt_value_equal(a->data.binary_inst.rhs, b->data.binary_inst.rhs))
                || (opt_value_equal(a->data.binary_inst.lhs, b->data.binary_inst.rhs)
                        && opt_value_equal(a->data.binary_inst.rhs, b->data.binary_inst.lhs));
        case IR_INST_SUB:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            return opt_value_equal(a->data.binary_inst.lhs, b->data.binary_inst.lhs)
                && opt_value_equal(a->data.binary_inst.rhs, b->data.binary_inst.rhs);
        case IR_INST_ICMP:
            return a->data.icmp_inst.pred == b->data.icmp_inst.pred
                && opt_value_equal(a->data.icmp_inst.lhs, b->data.icmp_inst.lhs)
                && opt_value_equal(a->data.icmp_inst.rhs, b->data.icmp_inst.rhs);
        case IR_INST_FCMP:
            return a->data.fcmp_inst.pred == b->data.fcmp_inst.pred
                && opt_value_equal(a->data.fcmp_inst.lhs, b->data.fcmp_inst.lhs)
                && opt_value_equal(a->data.fcmp_inst.rhs, b->data.fcmp_inst.rhs);
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            return opt_type_equal(a->data.cast_inst.to_type, b->data.cast_inst.to_type)
                && opt_value_equal(a->data.cast_inst.value, b->data.cast_inst.value);
        case IR_INST_GETELEMENTPTR:
            return a->data.gep_inst.inbounds == b->data.gep_inst.inbounds
                && opt_type_equal(a->data.gep_inst.source_element_type, b->data.gep_inst.source_element_type)
                && opt_value_equal(a->data.gep_inst.base_ptr, b->data.gep_inst.base_ptr)
                && opt_value_list_equal(&a->data.gep_inst.indices, &b->data.gep_inst.indices);
        case IR_INST_BITCAST:
            return opt_type_equal(a->data.bitcast_inst.to_type, b->data.bitcast_inst.to_type)
                && opt_value_equal(a->data.bitcast_inst.value, b->data.bitcast_inst.value);
        default:
            return false;
    }
}

static IRInstruction *opt_find_cse(OptCSEntryList *list, IRInstruction *inst) {
    for (int i = 0; i < list->count; ++i) {
        IRInstruction *candidate = list->items[i].inst;
        if (candidate->parent == inst->parent && opt_inst_same_expr(candidate, inst)) {
            return candidate;
        }
    }
    return NULL;
}

static bool opt_basic_block(IRFunction *function, IRBasicBlock *block) {
    bool changed = false;
    OptCSEntryList cse = {0};
    for (IRInstruction *inst = block->first_inst; inst != NULL;) {
        IRInstruction *next = inst->next;
        IRValue *replacement = NULL;
        bool rewritten = false;
        bool removed = false;
        if (opt_inst_simplify(inst, &replacement, &rewritten)) {
            opt_replace_all_uses(function, &inst->result, replacement);
            opt_remove_inst(block, inst);
            changed = true;
            removed = true;
        } else {
            changed = rewritten || changed;
        }
        if (!removed && opt_is_pure_inst(inst)) {
            IRInstruction *existing = opt_find_cse(&cse, inst);
            if (existing != NULL) {
                opt_replace_all_uses(function, &inst->result, &existing->result);
                opt_remove_inst(block, inst);
                changed = true;
            } else {
                opt_cse_list_push(&cse, inst);
            }
        }
        inst = next;
    }
    return changed;
}

static bool opt_delete_dead_pure_insts(IRFunction *function) {
    bool changed = false;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->last_inst; inst != NULL;) {
            IRInstruction *prev = inst->prev;
            if (opt_is_pure_inst(inst) && opt_count_uses(function, &inst->result) == 0) {
                opt_remove_inst(block, inst);
                changed = true;
            }
            inst = prev;
        }
    }
    return changed;
}

static bool opt_is_scalar_type(IRType *type) {
    return type != NULL
        && (type->kind == IR_TYPE_I1 || type->kind == IR_TYPE_I32 || type->kind == IR_TYPE_FLOAT);
}

static bool opt_is_scalar_alloca_inst(IRInstruction *inst) {
    return inst != NULL && inst->kind == IR_INST_ALLOCA
        && opt_is_scalar_type(inst->data.alloca_inst.allocated_type);
}

static IRInstruction *opt_scalar_alloca_from_value(IRValue *value) {
    if (value == NULL || value->kind != IR_VALUE_INSTRUCTION) {
        return NULL;
    }
    IRInstruction *inst = value->data.instruction;
    return opt_is_scalar_alloca_inst(inst) ? inst : NULL;
}

static bool opt_alloca_uses_are_direct_load_store(IRFunction *function, IRInstruction *alloca_inst) {
    IRValue *ptr = &alloca_inst->result;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (inst == alloca_inst) {
                continue;
            }
            switch (inst->kind) {
                case IR_INST_LOAD:
                    if (inst->data.load_inst.ptr == ptr) {
                        continue;
                    }
                    break;
                case IR_INST_STORE:
                    if (inst->data.store_inst.ptr == ptr && inst->data.store_inst.value != ptr) {
                        continue;
                    }
                    break;
                default:
                    break;
            }
            if (opt_count_uses_in_inst(inst, ptr) != 0) {
                return false;
            }
        }
    }
    return true;
}

static int opt_function_block_index(IRFunction *function, IRBasicBlock *block) {
    for (int i = 0; i < function->blocks.count; ++i) {
        if (function->blocks.items[i] == block) {
            return i;
        }
    }
    return -1;
}

static bool opt_inst_before_in_block(IRInstruction *a, IRInstruction *b) {
    if (a == NULL || b == NULL || a->parent != b->parent) {
        return false;
    }
    for (IRInstruction *inst = a; inst != NULL; inst = inst->next) {
        if (inst == b) {
            return true;
        }
    }
    return false;
}

static bool *opt_compute_dominators(IRFunction *function) {
    int n = function->blocks.count;
    if (n <= 0) {
        return NULL;
    }
    bool *dom = (bool *)xmalloc(sizeof(bool) * n * n);
    for (int b = 0; b < n; ++b) {
        for (int d = 0; d < n; ++d) {
            dom[b * n + d] = b == 0 ? d == 0 : true;
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 1; b < n; ++b) {
            IRBasicBlock *block = function->blocks.items[b];
            bool has_pred = false;
            bool new_row_any = false;
            bool *new_row = (bool *)xmalloc(sizeof(bool) * n);
            for (int d = 0; d < n; ++d) {
                new_row[d] = true;
            }
            for (int pi = 0; pi < block->pred_count; ++pi) {
                int pred_index = opt_function_block_index(function, block->preds[pi]);
                if (pred_index < 0) {
                    continue;
                }
                has_pred = true;
                for (int d = 0; d < n; ++d) {
                    new_row[d] = new_row[d] && dom[pred_index * n + d];
                }
            }
            if (!has_pred) {
                for (int d = 0; d < n; ++d) {
                    new_row[d] = false;
                }
            }
            new_row[b] = true;
            for (int d = 0; d < n; ++d) {
                if (dom[b * n + d] != new_row[d]) {
                    dom[b * n + d] = new_row[d];
                    new_row_any = true;
                }
            }
            if (new_row_any) {
                changed = true;
            }
            free(new_row);
        }
    }
    return dom;
}

static int *opt_compute_idoms(IRFunction *function, bool *dom) {
    int n = function->blocks.count;
    int *idom = (int *)xmalloc(sizeof(int) * n);
    for (int b = 0; b < n; ++b) {
        idom[b] = -1;
    }
    if (n > 0) {
        idom[0] = 0;
    }
    for (int b = 1; b < n; ++b) {
        for (int d = 0; d < n; ++d) {
            if (d == b || !dom[b * n + d]) {
                continue;
            }
            bool deepest = true;
            for (int other = 0; other < n; ++other) {
                if (other == b || other == d || !dom[b * n + other]) {
                    continue;
                }
                if (!dom[d * n + other]) {
                    deepest = false;
                    break;
                }
            }
            if (deepest) {
                idom[b] = d;
                break;
            }
        }
    }
    return idom;
}

static bool *opt_compute_dominance_frontier(IRFunction *function, int *idom) {
    int n = function->blocks.count;
    bool *df = (bool *)xmalloc(sizeof(bool) * n * n);
    for (int i = 0; i < n * n; ++i) {
        df[i] = false;
    }
    for (int b = 0; b < n; ++b) {
        IRBasicBlock *block = function->blocks.items[b];
        if (block->pred_count < 2) {
            continue;
        }
        for (int pi = 0; pi < block->pred_count; ++pi) {
            int runner = opt_function_block_index(function, block->preds[pi]);
            while (runner >= 0 && runner != idom[b]) {
                df[runner * n + b] = true;
                if (runner == idom[runner]) {
                    break;
                }
                runner = idom[runner];
            }
        }
    }
    return df;
}

static IRValue *opt_zero_value_for_type(IRType *type) {
    if (type != NULL && type->kind == IR_TYPE_FLOAT) {
        IRValue *value = mem_new_value(IR_VALUE_CONST_FLOAT, type, NULL);
        value->data.float_bits = float_bits_from_host(0.0f);
        return value;
    }
    return opt_new_const_int(type, 0);
}

static void opt_phi_incoming_push(IRInstruction *phi, IRBasicBlock *block, IRValue *value) {
    for (int i = 0; i < phi->data.phi_inst.count; ++i) {
        if (phi->data.phi_inst.blocks[i] == block) {
            phi->data.phi_inst.values.items[i] = value;
            return;
        }
    }
    mem_value_list_push(&phi->data.phi_inst.values, value);
    ensure_capacity((void **)&phi->data.phi_inst.blocks, &phi->data.phi_inst.capacity,
                    sizeof(IRBasicBlock *), phi->data.phi_inst.count + 1);
    phi->data.phi_inst.blocks[phi->data.phi_inst.count++] = block;
}

static IRInstruction *opt_find_phi_for_alloca(IRBasicBlock *block, IRInstruction *alloca_inst) {
    for (IRInstruction *inst = block->first_inst; inst != NULL && inst->kind == IR_INST_PHI; inst = inst->next) {
        if (inst->data.phi_inst.alloca_inst == alloca_inst) {
            return inst;
        }
    }
    return NULL;
}

static void opt_insert_inst_before(IRBasicBlock *block, IRInstruction *before, IRInstruction *inst);

static IRInstruction *opt_insert_phi_for_alloca(IRBasicBlock *block, IRInstruction *alloca_inst) {
    IRInstruction *existing = opt_find_phi_for_alloca(block, alloca_inst);
    if (existing != NULL) {
        return existing;
    }
    static int phi_id = 0;
    IRInstruction *phi = (IRInstruction *)xmalloc(sizeof(IRInstruction));
    memset(phi, 0, sizeof(IRInstruction));
    phi->kind = IR_INST_PHI;
    phi->result_type = alloca_inst->data.alloca_inst.allocated_type;
    phi->parent = block;
    phi->result.kind = IR_VALUE_INSTRUCTION;
    mem_set_value_type(&phi->result, phi->result_type);
    phi->result.name = str_printf("%%phi%d", phi_id++);
    phi->result.data.instruction = phi;
    phi->data.phi_inst.alloca_inst = alloca_inst;
    IRInstruction *insert_before = block->first_inst;
    while (insert_before != NULL && insert_before->kind == IR_INST_PHI) {
        insert_before = insert_before->next;
    }
    opt_insert_inst_before(block, insert_before, phi);
    return phi;
}

static bool opt_collect_promotable_allocas(IRFunction *function, IRInstruction ***out_items, int *out_count) {
    int count = 0;
    int capacity = 0;
    IRInstruction **items = NULL;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (opt_is_scalar_alloca_inst(inst) && opt_alloca_uses_are_direct_load_store(function, inst)) {
                ensure_capacity((void **)&items, &capacity, sizeof(IRInstruction *), count + 1);
                items[count++] = inst;
            }
        }
    }
    *out_items = items;
    *out_count = count;
    return count > 0;
}

static int opt_collect_alloca_def_blocks(IRFunction *function, IRInstruction *alloca_inst, bool *def_blocks) {
    int count = 0;
    IRValue *ptr = &alloca_inst->result;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (inst->kind == IR_INST_STORE && inst->data.store_inst.ptr == ptr) {
                if (!def_blocks[bi]) {
                    def_blocks[bi] = true;
                    ++count;
                }
            }
        }
    }
    return count;
}

static void opt_place_phi_nodes(IRFunction *function, IRInstruction *alloca_inst, bool *df) {
    int n = function->blocks.count;
    bool *def_blocks = (bool *)xmalloc(sizeof(bool) * n);
    bool *has_already = (bool *)xmalloc(sizeof(bool) * n);
    int *work = (int *)xmalloc(sizeof(int) * n * 2 + sizeof(int));
    int work_count = 0;
    for (int i = 0; i < n; ++i) {
        def_blocks[i] = false;
        has_already[i] = false;
    }
    opt_collect_alloca_def_blocks(function, alloca_inst, def_blocks);
    for (int i = 0; i < n; ++i) {
        if (def_blocks[i]) {
            work[work_count++] = i;
        }
    }
    for (int wi = 0; wi < work_count; ++wi) {
        int x = work[wi];
        for (int y = 0; y < n; ++y) {
            if (!df[x * n + y] || has_already[y]) {
                continue;
            }
            opt_insert_phi_for_alloca(function->blocks.items[y], alloca_inst);
            has_already[y] = true;
            if (!def_blocks[y]) {
                if (work_count < n * 2) {
                    work[work_count++] = y;
                }
                def_blocks[y] = true;
            }
        }
    }
    free(def_blocks);
    free(has_already);
    free(work);
}

typedef struct {
    IRValue **items;
    int count;
    int capacity;
} OptValueStack;

static void opt_value_stack_push(OptValueStack *stack, IRValue *value) {
    ensure_capacity((void **)&stack->items, &stack->capacity, sizeof(IRValue *), stack->count + 1);
    stack->items[stack->count++] = value;
}

static IRValue *opt_value_stack_top(OptValueStack *stack, IRType *type) {
    if (stack->count == 0) {
        opt_value_stack_push(stack, opt_zero_value_for_type(type));
    }
    return stack->items[stack->count - 1];
}

static bool opt_alloca_has_load(IRFunction *function, IRInstruction *alloca_inst);

static void opt_remove_stores_to_alloca(IRFunction *function, IRInstruction *alloca_inst) {
    IRValue *ptr = &alloca_inst->result;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            if (inst->kind == IR_INST_STORE && inst->data.store_inst.ptr == ptr) {
                opt_remove_inst(block, inst);
            }
            inst = next;
        }
    }
}

static void opt_rename_alloca_recursive(IRFunction *function, int block_index, int *idom,
                                        IRInstruction *alloca_inst, OptValueStack *stack) {
    IRBasicBlock *block = function->blocks.items[block_index];
    int stack_mark = stack->count;
    for (IRInstruction *inst = block->first_inst; inst != NULL && inst->kind == IR_INST_PHI; inst = inst->next) {
        if (inst->data.phi_inst.alloca_inst == alloca_inst) {
            opt_value_stack_push(stack, &inst->result);
        }
    }
    IRValue *ptr = &alloca_inst->result;
    for (IRInstruction *inst = block->first_inst; inst != NULL;) {
        IRInstruction *next = inst->next;
        if (inst->kind == IR_INST_LOAD && inst->data.load_inst.ptr == ptr) {
            IRValue *value = opt_value_stack_top(stack, alloca_inst->data.alloca_inst.allocated_type);
            opt_replace_all_uses(function, &inst->result, value);
            opt_remove_inst(block, inst);
        } else if (inst->kind == IR_INST_STORE && inst->data.store_inst.ptr == ptr) {
            opt_value_stack_push(stack, inst->data.store_inst.value);
        }
        inst = next;
    }
    IRValue *current = opt_value_stack_top(stack, alloca_inst->data.alloca_inst.allocated_type);
    for (int si = 0; si < block->succ_count; ++si) {
        IRBasicBlock *succ = block->succs[si];
        for (IRInstruction *phi = succ->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
            if (phi->data.phi_inst.alloca_inst == alloca_inst) {
                opt_phi_incoming_push(phi, block, current);
            }
        }
    }
    for (int child = 0; child < function->blocks.count; ++child) {
        if (child != block_index && idom[child] == block_index) {
            opt_rename_alloca_recursive(function, child, idom, alloca_inst, stack);
        }
    }
    stack->count = stack_mark;
}

static bool opt_phi_has_incoming_from_all_preds(IRInstruction *phi) {
    return phi->parent != NULL && phi->data.phi_inst.count == phi->parent->pred_count;
}

static bool opt_phi_trivial_value(IRInstruction *phi, IRValue **replacement) {
    IRValue *same = NULL;
    for (int i = 0; i < phi->data.phi_inst.count; ++i) {
        IRValue *value = phi->data.phi_inst.values.items[i];
        if (value == &phi->result) {
            continue;
        }
        if (same == NULL) {
            same = value;
        } else if (!opt_value_equal(same, value)) {
            return false;
        }
    }
    if (same == NULL) {
        return false;
    }
    *replacement = same;
    return true;
}

static bool opt_simplify_phi_nodes(IRFunction *function) {
    bool changed = false;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            IRValue *replacement = NULL;
            if (inst->kind == IR_INST_PHI &&
                    (!opt_phi_has_incoming_from_all_preds(inst) ||
                     opt_phi_trivial_value(inst, &replacement))) {
                if (replacement != NULL) {
                    opt_replace_all_uses(function, &inst->result, replacement);
                }
                if (replacement != NULL || inst->data.phi_inst.count == 0) {
                    opt_remove_inst(block, inst);
                    changed = true;
                }
            }
            inst = next;
        }
    }
    return changed;
}

static bool opt_mem2reg(IRFunction *function) {
    IRInstruction **allocas = NULL;
    int alloca_count = 0;
    if (!opt_collect_promotable_allocas(function, &allocas, &alloca_count)) {
        return false;
    }
    bool changed = false;
    bool *dom = opt_compute_dominators(function);
    int *idom = opt_compute_idoms(function, dom);
    bool *df = opt_compute_dominance_frontier(function, idom);
    for (int i = 0; i < alloca_count; ++i) {
        IRInstruction *alloca_inst = allocas[i];
        if (alloca_inst->parent == NULL || !opt_alloca_uses_are_direct_load_store(function, alloca_inst)) {
            continue;
        }
        opt_place_phi_nodes(function, alloca_inst, df);
        OptValueStack stack = {0};
        opt_value_stack_push(&stack, opt_zero_value_for_type(alloca_inst->data.alloca_inst.allocated_type));
        opt_rename_alloca_recursive(function, 0, idom, alloca_inst, &stack);
        free(stack.items);
        if (!opt_alloca_has_load(function, alloca_inst)) {
            opt_remove_stores_to_alloca(function, alloca_inst);
        }
        if (opt_count_uses(function, &alloca_inst->result) == 0) {
            opt_remove_inst(alloca_inst->parent, alloca_inst);
        }
        changed = true;
    }
    changed = opt_simplify_phi_nodes(function) || changed;
    free(df);
    free(idom);
    free(dom);
    free(allocas);
    return changed;
}

static bool opt_store_dominates_load(IRFunction *function, bool *dom,
                                     IRInstruction *store, IRInstruction *load) {
    if (store->parent == load->parent) {
        return opt_inst_before_in_block(store, load);
    }
    int n = function->blocks.count;
    int store_block = opt_function_block_index(function, store->parent);
    int load_block = opt_function_block_index(function, load->parent);
    if (store_block < 0 || load_block < 0) {
        return false;
    }
    return dom[load_block * n + store_block];
}

static bool opt_promote_single_store_allocas(IRFunction *function) {
    bool changed = false;
    bool *dom = opt_compute_dominators(function);
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *alloca_inst = block->first_inst; alloca_inst != NULL;) {
            IRInstruction *next_alloca = alloca_inst->next;
            if (!opt_is_scalar_alloca_inst(alloca_inst) ||
                    !opt_alloca_uses_are_direct_load_store(function, alloca_inst)) {
                alloca_inst = next_alloca;
                continue;
            }
            IRValue *ptr = &alloca_inst->result;
            IRInstruction *only_store = NULL;
            int store_count = 0;
            bool ok = true;
            for (int bj = 0; bj < function->blocks.count; ++bj) {
                IRBasicBlock *scan = function->blocks.items[bj];
                for (IRInstruction *inst = scan->first_inst; inst != NULL; inst = inst->next) {
                    if (inst->kind == IR_INST_STORE && inst->data.store_inst.ptr == ptr) {
                        only_store = inst;
                        ++store_count;
                    }
                }
            }
            if (store_count != 1 || only_store == NULL) {
                alloca_inst = next_alloca;
                continue;
            }
            for (int bj = 0; bj < function->blocks.count && ok; ++bj) {
                IRBasicBlock *scan = function->blocks.items[bj];
                for (IRInstruction *inst = scan->first_inst; inst != NULL; inst = inst->next) {
                    if (inst->kind == IR_INST_LOAD && inst->data.load_inst.ptr == ptr &&
                            !opt_store_dominates_load(function, dom, only_store, inst)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok) {
                alloca_inst = next_alloca;
                continue;
            }
            IRValue *stored_value = only_store->data.store_inst.value;
            for (int bj = 0; bj < function->blocks.count; ++bj) {
                IRBasicBlock *scan = function->blocks.items[bj];
                for (IRInstruction *inst = scan->first_inst; inst != NULL;) {
                    IRInstruction *next = inst->next;
                    if (inst->kind == IR_INST_LOAD && inst->data.load_inst.ptr == ptr) {
                        opt_replace_all_uses(function, &inst->result, stored_value);
                        opt_remove_inst(scan, inst);
                        changed = true;
                    }
                    inst = next;
                }
            }
            opt_remove_inst(only_store->parent, only_store);
            opt_remove_inst(alloca_inst->parent, alloca_inst);
            changed = true;
            alloca_inst = next_alloca;
        }
    }
    free(dom);
    return changed;
}

typedef struct {
    IRValue *ptr;
    IRValue *value;
} OptStoreValue;

typedef struct {
    OptStoreValue *items;
    int count;
    int capacity;
} OptStoreValueList;

static int opt_store_value_find(OptStoreValueList *list, IRValue *ptr) {
    for (int i = 0; i < list->count; ++i) {
        if (list->items[i].ptr == ptr) {
            return i;
        }
    }
    return -1;
}

static void opt_store_value_set(OptStoreValueList *list, IRValue *ptr, IRValue *value) {
    int index = opt_store_value_find(list, ptr);
    if (index >= 0) {
        list->items[index].value = value;
        return;
    }
    ensure_capacity((void **)&list->items, &list->capacity, sizeof(OptStoreValue), list->count + 1);
    list->items[list->count].ptr = ptr;
    list->items[list->count].value = value;
    list->count++;
}

static bool opt_forward_loads_in_block(IRFunction *function, IRBasicBlock *block) {
    bool changed = false;
    OptStoreValueList known = {0};
    for (IRInstruction *inst = block->first_inst; inst != NULL;) {
        IRInstruction *next = inst->next;
        if (inst->kind == IR_INST_STORE) {
            IRInstruction *alloca_inst = opt_scalar_alloca_from_value(inst->data.store_inst.ptr);
            if (alloca_inst != NULL && opt_alloca_uses_are_direct_load_store(function, alloca_inst)) {
                opt_store_value_set(&known, inst->data.store_inst.ptr, inst->data.store_inst.value);
            }
        } else if (inst->kind == IR_INST_LOAD) {
            IRInstruction *alloca_inst = opt_scalar_alloca_from_value(inst->data.load_inst.ptr);
            if (alloca_inst == NULL || !opt_alloca_uses_are_direct_load_store(function, alloca_inst)) {
                inst = next;
                continue;
            }
            int index = opt_store_value_find(&known, inst->data.load_inst.ptr);
            if (index >= 0) {
                opt_replace_all_uses(function, &inst->result, known.items[index].value);
                opt_remove_inst(block, inst);
                changed = true;
            } else {
                opt_store_value_set(&known, inst->data.load_inst.ptr, &inst->result);
            }
        }
        inst = next;
    }
    free(known.items);
    return changed;
}

static bool opt_alloca_has_load(IRFunction *function, IRInstruction *alloca_inst) {
    IRValue *ptr = &alloca_inst->result;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (inst->kind == IR_INST_LOAD && inst->data.load_inst.ptr == ptr) {
                return true;
            }
        }
    }
    return false;
}

static bool opt_delete_dead_scalar_memory(IRFunction *function) {
    bool changed = false;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            if (inst->kind == IR_INST_STORE) {
                IRInstruction *alloca_inst = opt_scalar_alloca_from_value(inst->data.store_inst.ptr);
                if (alloca_inst != NULL &&
                        opt_alloca_uses_are_direct_load_store(function, alloca_inst) &&
                        !opt_alloca_has_load(function, alloca_inst)) {
                    opt_remove_inst(block, inst);
                    changed = true;
                }
            }
            inst = next;
        }
    }
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            if (opt_is_scalar_alloca_inst(inst) && opt_count_uses(function, &inst->result) == 0) {
                opt_remove_inst(block, inst);
                changed = true;
            }
            inst = next;
        }
    }
    return changed;
}

static bool opt_global_memory_pass(IRFunction *function) {
    bool changed = false;
    changed = opt_mem2reg(function) || changed;
    changed = opt_promote_single_store_allocas(function) || changed;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        changed = opt_forward_loads_in_block(function, function->blocks.items[bi]) || changed;
    }
    changed = opt_delete_dead_scalar_memory(function) || changed;
    return changed;
}

static bool opt_delete_after_terminator(IRFunction *function) {
    (void)function;
    bool changed = false;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        bool terminated = false;
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            if (terminated) {
                opt_remove_inst(block, inst);
                changed = true;
            } else if (inst->kind == IR_INST_BR || inst->kind == IR_INST_RET) {
                terminated = true;
            }
            inst = next;
        }
    }
    return changed;
}

static bool opt_dom_block(IRFunction *function, bool *dom, IRBasicBlock *dominator, IRBasicBlock *block) {
    int n = function->blocks.count;
    int d = opt_function_block_index(function, dominator);
    int b = opt_function_block_index(function, block);
    return d >= 0 && b >= 0 && dom[b * n + d];
}

static bool opt_inst_dominates_inst(IRFunction *function, bool *dom,
                                    IRInstruction *dominator, IRInstruction *inst) {
    if (dominator == inst) {
        return true;
    }
    if (dominator->parent == inst->parent) {
        return opt_inst_before_in_block(dominator, inst);
    }
    return opt_dom_block(function, dom, dominator->parent, inst->parent);
}

static bool opt_loop_safe_pure_inst(IRInstruction *inst) {
    switch (inst->kind) {
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_ICMP:
        case IR_INST_FCMP:
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
        case IR_INST_GETELEMENTPTR:
        case IR_INST_BITCAST:
            return inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID;
        default:
            return false;
    }
}

static bool opt_value_defined_in_loop(IRFunction *function, bool *in_loop, IRValue *value) {
    if (value == NULL || value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *inst = value->data.instruction;
    int index = opt_function_block_index(function, inst->parent);
    return index >= 0 && in_loop[index];
}

static bool opt_value_invariant_for_loop(IRFunction *function, bool *in_loop, IRValue *value) {
    return !opt_value_defined_in_loop(function, in_loop, value);
}

static bool opt_value_list_invariant_for_loop(IRFunction *function, bool *in_loop, IRValueList *list) {
    for (int i = 0; i < list->count; ++i) {
        if (!opt_value_invariant_for_loop(function, in_loop, list->items[i])) {
            return false;
        }
    }
    return true;
}

static bool opt_inst_loop_invariant(IRFunction *function, bool *in_loop, IRInstruction *inst) {
    if (!opt_loop_safe_pure_inst(inst)) {
        return false;
    }
    switch (inst->kind) {
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.binary_inst.lhs)
                && opt_value_invariant_for_loop(function, in_loop, inst->data.binary_inst.rhs);
        case IR_INST_ICMP:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.icmp_inst.lhs)
                && opt_value_invariant_for_loop(function, in_loop, inst->data.icmp_inst.rhs);
        case IR_INST_FCMP:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.fcmp_inst.lhs)
                && opt_value_invariant_for_loop(function, in_loop, inst->data.fcmp_inst.rhs);
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.cast_inst.value);
        case IR_INST_GETELEMENTPTR:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.gep_inst.base_ptr)
                && opt_value_list_invariant_for_loop(function, in_loop, &inst->data.gep_inst.indices);
        case IR_INST_BITCAST:
            return opt_value_invariant_for_loop(function, in_loop, inst->data.bitcast_inst.value);
        default:
            return false;
    }
}

static void opt_insert_inst_before(IRBasicBlock *block, IRInstruction *before, IRInstruction *inst) {
    inst->parent = block;
    if (before == NULL) {
        inst->prev = block->last_inst;
        inst->next = NULL;
        if (block->last_inst != NULL) {
            block->last_inst->next = inst;
        } else {
            block->first_inst = inst;
        }
        block->last_inst = inst;
        return;
    }
    inst->prev = before->prev;
    inst->next = before;
    if (before->prev != NULL) {
        before->prev->next = inst;
    } else {
        block->first_inst = inst;
    }
    before->prev = inst;
}

static void opt_move_inst_before(IRBasicBlock *target, IRInstruction *before, IRInstruction *inst) {
    opt_remove_inst(inst->parent, inst);
    opt_insert_inst_before(target, before, inst);
}

static IRInstruction *opt_preheader_insert_point(IRBasicBlock *preheader) {
    if (preheader == NULL) {
        return NULL;
    }
    if (preheader->last_inst != NULL &&
            (preheader->last_inst->kind == IR_INST_BR || preheader->last_inst->kind == IR_INST_RET)) {
        return preheader->last_inst;
    }
    return NULL;
}

static bool *opt_collect_natural_loop(IRFunction *function, IRBasicBlock *header, IRBasicBlock *latch) {
    int n = function->blocks.count;
    bool *in_loop = (bool *)xmalloc(sizeof(bool) * n);
    int *stack = (int *)xmalloc(sizeof(int) * n);
    for (int i = 0; i < n; ++i) {
        in_loop[i] = false;
    }
    int header_index = opt_function_block_index(function, header);
    int latch_index = opt_function_block_index(function, latch);
    if (header_index < 0 || latch_index < 0) {
        free(stack);
        return in_loop;
    }
    int sp = 0;
    in_loop[header_index] = true;
    if (!in_loop[latch_index]) {
        in_loop[latch_index] = true;
        stack[sp++] = latch_index;
    }
    while (sp > 0) {
        int index = stack[--sp];
        IRBasicBlock *block = function->blocks.items[index];
        for (int pi = 0; pi < block->pred_count; ++pi) {
            int pred_index = opt_function_block_index(function, block->preds[pi]);
            if (pred_index >= 0 && !in_loop[pred_index]) {
                in_loop[pred_index] = true;
                stack[sp++] = pred_index;
            }
        }
    }
    free(stack);
    return in_loop;
}

static IRBasicBlock *opt_find_preheader(IRFunction *function, bool *in_loop, IRBasicBlock *header) {
    IRBasicBlock *preheader = NULL;
    for (int pi = 0; pi < header->pred_count; ++pi) {
        IRBasicBlock *pred = header->preds[pi];
        int pred_index = opt_function_block_index(function, pred);
        if (pred_index < 0 || in_loop[pred_index]) {
            continue;
        }
        if (preheader != NULL) {
            return NULL;
        }
        preheader = pred;
    }
    if (preheader == NULL || preheader->last_inst == NULL ||
            preheader->last_inst->kind != IR_INST_BR ||
            preheader->last_inst->data.br_inst.is_conditional ||
            preheader->last_inst->data.br_inst.true_block != header) {
        return NULL;
    }
    return preheader;
}

typedef struct {
    IRInstruction *phi;
    IRInstruction *update_inst;
    IRValue *start_value;
    int step;
} OptLoopInductionVar;

typedef struct {
    IRValue **from;
    IRValue **to;
    int count;
    int from_capacity;
    int to_capacity;
} OptValueMap;

static void opt_value_map_push(OptValueMap *map, IRValue *from, IRValue *to) {
    ensure_capacity((void **)&map->from, &map->from_capacity, sizeof(IRValue *), map->count + 1);
    ensure_capacity((void **)&map->to, &map->to_capacity, sizeof(IRValue *), map->count + 1);
    map->from[map->count] = from;
    map->to[map->count] = to;
    map->count++;
}

static IRValue *opt_value_map_get(OptValueMap *map, IRValue *from) {
    for (int i = map->count - 1; i >= 0; --i) {
        if (map->from[i] == from) {
            return map->to[i];
        }
    }
    return from;
}

static void opt_value_map_reset(OptValueMap *map) {
    free(map->from);
    free(map->to);
    map->from = NULL;
    map->to = NULL;
    map->count = 0;
    map->from_capacity = 0;
    map->to_capacity = 0;
}

static IRInstruction *opt_new_inst(IRInstructionKind kind, IRType *result_type) {
    static int temp_id = 0;
    IRInstruction *inst = (IRInstruction *)xmalloc(sizeof(IRInstruction));
    memset(inst, 0, sizeof(IRInstruction));
    inst->kind = kind;
    inst->result_type = result_type;
    if (result_type != NULL && result_type->kind != IR_TYPE_VOID) {
        inst->result.kind = IR_VALUE_INSTRUCTION;
        mem_set_value_type(&inst->result, result_type);
        inst->result.name = str_printf("%%opt%d", temp_id++);
        inst->result.data.instruction = inst;
    }
    return inst;
}

static IRInstruction *opt_create_phi(IRBasicBlock *block, IRType *type) {
    IRInstruction *phi = opt_new_inst(IR_INST_PHI, type);
    phi->parent = block;
    IRInstruction *insert_before = block->first_inst;
    while (insert_before != NULL && insert_before->kind == IR_INST_PHI) {
        insert_before = insert_before->next;
    }
    opt_insert_inst_before(block, insert_before, phi);
    return phi;
}

static IRInstruction *opt_create_binary(IRBasicBlock *block, IRInstruction *before,
                                        IRInstructionKind kind, IRType *type,
                                        IRValue *lhs, IRValue *rhs) {
    IRInstruction *inst = opt_new_inst(kind, type);
    inst->data.binary_inst.lhs = lhs;
    inst->data.binary_inst.rhs = rhs;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_icmp(IRBasicBlock *block, IRInstruction *before,
                                      IRIcmpPredicate pred, IRValue *lhs, IRValue *rhs,
                                      IRType *type) {
    IRInstruction *inst = opt_new_inst(IR_INST_ICMP, type);
    inst->data.icmp_inst.pred = pred;
    inst->data.icmp_inst.lhs = lhs;
    inst->data.icmp_inst.rhs = rhs;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_fcmp(IRBasicBlock *block, IRInstruction *before,
                                      IRFcmpPredicate pred, IRValue *lhs, IRValue *rhs,
                                      IRType *type) {
    IRInstruction *inst = opt_new_inst(IR_INST_FCMP, type);
    inst->data.fcmp_inst.pred = pred;
    inst->data.fcmp_inst.lhs = lhs;
    inst->data.fcmp_inst.rhs = rhs;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_cast(IRBasicBlock *block, IRInstruction *before,
                                      IRInstructionKind kind, IRValue *value, IRType *to_type) {
    IRInstruction *inst = opt_new_inst(kind, to_type);
    inst->data.cast_inst.value = value;
    inst->data.cast_inst.to_type = to_type;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_bitcast(IRBasicBlock *block, IRInstruction *before,
                                         IRValue *value, IRType *to_type) {
    IRInstruction *inst = opt_new_inst(IR_INST_BITCAST, to_type);
    inst->data.bitcast_inst.value = value;
    inst->data.bitcast_inst.to_type = to_type;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_load(IRBasicBlock *block, IRInstruction *before,
                                      IRType *value_type, IRValue *ptr, int alignment) {
    IRInstruction *inst = opt_new_inst(IR_INST_LOAD, value_type);
    inst->data.load_inst.ptr = ptr;
    inst->data.load_inst.value_type = value_type;
    inst->data.load_inst.alignment = alignment;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_store(IRBasicBlock *block, IRInstruction *before,
                                       IRType *void_type, IRValue *value, IRValue *ptr, int alignment) {
    IRInstruction *inst = opt_new_inst(IR_INST_STORE, void_type);
    inst->data.store_inst.value = value;
    inst->data.store_inst.ptr = ptr;
    inst->data.store_inst.alignment = alignment;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_call(IRBasicBlock *block, IRInstruction *before,
                                      IRFunction *callee, IRType *ret_type, IRValueList args) {
    IRInstruction *inst = opt_new_inst(IR_INST_CALL, ret_type);
    inst->data.call_inst.callee = callee;
    inst->data.call_inst.ret_type = ret_type;
    inst->data.call_inst.args = args;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static IRInstruction *opt_create_gep(IRBasicBlock *block, IRInstruction *before,
                                     IRValue *base_ptr, IRType *source_type,
                                     IRValueList indices, IRType *result_pointee, bool inbounds) {
    IRInstruction *inst = opt_new_inst(IR_INST_GETELEMENTPTR, mem_pointer_type(result_pointee));
    inst->data.gep_inst.base_ptr = base_ptr;
    inst->data.gep_inst.source_element_type = source_type;
    inst->data.gep_inst.indices = indices;
    inst->data.gep_inst.inbounds = inbounds;
    opt_insert_inst_before(block, before, inst);
    return inst;
}

static int opt_phi_incoming_index(IRInstruction *phi, IRBasicBlock *block) {
    if (phi == NULL || phi->kind != IR_INST_PHI) {
        return -1;
    }
    for (int i = 0; i < phi->data.phi_inst.count; ++i) {
        if (phi->data.phi_inst.blocks[i] == block) {
            return i;
        }
    }
    return -1;
}

static IRValue *opt_phi_incoming_value(IRInstruction *phi, IRBasicBlock *block) {
    int index = opt_phi_incoming_index(phi, block);
    return index >= 0 ? phi->data.phi_inst.values.items[index] : NULL;
}

static IRBasicBlock *opt_find_unique_loop_latch(IRFunction *function, bool *in_loop, IRBasicBlock *header) {
    IRBasicBlock *latch = NULL;
    for (int pi = 0; pi < header->pred_count; ++pi) {
        IRBasicBlock *pred = header->preds[pi];
        int pred_index = opt_function_block_index(function, pred);
        if (pred_index < 0 || !in_loop[pred_index]) {
            continue;
        }
        if (latch != NULL) {
            return NULL;
        }
        latch = pred;
    }
    return latch;
}

static bool opt_inst_uses_value(IRInstruction *inst, IRValue *value) {
    return opt_count_uses_in_inst(inst, value) > 0;
}

static bool opt_all_uses_in_loop(IRFunction *function, bool *in_loop, IRValue *value) {
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (!opt_inst_uses_value(inst, value)) {
                continue;
            }
            if (!in_loop[bi]) {
                return false;
            }
        }
    }
    return true;
}

static void opt_replace_uses_in_loop(IRFunction *function, bool *in_loop, IRValue *old_value, IRValue *new_value) {
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        if (!in_loop[bi]) {
            continue;
        }
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            opt_replace_value_in_inst(inst, old_value, new_value);
        }
    }
}

static IRIcmpPredicate opt_swap_icmp_pred(IRIcmpPredicate pred) {
    switch (pred) {
        case IR_ICMP_EQ: return IR_ICMP_EQ;
        case IR_ICMP_NE: return IR_ICMP_NE;
        case IR_ICMP_SLT: return IR_ICMP_SGT;
        case IR_ICMP_SLE: return IR_ICMP_SGE;
        case IR_ICMP_SGT: return IR_ICMP_SLT;
        case IR_ICMP_SGE: return IR_ICMP_SLE;
    }
    return pred;
}

static bool opt_eval_icmp_pred64(IRIcmpPredicate pred, long long lhs, long long rhs) {
    switch (pred) {
        case IR_ICMP_EQ: return lhs == rhs;
        case IR_ICMP_NE: return lhs != rhs;
        case IR_ICMP_SLT: return lhs < rhs;
        case IR_ICMP_SLE: return lhs <= rhs;
        case IR_ICMP_SGT: return lhs > rhs;
        case IR_ICMP_SGE: return lhs >= rhs;
    }
    return false;
}

static bool opt_compute_small_trip_count(IRIcmpPredicate pred, int start, int bound, int step,
                                         int max_trip_count, int *trip_count) {
    if (step == 0 || max_trip_count <= 0) {
        return false;
    }
    long long current = start;
    for (int i = 0; i <= max_trip_count; ++i) {
        if (!opt_eval_icmp_pred64(pred, current, bound)) {
            *trip_count = i;
            return true;
        }
        if (i == max_trip_count) {
            break;
        }
        long long next = current + step;
        if (next < INT_MIN || next > INT_MAX) {
            return false;
        }
        current = next;
    }
    return false;
}

static int opt_loop_block_count(IRFunction *function, bool *in_loop) {
    int count = 0;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        if (in_loop[bi]) {
            count++;
        }
    }
    return count;
}

static int opt_block_non_phi_non_term_inst_count(IRBasicBlock *block) {
    int count = 0;
    for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
        if (inst->kind == IR_INST_PHI || inst->kind == IR_INST_BR || inst->kind == IR_INST_RET) {
            continue;
        }
        count++;
    }
    return count;
}

static bool opt_match_loop_induction_var(IRFunction *function, bool *in_loop,
                                         IRBasicBlock *header, IRBasicBlock *preheader,
                                         IRBasicBlock *latch, IRInstruction *phi,
                                         OptLoopInductionVar *iv, bool *normalized) {
    (void)header;
    if (phi == NULL || phi->kind != IR_INST_PHI || phi->result_type == NULL
            || phi->result_type->kind != IR_TYPE_I32 || phi->data.phi_inst.count != 2) {
        return false;
    }
    IRValue *start_value = opt_phi_incoming_value(phi, preheader);
    IRValue *latch_value = opt_phi_incoming_value(phi, latch);
    if (start_value == NULL || latch_value == NULL || !opt_value_invariant_for_loop(function, in_loop, start_value)) {
        return false;
    }
    if (latch_value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *update_inst = latch_value->data.instruction;
    if (update_inst == NULL || update_inst->parent != latch
            || (update_inst->kind != IR_INST_ADD && update_inst->kind != IR_INST_SUB)) {
        return false;
    }
    int step = 0;
    int rhs_const = 0;
    if (update_inst->kind == IR_INST_ADD) {
        if (update_inst->data.binary_inst.lhs == &phi->result
                && opt_const_int_value(update_inst->data.binary_inst.rhs, &rhs_const)) {
            step = rhs_const;
        } else if (update_inst->data.binary_inst.rhs == &phi->result
                && opt_const_int_value(update_inst->data.binary_inst.lhs, &rhs_const)) {
            step = rhs_const;
            update_inst->data.binary_inst.lhs = &phi->result;
            update_inst->data.binary_inst.rhs = opt_new_const_int(update_inst->result_type, rhs_const);
            *normalized = true;
        } else {
            return false;
        }
    } else {
        if (update_inst->data.binary_inst.lhs != &phi->result
                || !opt_const_int_value(update_inst->data.binary_inst.rhs, &rhs_const)) {
            return false;
        }
        if (rhs_const == INT_MIN) {
            return false;
        }
        step = -rhs_const;
        update_inst->kind = IR_INST_ADD;
        update_inst->data.binary_inst.rhs = opt_new_const_int(update_inst->result_type, step);
        *normalized = true;
    }
    if (step == 0) {
        return false;
    }
    iv->phi = phi;
    iv->update_inst = update_inst;
    iv->start_value = start_value;
    iv->step = step;
    return true;
}

static bool opt_match_iv_scale(IRValue *value, IRInstruction *phi, int *scale) {
    int factor = 0;
    if (value == &phi->result) {
        *scale = 1;
        return true;
    }
    if (value == NULL || value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *inst = value->data.instruction;
    if (inst == NULL || inst->kind != IR_INST_MUL || inst->result_type == NULL
            || inst->result_type->kind != IR_TYPE_I32) {
        return false;
    }
    if (inst->data.binary_inst.lhs == &phi->result && opt_const_int_value(inst->data.binary_inst.rhs, &factor)) {
        *scale = factor;
        return true;
    }
    if (inst->data.binary_inst.rhs == &phi->result && opt_const_int_value(inst->data.binary_inst.lhs, &factor)) {
        *scale = factor;
        return true;
    }
    return false;
}

static bool opt_match_strength_reduction_expr(IRFunction *function, bool *in_loop,
                                              IRInstruction *inst, OptLoopInductionVar *iv,
                                              int *scale, IRValue **offset, bool *subtract_offset) {
    *scale = 0;
    *offset = NULL;
    *subtract_offset = false;
    if (inst == NULL || inst == iv->phi || inst == iv->update_inst
            || inst->result_type == NULL || inst->result_type->kind != IR_TYPE_I32) {
        return false;
    }
    if (!opt_all_uses_in_loop(function, in_loop, &inst->result)) {
        return false;
    }
    if (opt_match_iv_scale(&inst->result, iv->phi, scale)) {
        return *scale != 1;
    }
    if (inst->kind == IR_INST_ADD || inst->kind == IR_INST_SUB) {
        if (opt_match_iv_scale(inst->data.binary_inst.lhs, iv->phi, scale)
                && opt_value_invariant_for_loop(function, in_loop, inst->data.binary_inst.rhs)) {
            *offset = inst->data.binary_inst.rhs;
            *subtract_offset = inst->kind == IR_INST_SUB;
            return true;
        }
        if (inst->kind == IR_INST_ADD
                && opt_match_iv_scale(inst->data.binary_inst.rhs, iv->phi, scale)
                && opt_value_invariant_for_loop(function, in_loop, inst->data.binary_inst.lhs)) {
            *offset = inst->data.binary_inst.lhs;
            *subtract_offset = false;
            return true;
        }
    }
    return false;
}

static IRValue *opt_build_strength_init(IRBasicBlock *preheader, IRInstruction *before,
                                        OptLoopInductionVar *iv, int scale,
                                        IRValue *offset, bool subtract_offset) {
    IRValue *value = iv->start_value;
    if (scale != 1) {
        IRInstruction *mul = opt_create_binary(preheader, before, IR_INST_MUL,
                                               iv->phi->result_type, value,
                                               opt_new_const_int(iv->phi->result_type, scale));
        value = &mul->result;
    }
    if (offset != NULL) {
        IRInstruction *offset_inst = opt_create_binary(preheader, before,
                                                       subtract_offset ? IR_INST_SUB : IR_INST_ADD,
                                                       iv->phi->result_type, value, offset);
        value = &offset_inst->result;
    }
    return value;
}

static bool opt_match_pointer_induction_index(IRValue *value, IRInstruction *phi,
                                              int *scale, IRValue **offset,
                                              bool *subtract_offset) {
    *scale = 0;
    *offset = NULL;
    *subtract_offset = false;
    if (opt_match_iv_scale(value, phi, scale)) {
        return true;
    }
    if (value == NULL || value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *inst = value->data.instruction;
    if (inst == NULL || inst->result_type == NULL || inst->result_type->kind != IR_TYPE_I32) {
        return false;
    }
    if (inst->kind == IR_INST_ADD || inst->kind == IR_INST_SUB) {
        if (opt_match_iv_scale(inst->data.binary_inst.lhs, phi, scale)) {
            *offset = inst->data.binary_inst.rhs;
            *subtract_offset = inst->kind == IR_INST_SUB;
            return true;
        }
        if (inst->kind == IR_INST_ADD && opt_match_iv_scale(inst->data.binary_inst.rhs, phi, scale)) {
            *offset = inst->data.binary_inst.lhs;
            *subtract_offset = false;
            return true;
        }
    }
    return false;
}

static bool opt_match_pointer_induction_gep(IRFunction *function, bool *in_loop,
                                            IRInstruction *inst, OptLoopInductionVar *iv,
                                            int *varying_index, int *scale,
                                            IRValue **offset, bool *subtract_offset) {
    *varying_index = -1;
    *scale = 0;
    *offset = NULL;
    *subtract_offset = false;
    if (inst == NULL || inst->kind != IR_INST_GETELEMENTPTR || inst->result_type == NULL
            || inst->result_type->kind != IR_TYPE_POINTER || inst->data.gep_inst.indices.count == 0) {
        return false;
    }
    if (!opt_all_uses_in_loop(function, in_loop, &inst->result)
            || !opt_value_invariant_for_loop(function, in_loop, inst->data.gep_inst.base_ptr)) {
        return false;
    }
    for (int i = 0; i < inst->data.gep_inst.indices.count; ++i) {
        IRValue *index = inst->data.gep_inst.indices.items[i];
        if (opt_value_invariant_for_loop(function, in_loop, index)) {
            continue;
        }
        if (i != inst->data.gep_inst.indices.count - 1 || *varying_index >= 0
                || !opt_match_pointer_induction_index(index, iv->phi, scale, offset, subtract_offset)) {
            return false;
        }
        if (*offset != NULL && !opt_value_invariant_for_loop(function, in_loop, *offset)) {
            return false;
        }
        *varying_index = i;
    }
    return *varying_index >= 0;
}

__attribute__((unused))
static bool opt_loop_pointer_induction(IRFunction *function, bool *in_loop,
                                       IRBasicBlock *header, IRBasicBlock *preheader,
                                       IRBasicBlock *latch) {
    bool changed = false;
    IRInstruction *insert_before_preheader = opt_preheader_insert_point(preheader);
    IRInstruction *insert_before_latch = latch != NULL ? opt_preheader_insert_point(latch) : NULL;
    IRInstruction **header_phis = NULL;
    int phi_count = 0;
    int phi_capacity = 0;
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        ensure_capacity((void **)&header_phis, &phi_capacity, sizeof(IRInstruction *), phi_count + 1);
        header_phis[phi_count++] = phi;
    }
    for (int phi_index = 0; phi_index < phi_count; ++phi_index) {
        IRInstruction *phi = header_phis[phi_index];
        bool normalized = false;
        OptLoopInductionVar iv = {0};
        if (!opt_match_loop_induction_var(function, in_loop, header, preheader, latch, phi, &iv, &normalized)) {
            changed = normalized || changed;
            continue;
        }
        changed = normalized || changed;
        for (int bi = 0; bi < function->blocks.count; ++bi) {
            if (!in_loop[bi]) {
                continue;
            }
            IRBasicBlock *block = function->blocks.items[bi];
            for (IRInstruction *inst = block->first_inst; inst != NULL;) {
                IRInstruction *next = inst->next;
                int varying_index = -1;
                int scale = 0;
                IRValue *offset = NULL;
                bool subtract_offset = false;
                if (opt_match_pointer_induction_gep(function, in_loop, inst, &iv, &varying_index,
                                                    &scale, &offset, &subtract_offset)) {
                    long long delta = (long long)iv.step * scale;
                    if (delta >= INT_MIN && delta <= INT_MAX && delta != 0) {
                        IRValue *init_index = opt_build_strength_init(preheader, insert_before_preheader,
                                                                      &iv, scale, offset, subtract_offset);
                        IRValueList init_indices = {0};
                        for (int i = 0; i < inst->data.gep_inst.indices.count; ++i) {
                            mem_value_list_push(&init_indices, i == varying_index
                                                                   ? init_index
                                                                   : inst->data.gep_inst.indices.items[i]);
                        }
                        IRType *result_pointee = inst->result_type->data.pointer.pointee;
                        if (result_pointee != NULL) {
                            IRInstruction *init_gep = opt_create_gep(preheader, insert_before_preheader,
                                                                     inst->data.gep_inst.base_ptr,
                                                                     inst->data.gep_inst.source_element_type,
                                                                     init_indices, result_pointee,
                                                                     inst->data.gep_inst.inbounds);
                            IRInstruction *derived_phi = opt_create_phi(header, inst->result_type);
                            IRValueList step_indices = {0};
                            mem_value_list_push(&step_indices,
                                                opt_new_const_int(iv.phi->result_type, (int)delta));
                            IRInstruction *next_gep = opt_create_gep(latch, insert_before_latch,
                                                                     &derived_phi->result, result_pointee,
                                                                     step_indices, result_pointee,
                                                                     inst->data.gep_inst.inbounds);
                            opt_phi_incoming_push(derived_phi, preheader, &init_gep->result);
                            opt_phi_incoming_push(derived_phi, latch, &next_gep->result);
                            opt_replace_uses_in_loop(function, in_loop, &inst->result, &derived_phi->result);
                            if (opt_count_uses(function, &inst->result) == 0) {
                                opt_remove_inst(block, inst);
                            }
                            changed = true;
                        }
                    }
                }
                inst = next;
            }
        }
    }
    free(header_phis);
    return changed;
}

static bool opt_loop_strength_reduction(IRFunction *function, bool *in_loop,
                                        IRBasicBlock *header, IRBasicBlock *preheader,
                                        IRBasicBlock *latch) {
    bool changed = false;
    IRInstruction *insert_before_preheader = opt_preheader_insert_point(preheader);
    IRInstruction *insert_before_latch = latch != NULL ? opt_preheader_insert_point(latch) : NULL;
    IRInstruction **header_phis = NULL;
    int phi_count = 0;
    int phi_capacity = 0;
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        ensure_capacity((void **)&header_phis, &phi_capacity, sizeof(IRInstruction *), phi_count + 1);
        header_phis[phi_count++] = phi;
    }
    for (int phi_index = 0; phi_index < phi_count; ++phi_index) {
        IRInstruction *phi = header_phis[phi_index];
        bool normalized = false;
        OptLoopInductionVar iv = {0};
        if (!opt_match_loop_induction_var(function, in_loop, header, preheader, latch, phi, &iv, &normalized)) {
            changed = normalized || changed;
            continue;
        }
        changed = normalized || changed;
        for (int bi = 0; bi < function->blocks.count; ++bi) {
            if (!in_loop[bi]) {
                continue;
            }
            IRBasicBlock *block = function->blocks.items[bi];
            for (IRInstruction *inst = block->first_inst; inst != NULL;) {
                IRInstruction *next = inst->next;
                int scale = 0;
                IRValue *offset = NULL;
                bool subtract_offset = false;
                if (opt_match_strength_reduction_expr(function, in_loop, inst, &iv,
                                                      &scale, &offset, &subtract_offset)) {
                    long long delta = (long long)iv.step * scale;
                    if (delta >= INT_MIN && delta <= INT_MAX) {
                        IRValue *init = opt_build_strength_init(preheader, insert_before_preheader,
                                                                &iv, scale, offset, subtract_offset);
                        if (delta == 0) {
                            opt_replace_uses_in_loop(function, in_loop, &inst->result, init);
                        } else {
                            IRInstruction *derived_phi = opt_create_phi(header, iv.phi->result_type);
                            IRInstruction *next_value = opt_create_binary(latch, insert_before_latch,
                                                                          IR_INST_ADD, iv.phi->result_type,
                                                                          &derived_phi->result,
                                                                          opt_new_const_int(iv.phi->result_type,
                                                                                            (int)delta));
                            opt_phi_incoming_push(derived_phi, preheader, init);
                            opt_phi_incoming_push(derived_phi, latch, &next_value->result);
                            opt_replace_uses_in_loop(function, in_loop, &inst->result, &derived_phi->result);
                        }
                        if (opt_count_uses(function, &inst->result) == 0) {
                            opt_remove_inst(block, inst);
                        }
                        changed = true;
                    }
                }
                inst = next;
            }
        }
    }
    free(header_phis);
    return changed;
}

static IRInstruction *opt_clone_inst_with_map(IRBasicBlock *block, IRInstruction *before,
                                              IRInstruction *src, OptValueMap *map) {
    switch (src->kind) {
        case IR_INST_LOAD:
            return opt_create_load(block, before,
                                   src->data.load_inst.value_type,
                                   opt_value_map_get(map, src->data.load_inst.ptr),
                                   src->data.load_inst.alignment);
        case IR_INST_STORE:
            return opt_create_store(block, before, src->result_type,
                                    opt_value_map_get(map, src->data.store_inst.value),
                                    opt_value_map_get(map, src->data.store_inst.ptr),
                                    src->data.store_inst.alignment);
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            return opt_create_binary(block, before, src->kind, src->result_type,
                                     opt_value_map_get(map, src->data.binary_inst.lhs),
                                     opt_value_map_get(map, src->data.binary_inst.rhs));
        case IR_INST_ICMP:
            return opt_create_icmp(block, before,
                                   src->data.icmp_inst.pred,
                                   opt_value_map_get(map, src->data.icmp_inst.lhs),
                                   opt_value_map_get(map, src->data.icmp_inst.rhs),
                                   src->result_type);
        case IR_INST_FCMP:
            return opt_create_fcmp(block, before,
                                   src->data.fcmp_inst.pred,
                                   opt_value_map_get(map, src->data.fcmp_inst.lhs),
                                   opt_value_map_get(map, src->data.fcmp_inst.rhs),
                                   src->result_type);
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            return opt_create_cast(block, before, src->kind,
                                   opt_value_map_get(map, src->data.cast_inst.value),
                                   src->data.cast_inst.to_type);
        case IR_INST_CALL: {
            IRValueList args = {0};
            for (int i = 0; i < src->data.call_inst.args.count; ++i) {
                mem_value_list_push(&args, opt_value_map_get(map, src->data.call_inst.args.items[i]));
            }
            return opt_create_call(block, before, src->data.call_inst.callee,
                                   src->data.call_inst.ret_type, args);
        }
        case IR_INST_GETELEMENTPTR: {
            IRValueList indices = {0};
            for (int i = 0; i < src->data.gep_inst.indices.count; ++i) {
                mem_value_list_push(&indices, opt_value_map_get(map, src->data.gep_inst.indices.items[i]));
            }
            IRType *result_pointee = src->result_type->data.pointer.pointee;
            return opt_create_gep(block, before,
                                  opt_value_map_get(map, src->data.gep_inst.base_ptr),
                                  src->data.gep_inst.source_element_type,
                                  indices, result_pointee, src->data.gep_inst.inbounds);
        }
        case IR_INST_BITCAST:
            return opt_create_bitcast(block, before,
                                      opt_value_map_get(map, src->data.bitcast_inst.value),
                                      src->data.bitcast_inst.to_type);
        default:
            return NULL;
    }
}

static bool opt_try_unroll_small_loop(IRFunction *function, bool *in_loop,
                                      IRBasicBlock *header, IRBasicBlock *preheader,
                                      IRBasicBlock *latch) {
    if (opt_loop_block_count(function, in_loop) != 2 || latch == NULL || latch == header) {
        return false;
    }
    if (header->last_inst == NULL || header->last_inst->kind != IR_INST_BR
            || !header->last_inst->data.br_inst.is_conditional) {
        return false;
    }
    if (latch->last_inst == NULL || latch->last_inst->kind != IR_INST_BR
            || latch->last_inst->data.br_inst.is_conditional
            || latch->last_inst->data.br_inst.true_block != header) {
        return false;
    }
    IRBasicBlock *body = NULL;
    IRBasicBlock *exit_block = NULL;
    if (header->last_inst->data.br_inst.true_block == latch
            && header->last_inst->data.br_inst.false_block != NULL
            && header->last_inst->data.br_inst.false_block != latch) {
        body = latch;
        exit_block = header->last_inst->data.br_inst.false_block;
    } else if (header->last_inst->data.br_inst.false_block == latch
            && header->last_inst->data.br_inst.true_block != NULL
            && header->last_inst->data.br_inst.true_block != latch) {
        body = latch;
        exit_block = header->last_inst->data.br_inst.true_block;
    }
    if (body == NULL || exit_block == NULL || opt_block_non_phi_non_term_inst_count(body) > 24) {
        return false;
    }
    IRValue *cond = header->last_inst->data.br_inst.condition;
    if (cond == NULL || cond->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *cmp = cond->data.instruction;
    if (cmp == NULL || cmp->parent != header || cmp->kind != IR_INST_ICMP) {
        return false;
    }
    OptLoopInductionVar iv = {0};
    bool matched_iv = false;
    bool normalized = false;
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        OptLoopInductionVar candidate = {0};
        bool phi_normalized = false;
        if (!opt_match_loop_induction_var(function, in_loop, header, preheader, latch, phi,
                                          &candidate, &phi_normalized)) {
            normalized = phi_normalized || normalized;
            continue;
        }
        normalized = phi_normalized || normalized;
        if (cmp->data.icmp_inst.lhs == &phi->result || cmp->data.icmp_inst.rhs == &phi->result) {
            iv = candidate;
            matched_iv = true;
            break;
        }
    }
    if (!matched_iv) {
        return normalized;
    }
    if (cmp->data.icmp_inst.rhs == &iv.phi->result) {
        cmp->data.icmp_inst.pred = opt_swap_icmp_pred(cmp->data.icmp_inst.pred);
        IRValue *tmp = cmp->data.icmp_inst.lhs;
        cmp->data.icmp_inst.lhs = cmp->data.icmp_inst.rhs;
        cmp->data.icmp_inst.rhs = tmp;
        normalized = true;
    }
    if (cmp->data.icmp_inst.lhs != &iv.phi->result) {
        return normalized;
    }
    int start = 0;
    int bound = 0;
    int trip_count = 0;
    if (!opt_const_int_value(iv.start_value, &start)
            || !opt_const_int_value(cmp->data.icmp_inst.rhs, &bound)
            || !opt_compute_small_trip_count(cmp->data.icmp_inst.pred, start, bound, iv.step, 4, &trip_count)
            || trip_count <= 0) {
        return normalized;
    }
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        if (phi->data.phi_inst.count != 2
                || opt_phi_incoming_value(phi, preheader) == NULL
                || opt_phi_incoming_value(phi, latch) == NULL) {
            return normalized;
        }
    }
    IRInstruction *insert_before = opt_preheader_insert_point(preheader);
    OptValueMap state = {0};
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        opt_value_map_push(&state, &phi->result, opt_phi_incoming_value(phi, preheader));
    }
    for (int iter = 0; iter < trip_count; ++iter) {
        OptValueMap iter_map = {0};
        for (int i = 0; i < state.count; ++i) {
            opt_value_map_push(&iter_map, state.from[i], state.to[i]);
        }
        for (IRInstruction *inst = body->first_inst; inst != NULL; inst = inst->next) {
            if (inst->kind == IR_INST_BR) {
                continue;
            }
            IRInstruction *clone = opt_clone_inst_with_map(preheader, insert_before, inst, &iter_map);
            if (clone == NULL) {
                opt_value_map_reset(&iter_map);
                opt_value_map_reset(&state);
                return normalized;
            }
            if (clone->result_type != NULL && clone->result_type->kind != IR_TYPE_VOID) {
                opt_value_map_push(&iter_map, &inst->result, &clone->result);
            }
        }
        OptValueMap next_state = {0};
        for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
            IRValue *next_value = opt_value_map_get(&iter_map, opt_phi_incoming_value(phi, latch));
            opt_value_map_push(&next_state, &phi->result, next_value);
        }
        opt_value_map_reset(&iter_map);
        opt_value_map_reset(&state);
        state = next_state;
    }
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        IRValue *final_value = opt_value_map_get(&state, &phi->result);
        opt_phi_incoming_push(phi, preheader, final_value);
    }
    opt_value_map_reset(&state);
    (void)exit_block;
    return true;
}

__attribute__((unused))
static bool opt_try_partial_unroll_loop(IRFunction *function, bool *in_loop,
                                        IRBasicBlock *header, IRBasicBlock *preheader,
                                        IRBasicBlock *latch) {
    if (opt_loop_block_count(function, in_loop) != 2 || latch == NULL || latch == header) {
        return false;
    }
    if (header->last_inst == NULL || header->last_inst->kind != IR_INST_BR
            || !header->last_inst->data.br_inst.is_conditional) {
        return false;
    }
    if (latch->last_inst == NULL || latch->last_inst->kind != IR_INST_BR
            || latch->last_inst->data.br_inst.is_conditional
            || latch->last_inst->data.br_inst.true_block != header) {
        return false;
    }
    IRBasicBlock *body = NULL;
    if (header->last_inst->data.br_inst.true_block == latch
            && header->last_inst->data.br_inst.false_block != NULL
            && header->last_inst->data.br_inst.false_block != latch) {
        body = latch;
    } else if (header->last_inst->data.br_inst.false_block == latch
            && header->last_inst->data.br_inst.true_block != NULL
            && header->last_inst->data.br_inst.true_block != latch) {
        body = latch;
    }
    if (body == NULL) {
        return false;
    }
    int body_inst_count = opt_block_non_phi_non_term_inst_count(body);
    if (body_inst_count <= 0 || body_inst_count > 24) {
        return false;
    }
    IRValue *cond = header->last_inst->data.br_inst.condition;
    if (cond == NULL || cond->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *cmp = cond->data.instruction;
    if (cmp == NULL || cmp->parent != header || cmp->kind != IR_INST_ICMP) {
        return false;
    }
    OptLoopInductionVar iv = {0};
    bool matched_iv = false;
    bool normalized = false;
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        OptLoopInductionVar candidate = {0};
        bool phi_normalized = false;
        if (!opt_match_loop_induction_var(function, in_loop, header, preheader, latch, phi,
                                          &candidate, &phi_normalized)) {
            normalized = phi_normalized || normalized;
            continue;
        }
        normalized = phi_normalized || normalized;
        if (cmp->data.icmp_inst.lhs == &phi->result || cmp->data.icmp_inst.rhs == &phi->result) {
            iv = candidate;
            matched_iv = true;
            break;
        }
    }
    if (!matched_iv) {
        return normalized;
    }
    if (cmp->data.icmp_inst.rhs == &iv.phi->result) {
        cmp->data.icmp_inst.pred = opt_swap_icmp_pred(cmp->data.icmp_inst.pred);
        IRValue *tmp = cmp->data.icmp_inst.lhs;
        cmp->data.icmp_inst.lhs = cmp->data.icmp_inst.rhs;
        cmp->data.icmp_inst.rhs = tmp;
        normalized = true;
    }
    if (cmp->data.icmp_inst.lhs != &iv.phi->result) {
        return normalized;
    }
    int start = 0;
    int bound = 0;
    int trip_count = 0;
    if (!opt_const_int_value(iv.start_value, &start)
            || !opt_const_int_value(cmp->data.icmp_inst.rhs, &bound)
            || !opt_compute_small_trip_count(cmp->data.icmp_inst.pred, start, bound, iv.step, 256, &trip_count)
            || trip_count < 8) {
        return normalized;
    }
    int unroll_factor = 0;
    if (trip_count % 8 == 0 && body_inst_count <= 16 && body_inst_count * 8 <= 128) {
        unroll_factor = 8;
    } else if (trip_count % 4 == 0 && body_inst_count <= 24 && body_inst_count * 4 <= 128) {
        unroll_factor = 4;
    } else {
        return normalized;
    }
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        if (phi->data.phi_inst.count != 2
                || opt_phi_incoming_value(phi, preheader) == NULL
                || opt_phi_incoming_value(phi, latch) == NULL) {
            return normalized;
        }
    }
    IRInstruction *insert_before = opt_preheader_insert_point(latch);
    if (insert_before == NULL) {
        return normalized;
    }
    IRInstruction **body_insts = NULL;
    int body_count = 0;
    int body_capacity = 0;
    for (IRInstruction *inst = body->first_inst; inst != NULL; inst = inst->next) {
        if (inst->kind == IR_INST_BR) {
            continue;
        }
        ensure_capacity((void **)&body_insts, &body_capacity, sizeof(IRInstruction *), body_count + 1);
        body_insts[body_count++] = inst;
    }
    OptValueMap state = {0};
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        opt_value_map_push(&state, &phi->result, opt_phi_incoming_value(phi, latch));
    }
    for (int iter = 1; iter < unroll_factor; ++iter) {
        OptValueMap iter_map = {0};
        for (int i = 0; i < state.count; ++i) {
            opt_value_map_push(&iter_map, state.from[i], state.to[i]);
        }
        for (int inst_index = 0; inst_index < body_count; ++inst_index) {
            IRInstruction *inst = body_insts[inst_index];
            IRInstruction *clone = opt_clone_inst_with_map(latch, insert_before, inst, &iter_map);
            if (clone == NULL) {
                opt_value_map_reset(&iter_map);
                opt_value_map_reset(&state);
                free(body_insts);
                return normalized;
            }
            if (clone->result_type != NULL && clone->result_type->kind != IR_TYPE_VOID) {
                opt_value_map_push(&iter_map, &inst->result, &clone->result);
            }
        }
        OptValueMap next_state = {0};
        for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
            IRValue *next_value = opt_value_map_get(&iter_map, opt_phi_incoming_value(phi, latch));
            opt_value_map_push(&next_state, &phi->result, next_value);
        }
        opt_value_map_reset(&iter_map);
        opt_value_map_reset(&state);
        state = next_state;
    }
    for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        int latch_index = opt_phi_incoming_index(phi, latch);
        if (latch_index >= 0) {
            phi->data.phi_inst.values.items[latch_index] = opt_value_map_get(&state, &phi->result);
        }
    }
    opt_value_map_reset(&state);
    free(body_insts);
    return true;
}

static bool opt_loop_licm(IRFunction *function, bool *in_loop, IRBasicBlock *preheader) {
    bool changed = false;
    IRInstruction *insert_before = opt_preheader_insert_point(preheader);
    bool moved = true;
    while (moved) {
        moved = false;
        for (int bi = 0; bi < function->blocks.count; ++bi) {
            if (!in_loop[bi]) {
                continue;
            }
            IRBasicBlock *block = function->blocks.items[bi];
            for (IRInstruction *inst = block->first_inst; inst != NULL;) {
                IRInstruction *next = inst->next;
                if (opt_inst_loop_invariant(function, in_loop, inst)) {
                    opt_move_inst_before(preheader, insert_before, inst);
                    moved = true;
                    changed = true;
                }
                inst = next;
            }
        }
    }
    return changed;
}

static bool opt_loop_cse(IRFunction *function, bool *dom, bool *in_loop) {
    bool changed = false;
    OptCSEntryList cse = {0};
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        if (!in_loop[bi]) {
            continue;
        }
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL;) {
            IRInstruction *next = inst->next;
            if (opt_loop_safe_pure_inst(inst)) {
                IRInstruction *existing = NULL;
                for (int i = 0; i < cse.count; ++i) {
                    IRInstruction *candidate = cse.items[i].inst;
                    if (opt_inst_same_expr(candidate, inst)
                            && opt_inst_dominates_inst(function, dom, candidate, inst)) {
                        existing = candidate;
                        break;
                    }
                }
                if (existing != NULL) {
                    opt_replace_all_uses(function, &inst->result, &existing->result);
                    opt_remove_inst(block, inst);
                    changed = true;
                } else {
                    opt_cse_list_push(&cse, inst);
                }
            }
            inst = next;
        }
    }
    free(cse.items);
    return changed;
}

static bool opt_loop_pass(IRFunction *function) {
    bool changed = false;
    bool *dom = opt_compute_dominators(function);
    for (int hi = 0; hi < function->blocks.count; ++hi) {
        IRBasicBlock *header = function->blocks.items[hi];
        for (int pi = 0; pi < header->pred_count; ++pi) {
            IRBasicBlock *latch = header->preds[pi];
            int latch_index = opt_function_block_index(function, latch);
            if (latch_index < 0 || !dom[latch_index * function->blocks.count + hi]) {
                continue;
            }
            bool *in_loop = opt_collect_natural_loop(function, header, latch);
            IRBasicBlock *preheader = opt_find_preheader(function, in_loop, header);
            if (preheader != NULL) {
                IRBasicBlock *unique_latch = opt_find_unique_loop_latch(function, in_loop, header);
                if (unique_latch != NULL) {
                    changed = opt_try_unroll_small_loop(function, in_loop, header, preheader, unique_latch) || changed;
                    changed = opt_loop_strength_reduction(function, in_loop, header, preheader, unique_latch) || changed;
                }
                changed = opt_loop_cse(function, dom, in_loop) || changed;
                changed = opt_loop_licm(function, in_loop, preheader) || changed;
            }
            free(in_loop);
        }
    }
    free(dom);
    return changed;
}

static bool opt_simplify_function(IRFunction *function) {
    bool changed = false;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        changed = opt_basic_block(function, function->blocks.items[bi]) || changed;
    }
    changed = opt_delete_dead_pure_insts(function) || changed;
    changed = opt_delete_after_terminator(function) || changed;
    return changed;
}

static void optimize_ir(IRModule *module) {
    for (int fi = 0; fi < module->functions.count; ++fi) {
        IRFunction *function = module->functions.items[fi];
        if (function->is_external) {
            continue;
        }
        bool changed = true;
        int iteration = 0;
        while (changed && iteration < 8) {
            changed = false;
            changed = opt_global_memory_pass(function) || changed;
            changed = opt_loop_pass(function) || changed;
            changed = opt_simplify_function(function) || changed;
            ++iteration;
        }
    }
}

static void optimize_ir_basic_blocks(IRModule *module) {
    optimize_ir(module);
}

static void generate_program_mem_ir(Program *program, FILE *out) {
    IRModule *module = ast_to_ir(program);
    optimize_ir_basic_blocks(module);
    dump_mem_ir(module, out);
}

void generate_program_ir(Program *program, FILE *out) {
    (void)ast_to_ir(program);
    generate_program_llvm_text(program, out);
}

typedef struct {
    StrBuf out;
    int temp_id;
    int label_id;
    StringList break_labels;
    StringList continue_labels;
    bool current_block_terminated;
} MidIRGen;

static const char *mid_type_name(TypeSpec type) {
    switch (type) {
        case TYPE_INT: return "i32";
        case TYPE_FLOAT: return "f32";
        case TYPE_VOID: return "void";
    }
    return "i32";
}

static const char *mid_unary_name(UnaryOp op) {
    switch (op) {
        case UNARY_PLUS: return "pos";
        case UNARY_MINUS: return "neg";
        case UNARY_NOT: return "not";
    }
    return "unary";
}

static const char *mid_binary_name(BinaryOp op) {
    switch (op) {
        case BIN_ADD: return "add";
        case BIN_SUB: return "sub";
        case BIN_MUL: return "mul";
        case BIN_DIV: return "div";
        case BIN_MOD: return "mod";
        case BIN_LT: return "lt";
        case BIN_GT: return "gt";
        case BIN_LE: return "le";
        case BIN_GE: return "ge";
        case BIN_EQ: return "eq";
        case BIN_NE: return "ne";
        case BIN_AND: return "and";
        case BIN_OR: return "or";
    }
    return "bin";
}

static char *mid_new_temp(MidIRGen *gen) {
    return str_printf("%%m%d", gen->temp_id++);
}

static char *mid_new_label(MidIRGen *gen, const char *prefix) {
    return str_printf(".M_%s_%d", prefix, gen->label_id++);
}

static void mid_emit(MidIRGen *gen, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vappendf(&gen->out, fmt, ap);
    va_end(ap);
}

static void mid_mark_label(MidIRGen *gen, const char *label) {
    mid_emit(gen, "%s:\n", label);
    gen->current_block_terminated = false;
}

static char *mid_dims_text(const IntList *dims, bool param_array) {
    StrBuf sb;
    sb_init(&sb);
    if (param_array) {
        sb_append(&sb, "[]");
    }
    for (int i = 0; i < dims->count; ++i) {
        sb_appendf(&sb, "[%d]", dims->data[i]);
    }
    return sb.data ? sb.data : xstrdup("");
}

static char *mid_gen_expr(MidIRGen *gen, Expr *expr);
static void mid_gen_cond(MidIRGen *gen, Expr *expr, const char *true_label, const char *false_label);
static void mid_gen_stmt(MidIRGen *gen, Stmt *stmt);
static void mid_gen_decl(MidIRGen *gen, Decl *decl, const char *storage);

static char *mid_lval_ref(MidIRGen *gen, LVal *lval) {
    StrBuf sb;
    sb_init(&sb);
    sb_append(&sb, lval->name);
    for (int i = 0; i < lval->indices.count; ++i) {
        char *idx = mid_gen_expr(gen, lval->indices.items[i]);
        sb_appendf(&sb, "[%s]", idx);
    }
    return sb.data ? sb.data : xstrdup(lval->name);
}

static char *mid_expr_text(Expr *expr) {
    if (expr == NULL) {
        return xstrdup("0");
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return str_printf("%d", expr->data.number);
        case EXPR_FLOAT_NUMBER:
            return str_printf("f32bits(%u)", (unsigned)expr->data.number);
        case EXPR_LVAL: {
            StrBuf sb;
            sb_init(&sb);
            LVal *lval = expr->data.lval;
            sb_append(&sb, lval->name);
            for (int i = 0; i < lval->indices.count; ++i) {
                char *idx = mid_expr_text(lval->indices.items[i]);
                sb_appendf(&sb, "[%s]", idx);
            }
            return sb.data ? sb.data : xstrdup(lval->name);
        }
        case EXPR_UNARY: {
            char *operand = mid_expr_text(expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                return operand;
            }
            return str_printf("(%s %s)", mid_unary_name(expr->data.unary.op), operand);
        }
        case EXPR_BINARY: {
            char *lhs = mid_expr_text(expr->data.binary.lhs);
            char *rhs = mid_expr_text(expr->data.binary.rhs);
            return str_printf("(%s %s, %s)", mid_binary_name(expr->data.binary.op), lhs, rhs);
        }
        case EXPR_GETINT:
            return xstrdup("call getint()");
        case EXPR_CALL: {
            StrBuf args;
            sb_init(&args);
            for (int i = 0; i < expr->data.call.args.count; ++i) {
                char *arg = mid_expr_text(expr->data.call.args.items[i]);
                if (i > 0) {
                    sb_append(&args, ", ");
                }
                sb_append(&args, arg);
            }
            return str_printf("call %s(%s)", expr->data.call.name, args.data ? args.data : "");
        }
    }
    return xstrdup("0");
}

static char *mid_gen_logic_value(MidIRGen *gen, Expr *expr) {
    char *slot = mid_new_temp(gen);
    char *res = mid_new_temp(gen);
    char *true_label = mid_new_label(gen, "logic_true");
    char *false_label = mid_new_label(gen, "logic_false");
    char *end_label = mid_new_label(gen, "logic_end");
    mid_emit(gen, "  %s = local i32\n", slot);
    mid_gen_cond(gen, expr, true_label, false_label);
    mid_mark_label(gen, true_label);
    mid_emit(gen, "  store 1, %s\n", slot);
    mid_emit(gen, "  j %s\n", end_label);
    gen->current_block_terminated = true;
    mid_mark_label(gen, false_label);
    mid_emit(gen, "  store 0, %s\n", slot);
    mid_emit(gen, "  j %s\n", end_label);
    gen->current_block_terminated = true;
    mid_mark_label(gen, end_label);
    mid_emit(gen, "  %s = load %s\n", res, slot);
    return res;
}

static char *mid_gen_expr(MidIRGen *gen, Expr *expr) {
    if (expr == NULL) {
        return xstrdup("0");
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return str_printf("%d", expr->data.number);
        case EXPR_FLOAT_NUMBER:
            return str_printf("f32bits(%u)", (unsigned)expr->data.number);
        case EXPR_LVAL: {
            char *ref = mid_lval_ref(gen, expr->data.lval);
            char *tmp = mid_new_temp(gen);
            mid_emit(gen, "  %s = load %s\n", tmp, ref);
            return tmp;
        }
        case EXPR_GETINT: {
            char *tmp = mid_new_temp(gen);
            mid_emit(gen, "  %s = call getint()\n", tmp);
            return tmp;
        }
        case EXPR_CALL: {
            StrBuf args;
            sb_init(&args);
            for (int i = 0; i < expr->data.call.args.count; ++i) {
                char *arg = mid_gen_expr(gen, expr->data.call.args.items[i]);
                if (i > 0) {
                    sb_append(&args, ", ");
                }
                sb_append(&args, arg);
            }
            char *tmp = mid_new_temp(gen);
            mid_emit(gen, "  %s = call %s(%s)\n", tmp, expr->data.call.name, args.data ? args.data : "");
            return tmp;
        }
        case EXPR_UNARY: {
            char *operand = mid_gen_expr(gen, expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                return operand;
            }
            char *tmp = mid_new_temp(gen);
            mid_emit(gen, "  %s = %s %s\n", tmp, mid_unary_name(expr->data.unary.op), operand);
            return tmp;
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                return mid_gen_logic_value(gen, expr);
            }
            char *lhs = mid_gen_expr(gen, expr->data.binary.lhs);
            char *rhs = mid_gen_expr(gen, expr->data.binary.rhs);
            char *tmp = mid_new_temp(gen);
            mid_emit(gen, "  %s = %s %s, %s\n", tmp, mid_binary_name(op), lhs, rhs);
            return tmp;
        }
    }
    return xstrdup("0");
}

static void mid_gen_cond(MidIRGen *gen, Expr *expr, const char *true_label, const char *false_label) {
    if (expr != NULL && expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_OR) {
        char *rhs_label = mid_new_label(gen, "lor_rhs");
        mid_gen_cond(gen, expr->data.binary.lhs, true_label, rhs_label);
        mid_mark_label(gen, rhs_label);
        mid_gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    if (expr != NULL && expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_AND) {
        char *rhs_label = mid_new_label(gen, "land_rhs");
        mid_gen_cond(gen, expr->data.binary.lhs, rhs_label, false_label);
        mid_mark_label(gen, rhs_label);
        mid_gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    char *value = mid_gen_expr(gen, expr);
    mid_emit(gen, "  br %s, %s, %s\n", value, true_label, false_label);
    gen->current_block_terminated = true;
}

static void mid_gen_init_marker(MidIRGen *gen, const char *name, InitVal *init) {
    if (init == NULL) {
        return;
    }
    if (init->is_expr) {
        char *value = mid_gen_expr(gen, init->expr);
        mid_emit(gen, "  store %s, %s\n", value, name);
    } else {
        mid_emit(gen, "  init.aggregate %s\n", name);
    }
}

static void mid_gen_decl(MidIRGen *gen, Decl *decl, const char *storage) {
    for (int i = 0; i < decl->items.count; ++i) {
        DeclItem *item = decl->items.items[i];
        char *dims = mid_dims_text(&item->dims, false);
        mid_emit(gen, "  %s %s %s%s", storage, mid_type_name(decl->type), item->name, dims);
        if (decl->is_const) {
            mid_emit(gen, " const");
        }
        if (strcmp(storage, "global") == 0) {
            if (item->init == NULL) {
                mid_emit(gen, " = zero");
            } else if (item->init->is_expr) {
                char *value = mid_expr_text(item->init->expr);
                mid_emit(gen, " = %s", value);
            } else {
                mid_emit(gen, " = aggregate");
            }
        }
        mid_emit(gen, "\n");
        if (strcmp(storage, "global") != 0) {
            mid_gen_init_marker(gen, item->name, item->init);
        }
    }
}

static void mid_gen_block(MidIRGen *gen, Block *block) {
    for (int i = 0; i < block->items.count; ++i) {
        if (block->items.kinds[i] == BLOCK_ITEM_DECL) {
            mid_gen_decl(gen, (Decl *)block->items.items[i], "local");
        } else {
            mid_gen_stmt(gen, (Stmt *)block->items.items[i]);
        }
    }
}

static void mid_gen_stmt(MidIRGen *gen, Stmt *stmt) {
    switch (stmt->kind) {
        case STMT_ASSIGN: {
            char *ref = mid_lval_ref(gen, stmt->data.assign_stmt.lval);
            char *value = mid_gen_expr(gen, stmt->data.assign_stmt.expr);
            mid_emit(gen, "  store %s, %s\n", value, ref);
            return;
        }
        case STMT_EXPR:
            if (stmt->data.expr_stmt != NULL) {
                char *value = mid_gen_expr(gen, stmt->data.expr_stmt);
                mid_emit(gen, "  drop %s\n", value);
            }
            return;
        case STMT_BLOCK:
            mid_emit(gen, "  scope.begin\n");
            mid_gen_block(gen, stmt->data.block_stmt);
            mid_emit(gen, "  scope.end\n");
            return;
        case STMT_IF: {
            char *then_label = mid_new_label(gen, "if_then");
            char *else_label = mid_new_label(gen, "if_else");
            char *end_label = mid_new_label(gen, "if_end");
            mid_gen_cond(gen, stmt->data.if_stmt.cond, then_label,
                         stmt->data.if_stmt.else_stmt ? else_label : end_label);
            mid_mark_label(gen, then_label);
            mid_gen_stmt(gen, stmt->data.if_stmt.then_stmt);
            if (!gen->current_block_terminated) {
                mid_emit(gen, "  j %s\n", end_label);
                gen->current_block_terminated = true;
            }
            if (stmt->data.if_stmt.else_stmt != NULL) {
                mid_mark_label(gen, else_label);
                mid_gen_stmt(gen, stmt->data.if_stmt.else_stmt);
                if (!gen->current_block_terminated) {
                    mid_emit(gen, "  j %s\n", end_label);
                    gen->current_block_terminated = true;
                }
            }
            mid_mark_label(gen, end_label);
            return;
        }
        case STMT_WHILE: {
            char *cond_label = mid_new_label(gen, "while_cond");
            char *body_label = mid_new_label(gen, "while_body");
            char *end_label = mid_new_label(gen, "while_end");
            mid_emit(gen, "  j %s\n", cond_label);
            gen->current_block_terminated = true;
            mid_mark_label(gen, cond_label);
            string_list_push(&gen->break_labels, end_label);
            string_list_push(&gen->continue_labels, cond_label);
            mid_gen_cond(gen, stmt->data.while_stmt.cond, body_label, end_label);
            mid_mark_label(gen, body_label);
            mid_gen_stmt(gen, stmt->data.while_stmt.body);
            if (!gen->current_block_terminated) {
                mid_emit(gen, "  j %s\n", cond_label);
                gen->current_block_terminated = true;
            }
            gen->break_labels.count--;
            gen->continue_labels.count--;
            mid_mark_label(gen, end_label);
            return;
        }
        case STMT_BREAK:
            if (gen->break_labels.count > 0) {
                mid_emit(gen, "  j %s\n", gen->break_labels.items[gen->break_labels.count - 1]);
                gen->current_block_terminated = true;
            }
            return;
        case STMT_CONTINUE:
            if (gen->continue_labels.count > 0) {
                mid_emit(gen, "  j %s\n", gen->continue_labels.items[gen->continue_labels.count - 1]);
                gen->current_block_terminated = true;
            }
            return;
        case STMT_RETURN:
            if (stmt->data.return_expr == NULL) {
                mid_emit(gen, "  ret\n");
            } else {
                char *value = mid_gen_expr(gen, stmt->data.return_expr);
                mid_emit(gen, "  ret %s\n", value);
            }
            gen->current_block_terminated = true;
            return;
        case STMT_PRINTF: {
            StrBuf args;
            sb_init(&args);
            for (int i = 0; i < stmt->data.printf_stmt.args.count; ++i) {
                char *arg = mid_gen_expr(gen, stmt->data.printf_stmt.args.items[i]);
                if (i > 0) {
                    sb_append(&args, ", ");
                }
                sb_append(&args, arg);
            }
            mid_emit(gen, "  call printf(%s%s%s)\n",
                     stmt->data.printf_stmt.format,
                     stmt->data.printf_stmt.args.count > 0 ? ", " : "",
                     args.data ? args.data : "");
            return;
        }
    }
}

static void mid_gen_function(MidIRGen *gen, FuncDef *func) {
    gen->current_block_terminated = false;
    mid_emit(gen, "func %s %s(", mid_type_name(func->ret_type), func->name);
    for (int i = 0; i < func->params.count; ++i) {
        Param *param = func->params.items[i];
        char *dims = mid_dims_text(&param->dims, param->is_array);
        if (i > 0) {
            mid_emit(gen, ", ");
        }
        mid_emit(gen, "%s %s%s", mid_type_name(param->type), param->name, dims);
    }
    mid_emit(gen, "):\n");
    mid_mark_label(gen, "entry");
    mid_gen_block(gen, func->block);
    if (!gen->current_block_terminated) {
        if (func->ret_type == TYPE_VOID) {
            mid_emit(gen, "  ret\n");
        } else {
            mid_emit(gen, "  ret 0\n");
        }
    }
    mid_emit(gen, "endfunc\n\n");
}

void generate_program_mid_ir(Program *program, FILE *out) {
    MidIRGen gen;
    memset(&gen, 0, sizeof(gen));
    sb_init(&gen.out);
    mid_emit(&gen, "module sysy.mid\n\n");
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_DECL) {
            mid_gen_decl(&gen, item->data.decl, "global");
        }
    }
    if (gen.out.len > 0 && gen.out.data[gen.out.len - 1] != '\n') {
        mid_emit(&gen, "\n");
    }
    mid_emit(&gen, "\n");
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            mid_gen_function(&gen, item->data.func);
        }
    }
    fputs(gen.out.data ? gen.out.data : "", out);
}

int yyparse(void);

static bool has_flag(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

static const char *parse_output_path(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            return argv[i + 1];
        }
    }
    return "output.s";
}

static const char *parse_input_path(int argc, char **argv) {
    const char *input = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            ++i;
            continue;
        }
        if (argv[i][0] != '-') {
            input = argv[i];
        }
    }
    return input;
}

static int align_to(int value, int align) {
    return (value + align - 1) / align * align;
}

typedef struct RVSlot {
    IRValue *value;
    int offset;
    int object_offset;
    int object_size;
    int value_size;
    struct RVSlot *next;
} RVSlot;

typedef struct RVRegTemp {
    IRValue *value;
    IRInstruction *def_inst;
    IRInstruction *use_inst;
    const char *reg;
    bool persistent;
    struct RVRegTemp *next;
} RVRegTemp;

typedef struct {
    IRFunction *function;
    RVSlot *slots;
    RVRegTemp *reg_temps;
    int next_offset;
    int frame_size;
    int max_outgoing_args;
    int phi_scratch_offset;
    int phi_scratch_slots;
    int phi_label_id;
    int saved_reg_offsets[6];
    bool used_saved_regs[6];
    FILE *out;
} RVFrame;

typedef struct {
    IRValue *value;
    IRInstruction *phi;
    IRInstruction *update_inst;
    IRBasicBlock *latch;
    int score;
} RVLoopRegCandidate;

static const char *g_rv_loop_regs[] = {"s1", "s2", "s3", "s4", "s5", "s6"};

static const char *rv_symbol_name(const char *name) {
    if (name == NULL) {
        return "";
    }
    return name[0] == '@' ? name + 1 : name;
}

static char *rv_block_label(IRFunction *function, IRBasicBlock *block) {
    return str_printf(".L_%s_%s", rv_symbol_name(function->name), block->name);
}

static int rv_type_size(IRType *type) {
    if (type == NULL) {
        return 8;
    }
    switch (type->kind) {
        case IR_TYPE_VOID:
            return 0;
        case IR_TYPE_I1:
        case IR_TYPE_I32:
        case IR_TYPE_FLOAT:
            return 4;
        case IR_TYPE_POINTER:
        case IR_TYPE_FUNCTION:
            return 8;
        case IR_TYPE_ARRAY:
            return type->data.array.length * rv_type_size(type->data.array.element);
    }
    return 8;
}

static bool rv_value_is_float(IRValue *value) {
    return value != NULL && value->ptr_level == 0 && value->base_type == TYPE_FLOAT;
}

static bool rv_value_is_pointer(IRValue *value) {
    return value != NULL && value->ptr_level > 0;
}

static int rv_alloc_frame_bytes(RVFrame *frame, int bytes) {
    bytes = align_to(bytes <= 0 ? 4 : bytes, 4);
    int alignment = bytes >= 8 ? 8 : 4;
    frame->next_offset = -align_to(-(frame->next_offset - bytes), alignment);
    return frame->next_offset;
}

static RVSlot *rv_find_slot(RVFrame *frame, IRValue *value) {
    if (value == NULL) {
        return NULL;
    }
    for (RVSlot *slot = frame->slots; slot != NULL; slot = slot->next) {
        if (slot->value == value) {
            return slot;
        }
    }
    return NULL;
}

static RVRegTemp *rv_find_reg_temp(RVFrame *frame, IRValue *value) {
    if (value == NULL) {
        return NULL;
    }
    for (RVRegTemp *temp = frame->reg_temps; temp != NULL; temp = temp->next) {
        if (temp->value == value) {
            return temp;
        }
    }
    return NULL;
}

static void rv_add_reg_temp(RVFrame *frame, IRValue *value,
                            IRInstruction *def_inst, IRInstruction *use_inst,
                            const char *reg, bool persistent) {
    if (rv_find_reg_temp(frame, value) != NULL) {
        return;
    }
    RVRegTemp *temp = (RVRegTemp *)xmalloc(sizeof(RVRegTemp));
    memset(temp, 0, sizeof(RVRegTemp));
    temp->value = value;
    temp->def_inst = def_inst;
    temp->use_inst = use_inst;
    temp->reg = reg;
    temp->persistent = persistent;
    temp->next = frame->reg_temps;
    frame->reg_temps = temp;
}

static int rv_saved_reg_index(const char *reg) {
    for (int i = 0; i < 6; ++i) {
        if (strcmp(g_rv_loop_regs[i], reg) == 0) {
            return i;
        }
    }
    return -1;
}

static const char *rv_value_reg_home(RVFrame *frame, IRValue *value) {
    RVRegTemp *temp = rv_find_reg_temp(frame, value);
    return temp != NULL ? temp->reg : NULL;
}

static void rv_add_persistent_reg_home(RVFrame *frame, IRValue *value, const char *reg) {
    int index = rv_saved_reg_index(reg);
    if (index >= 0) {
        frame->used_saved_regs[index] = true;
    }
    rv_add_reg_temp(frame, value, NULL, NULL, reg, true);
}

static int rv_value_slot_size(IRValue *value) {
    return rv_value_is_pointer(value) ? 8 : 4;
}

static RVSlot *rv_add_value_slot(RVFrame *frame, IRValue *value) {
    RVSlot *existing = rv_find_slot(frame, value);
    if (existing != NULL) {
        return existing;
    }
    RVSlot *slot = (RVSlot *)xmalloc(sizeof(RVSlot));
    memset(slot, 0, sizeof(RVSlot));
    slot->value = value;
    slot->value_size = rv_value_slot_size(value);
    slot->offset = rv_alloc_frame_bytes(frame, slot->value_size);
    slot->object_offset = 0;
    slot->object_size = 0;
    slot->next = frame->slots;
    frame->slots = slot;
    return slot;
}

static void rv_emit_mem(FILE *out, const char *op, const char *reg, int offset, const char *base) {
    if (offset >= -2048 && offset <= 2047) {
        fprintf(out, "  %s %s, %d(%s)\n", op, reg, offset, base);
        return;
    }
    fprintf(out, "  li t5, %d\n", offset);
    fprintf(out, "  add t5, %s, t5\n", base);
    fprintf(out, "  %s %s, 0(t5)\n", op, reg);
}

static void rv_emit_addi(FILE *out, const char *dst, const char *base, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        fprintf(out, "  addi %s, %s, %d\n", dst, base, offset);
        return;
    }
    fprintf(out, "  li t5, %d\n", offset);
    fprintf(out, "  add %s, %s, t5\n", dst, base);
}

static bool rv_const_i32_value(IRValue *value, int *out_value) {
    if (value == NULL) {
        return false;
    }
    if (value->kind == IR_VALUE_CONST_INT) {
        *out_value = value->data.int_value;
        return true;
    }
    if (value->kind == IR_VALUE_CONST_ZERO) {
        *out_value = 0;
        return true;
    }
    return false;
}

static bool rv_imm12(int value) {
    return value >= -2048 && value <= 2047;
}

static int rv_pow2_shift(int value) {
    if (value <= 0) {
        return -1;
    }
    int shift = 0;
    while ((value & 1) == 0) {
        value >>= 1;
        ++shift;
    }
    return value == 1 && shift < 32 ? shift : -1;
}

typedef struct {
    int32_t magic;
    int shift;
} RVSignedDivMagic;

static bool rv_compute_signed_div_magic(int divisor, RVSignedDivMagic *out) {
    if (out == NULL || divisor == 0 || divisor == 1 || divisor == -1 || divisor == INT_MIN) {
        return false;
    }
    int abs_divisor = divisor < 0 ? -divisor : divisor;
    if (rv_pow2_shift(abs_divisor) >= 0) {
        return false;
    }
    const uint64_t two31 = 1ULL << 31;
    uint32_t ad = (uint32_t)abs_divisor;
    uint64_t t = two31 + (((uint32_t)divisor) >> 31);
    uint64_t anc = t - 1 - (t % ad);
    uint64_t p = 31;
    uint64_t q1 = two31 / anc;
    uint64_t r1 = two31 - q1 * anc;
    uint64_t q2 = two31 / ad;
    uint64_t r2 = two31 - q2 * ad;
    uint64_t delta = 0;
    do {
        ++p;
        q1 <<= 1;
        r1 <<= 1;
        if (r1 >= anc) {
            ++q1;
            r1 -= anc;
        }
        q2 <<= 1;
        r2 <<= 1;
        if (r2 >= ad) {
            ++q2;
            r2 -= ad;
        }
        delta = ad - r2;
    } while (q1 < delta || (q1 == delta && r1 == 0));
    int64_t magic = (int64_t)q2 + 1;
    if (divisor < 0) {
        magic = -magic;
    }
    out->magic = (int32_t)magic;
    out->shift = (int)(p - 32);
    return true;
}

static void rv_emit_signed_div_magic_quotient(FILE *out, const char *dst,
                                              const char *src, int divisor) {
    RVSignedDivMagic info;
    if (!rv_compute_signed_div_magic(divisor, &info)) {
        return;
    }
    fprintf(out, "  li t1, %d\n", info.magic);
    fprintf(out, "  mul t2, %s, t1\n", src);
    fprintf(out, "  srai t2, t2, 32\n");
    if (divisor > 0 && info.magic < 0) {
        fprintf(out, "  addw t2, t2, %s\n", src);
    } else if (divisor < 0 && info.magic > 0) {
        fprintf(out, "  subw t2, t2, %s\n", src);
    }
    if (info.shift > 0) {
        fprintf(out, "  sraiw t2, t2, %d\n", info.shift);
    }
    /* Scratch must avoid t3/t4 (reserved by the RVRegTemp local-register
       cache) and t0 (=src, still needed by the SREM caller). t1 holds the
       magic constant only for the initial mul and is dead here, so reuse it. */
    fprintf(out, "  srliw t1, t2, 31\n");
    fprintf(out, "  addw %s, t2, t1\n", dst);
}

static void rv_emit_and_i32_const(FILE *out, const char *dst, const char *src, int value) {
    if (rv_imm12(value)) {
        fprintf(out, "  andi %s, %s, %d\n", dst, src, value);
        return;
    }
    fprintf(out, "  li t5, %d\n", value);
    fprintf(out, "  and %s, %s, t5\n", dst, src);
}

static int rv_count_instruction_uses(IRInstruction *inst, IRValue *value);

static bool rv_inst_reg_temp_supported(IRInstruction *inst) {
    if (inst == NULL || inst->result_type == NULL ||
            inst->result_type->kind == IR_TYPE_VOID) {
        return false;
    }
    if (rv_value_is_float(&inst->result)) {
        switch (inst->kind) {
            case IR_INST_LOAD:
            case IR_INST_FADD:
            case IR_INST_FSUB:
            case IR_INST_FMUL:
            case IR_INST_FDIV:
            case IR_INST_SITOFP:
                return true;
            default:
                return false;
        }
    }
    switch (inst->kind) {
        case IR_INST_LOAD:
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_ICMP:
        case IR_INST_FCMP:
        case IR_INST_ZEXT:
        case IR_INST_GETELEMENTPTR:
        case IR_INST_BITCAST:
        case IR_INST_FPTOSI:
            return true;
        default:
            return false;
    }
}

static IRInstruction *rv_find_unique_use_in_block(IRFunction *function, IRValue *value,
                                                  IRBasicBlock **out_block) {
    IRInstruction *use_inst = NULL;
    IRBasicBlock *use_block = NULL;
    int count = 0;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            int uses = rv_count_instruction_uses(inst, value);
            if (uses == 0) {
                continue;
            }
            count += uses;
            use_inst = inst;
            use_block = block;
            if (count > 1) {
                return NULL;
            }
        }
    }
    if (count == 1 && out_block != NULL) {
        *out_block = use_block;
    }
    return count == 1 ? use_inst : NULL;
}

static bool rv_has_call_between(IRInstruction *from, IRInstruction *to) {
    if (from == NULL || to == NULL || from->parent != to->parent) {
        return true;
    }
    for (IRInstruction *inst = from->next; inst != NULL && inst != to; inst = inst->next) {
        if (inst->kind == IR_INST_CALL) {
            return true;
        }
    }
    return false;
}

static bool rv_inst_can_be_reg_temp(IRFunction *function, IRBasicBlock *def_block,
                                    IRInstruction *inst, IRInstruction **out_use) {
    if (!rv_inst_reg_temp_supported(inst)) {
        return false;
    }
    IRBasicBlock *use_block = NULL;
    IRInstruction *use_inst = rv_find_unique_use_in_block(function, &inst->result, &use_block);
    if (use_inst == NULL || use_block != def_block || use_inst == inst ||
            rv_has_call_between(inst, use_inst)) {
        return false;
    }
    *out_use = use_inst;
    return true;
}

static void rv_plan_reg_temps_for_block(RVFrame *frame, IRBasicBlock *block) {
    static const char *int_regs[] = {"t3", "t4"};
    static const char *float_regs[] = {"ft3", "ft4"};
    IRValue *active_int[2] = {0};
    IRValue *active_float[2] = {0};
    for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
        for (int i = 0; i < 2; ++i) {
            if (active_int[i] != NULL && rv_count_instruction_uses(inst, active_int[i]) > 0) {
                active_int[i] = NULL;
            }
            if (active_float[i] != NULL && rv_count_instruction_uses(inst, active_float[i]) > 0) {
                active_float[i] = NULL;
            }
        }
        IRInstruction *use_inst = NULL;
        if (!rv_inst_can_be_reg_temp(frame->function, block, inst, &use_inst)) {
            continue;
        }
        IRValue **active = rv_value_is_float(&inst->result) ? active_float : active_int;
        const char **regs = rv_value_is_float(&inst->result) ? float_regs : int_regs;
        for (int i = 0; i < 2; ++i) {
            if (active[i] == NULL) {
                active[i] = &inst->result;
                rv_add_reg_temp(frame, &inst->result, inst, use_inst, regs[i], false);
                break;
            }
        }
    }
}

static bool rv_match_loop_induction_candidate(IRBasicBlock *preheader, IRBasicBlock *latch,
                                              IRInstruction *phi, IRInstruction **out_update) {
    if (phi == NULL || phi->kind != IR_INST_PHI || phi->result_type == NULL
            || phi->result_type->kind != IR_TYPE_I32 || phi->data.phi_inst.count != 2) {
        return false;
    }
    IRValue *start_value = opt_phi_incoming_value(phi, preheader);
    IRValue *latch_value = opt_phi_incoming_value(phi, latch);
    if (start_value == NULL || latch_value == NULL || latch_value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *update = latch_value->data.instruction;
    if (update == NULL || update->parent != latch) {
        return false;
    }
    int delta = 0;
    if (update->kind == IR_INST_ADD) {
        if ((update->data.binary_inst.lhs == &phi->result
                    && opt_const_int_value(update->data.binary_inst.rhs, &delta))
                || (update->data.binary_inst.rhs == &phi->result
                    && opt_const_int_value(update->data.binary_inst.lhs, &delta))) {
            *out_update = update;
            return delta != 0;
        }
    }
    if (update->kind == IR_INST_SUB
            && update->data.binary_inst.lhs == &phi->result
            && opt_const_int_value(update->data.binary_inst.rhs, &delta)
            && delta != 0 && delta != INT_MIN) {
        *out_update = update;
        return true;
    }
    return false;
}

static bool rv_value_can_have_persistent_reg_home(IRValue *value) {
    if (value == NULL || rv_value_is_float(value)) {
        return false;
    }
    if (value->kind == IR_VALUE_PARAM) {
        return true;
    }
    if (value->kind != IR_VALUE_INSTRUCTION) {
        return false;
    }
    IRInstruction *inst = value->data.instruction;
    return inst != NULL && inst->kind != IR_INST_ALLOCA
        && inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID;
}

static int rv_count_loop_uses(IRFunction *function, bool *in_loop, IRValue *value) {
    int count = 0;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        if (!in_loop[bi]) {
            continue;
        }
        for (IRInstruction *inst = function->blocks.items[bi]->first_inst; inst != NULL; inst = inst->next) {
            count += opt_count_uses_in_inst(inst, value);
        }
    }
    return count;
}

static bool rv_block_dominates(IRFunction *function, bool *dom,
                               IRBasicBlock *dominator, IRBasicBlock *block) {
    int a = opt_function_block_index(function, dominator);
    int b = opt_function_block_index(function, block);
    return a >= 0 && b >= 0 && dom[a * function->blocks.count + b];
}

static bool rv_value_is_loop_invariant_candidate(IRFunction *function, bool *dom, bool *in_loop,
                                                 IRBasicBlock *header, IRBasicBlock *preheader,
                                                 IRValue *value) {
    if (!rv_value_can_have_persistent_reg_home(value)) {
        return false;
    }
    if (value->kind == IR_VALUE_PARAM) {
        return true;
    }
    IRInstruction *inst = value->data.instruction;
    if (inst == NULL || inst->parent == NULL) {
        return false;
    }
    if (in_loop[opt_function_block_index(function, inst->parent)]) {
        return false;
    }
    return inst->parent == preheader || rv_block_dominates(function, dom, inst->parent, header);
}

static void rv_candidate_list_push(RVLoopRegCandidate **items, int *count, int *capacity,
                                   IRValue *value, IRInstruction *phi, IRInstruction *update,
                                   IRBasicBlock *latch, int score) {
    for (int i = 0; i < *count; ++i) {
        if ((*items)[i].value == value) {
            if (score > (*items)[i].score) {
                (*items)[i].value = value;
                (*items)[i].score = score;
                (*items)[i].phi = phi;
                (*items)[i].update_inst = update;
                (*items)[i].latch = latch;
            }
            return;
        }
    }
    ensure_capacity((void **)items, capacity, sizeof(RVLoopRegCandidate), *count + 1);
    (*items)[*count].value = value;
    (*items)[*count].phi = phi;
    (*items)[*count].update_inst = update;
    (*items)[*count].latch = latch;
    (*items)[*count].score = score;
    (*count)++;
}

/* True if instruction a is guaranteed to execute before b is reached, i.e.
   a's definition dominates the program point of b. Within one block this is
   simple program order; across blocks it is block dominance. */
static bool rv_inst_dominates_inst(IRFunction *function, bool *dom,
                                   IRInstruction *a, IRInstruction *b) {
    if (a == NULL || b == NULL || a->parent == NULL || b->parent == NULL) {
        return false;
    }
    if (a->parent == b->parent) {
        return opt_inst_before_in_block(a, b);
    }
    int n = function->blocks.count;
    int ai = opt_function_block_index(function, a->parent);
    int bi = opt_function_block_index(function, b->parent);
    if (ai < 0 || bi < 0) {
        return false;
    }
    return dom[bi * n + ai];
}

/* Coalescing a loop phi with the value it takes on the back-edge (its "update")
   into one register is only safe when the phi's old value is dead by the time
   the update overwrites the shared register. Two hazards make it unsafe:
     1. read-after-clobber: some use of the phi is reached after the update
        (e.g. independent recurrences a=a+b; d=d+a reading the old a);
     2. parallel-copy/rotation: the phi value is a back-edge incoming of another
        phi, so the back-edge copy still needs the old value after the update
        already ran (e.g. A=D; D=C; C=B; B=temp).
   Either case means the live ranges interfere, so coalescing must be skipped. */
static bool rv_phi_update_coalesce_safe(IRFunction *function, bool *dom,
                                        IRInstruction *phi, IRInstruction *update) {
    if (phi == NULL || update == NULL) {
        return false;
    }
    IRValue *p = &phi->result;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (inst == update || inst == phi) {
                continue;
            }
            if (opt_count_uses_in_inst(inst, p) == 0) {
                continue;
            }
            /* Hazard 2: used as another phi's incoming value -> parallel copy
               on the back-edge still needs the old value. */
            if (inst->kind == IR_INST_PHI) {
                return false;
            }
            /* Hazard 1: a use reached after the update overwrites the register. */
            if (rv_inst_dominates_inst(function, dom, update, inst)) {
                return false;
            }
        }
    }
    return true;
}

static void rv_plan_loop_reg_homes(RVFrame *frame, IRFunction *function) {
    RVLoopRegCandidate *candidates = NULL;
    int candidate_count = 0;
    int candidate_capacity = 0;
    bool *dom = opt_compute_dominators(function);
    for (int hi = 0; hi < function->blocks.count; ++hi) {
        IRBasicBlock *header = function->blocks.items[hi];
        for (int pi = 0; pi < header->pred_count; ++pi) {
            IRBasicBlock *latch = header->preds[pi];
            int latch_index = opt_function_block_index(function, latch);
            if (latch_index < 0 || !dom[latch_index * function->blocks.count + hi]) {
                continue;
            }
            bool *in_loop = opt_collect_natural_loop(function, header, latch);
            IRBasicBlock *preheader = opt_find_preheader(function, in_loop, header);
            IRBasicBlock *unique_latch = opt_find_unique_loop_latch(function, in_loop, header);
            if (preheader != NULL && unique_latch == latch) {
                int loop_blocks = opt_loop_block_count(function, in_loop);
                for (IRInstruction *phi = header->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
                    if (!rv_value_can_have_persistent_reg_home(&phi->result) || phi->data.phi_inst.count != 2) {
                        continue;
                    }
                    IRValue *preheader_incoming = opt_phi_incoming_value(phi, preheader);
                    IRValue *latch_incoming = opt_phi_incoming_value(phi, latch);
                    if (preheader_incoming == NULL || latch_incoming == NULL) {
                        continue;
                    }
                    /* Skip rotation/shift-register chains formed by loop-header
                       phis feeding each other across the back-edge (e.g. the
                       hash rounds A=D; D=C; C=B; B=temp). Such phis form a
                       cyclic parallel copy on the back-edge; pinning any subset
                       of them to persistent registers makes the mixed
                       register/spill copy collapse the cycle. A phi is part of
                       such a chain if its back-edge value is another header phi
                       (it reads a phi), or its result is the back-edge value of
                       another header phi (it is read by a phi). Induction vars
                       and accumulators (latch value is a self-referencing
                       arithmetic instruction) are unaffected. */
                    bool in_phi_cycle = false;
                    if (latch_incoming->kind == IR_VALUE_INSTRUCTION
                            && latch_incoming->data.instruction != NULL
                            && latch_incoming->data.instruction->kind == IR_INST_PHI
                            && latch_incoming->data.instruction->parent == header) {
                        in_phi_cycle = true;
                    }
                    for (IRInstruction *other = header->first_inst;
                         !in_phi_cycle && other != NULL && other->kind == IR_INST_PHI;
                         other = other->next) {
                        if (other == phi) {
                            continue;
                        }
                        if (opt_phi_incoming_value(other, latch) == &phi->result) {
                            in_phi_cycle = true;
                        }
                    }
                    if (in_phi_cycle) {
                        continue;
                    }
                    int loop_uses = rv_count_loop_uses(function, in_loop, &phi->result);
                    if (loop_uses == 0) {
                        continue;
                    }
                    IRInstruction *update = NULL;
                    bool is_induction = rv_match_loop_induction_candidate(preheader, latch, phi, &update);
                    int score = loop_uses * 24 + loop_blocks * 4;
                    if (is_induction) {
                        score += 32;
                    } else {
                        score += 12;
                    }
                    if (rv_value_can_have_persistent_reg_home(preheader_incoming)
                            && rv_value_is_loop_invariant_candidate(function, dom, in_loop,
                                                                    header, preheader, preheader_incoming)) {
                        score += 8;
                    }
                    if (latch_incoming->kind == IR_VALUE_INSTRUCTION
                            && latch_incoming->data.instruction != NULL
                            && latch_incoming->data.instruction->parent == latch
                            && opt_count_uses(function, latch_incoming) == 1) {
                        score += 12;
                        if (update == NULL) {
                            update = latch_incoming->data.instruction;
                        }
                    }
                    rv_candidate_list_push(&candidates, &candidate_count, &candidate_capacity,
                                           &phi->result, phi, update, latch, score);
                }
                for (int i = 0; i < function->params.count; ++i) {
                    IRValue *param_value = &function->params.items[i]->value;
                    int loop_uses = rv_count_loop_uses(function, in_loop, param_value);
                    if (loop_uses < 2) {
                        continue;
                    }
                    int score = loop_uses * 10 + loop_blocks * 3;
                    rv_candidate_list_push(&candidates, &candidate_count, &candidate_capacity,
                                           param_value, NULL, NULL, NULL, score);
                }
                for (int bi = 0; bi < function->blocks.count; ++bi) {
                    if (in_loop[bi]) {
                        continue;
                    }
                    IRBasicBlock *block = function->blocks.items[bi];
                    if (!rv_block_dominates(function, dom, block, header)) {
                        continue;
                    }
                    for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
                        if (!rv_value_is_loop_invariant_candidate(function, dom, in_loop,
                                                                  header, preheader, &inst->result)) {
                            continue;
                        }
                        int loop_uses = rv_count_loop_uses(function, in_loop, &inst->result);
                        if (loop_uses < 2) {
                            continue;
                        }
                        int score = loop_uses * 12 + loop_blocks * 3;
                        if (block == preheader) {
                            score += 12;
                        }
                        if (inst->kind == IR_INST_GETELEMENTPTR || inst->kind == IR_INST_BITCAST) {
                            score += 10;
                        } else if (inst->kind == IR_INST_LOAD) {
                            score += 6;
                        }
                        rv_candidate_list_push(&candidates, &candidate_count, &candidate_capacity,
                                               &inst->result, NULL, NULL, NULL, score);
                    }
                }
            }
            free(in_loop);
        }
    }
    for (int i = 0; i < candidate_count; ++i) {
        for (int j = i + 1; j < candidate_count; ++j) {
            if (candidates[j].score > candidates[i].score) {
                RVLoopRegCandidate tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
    int assign_count = candidate_count < 6 ? candidate_count : 6;
    for (int i = 0; i < assign_count; ++i) {
        const char *reg = g_rv_loop_regs[i];
        rv_add_persistent_reg_home(frame, candidates[i].value, reg);
        if (candidates[i].phi != NULL && candidates[i].update_inst != NULL
                && opt_phi_incoming_value(candidates[i].phi, candidates[i].latch)
                       == &candidates[i].update_inst->result
                && opt_count_uses(function, &candidates[i].update_inst->result) == 1
                && rv_phi_update_coalesce_safe(function, dom,
                                               candidates[i].phi, candidates[i].update_inst)) {
            rv_add_persistent_reg_home(frame, &candidates[i].update_inst->result, reg);
        }
    }
    free(dom);
    free(candidates);
}

static void rv_plan_reg_temps(RVFrame *frame, IRFunction *function) {
    rv_plan_loop_reg_homes(frame, function);
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        rv_plan_reg_temps_for_block(frame, function->blocks.items[bi]);
    }
}

static bool rv_inst_needs_result_slot(RVFrame *frame, IRInstruction *inst) {
    (void)frame;
    if (inst->result_type == NULL || inst->result_type->kind == IR_TYPE_VOID) {
        return false;
    }
    if (rv_find_reg_temp(frame, &inst->result) != NULL) {
        return false;
    }
    return true;
}

static bool rv_inst_has_result_home(RVFrame *frame, IRInstruction *inst) {
    return rv_find_reg_temp(frame, &inst->result) != NULL ||
           rv_find_slot(frame, &inst->result) != NULL;
}

static void rv_store_int_slot(RVFrame *frame, IRValue *value, const char *reg) {
    RVRegTemp *temp = rv_find_reg_temp(frame, value);
    if (temp != NULL) {
        if (strcmp(temp->reg, reg) != 0) {
            fprintf(frame->out, "  mv %s, %s\n", temp->reg, reg);
        }
        return;
    }
    RVSlot *slot = rv_find_slot(frame, value);
    if (slot != NULL) {
        rv_emit_mem(frame->out, slot->value_size == 8 ? "sd" : "sw", reg, slot->offset, "s0");
    }
}

static void rv_store_float_slot(RVFrame *frame, IRValue *value, const char *reg) {
    RVRegTemp *temp = rv_find_reg_temp(frame, value);
    if (temp != NULL) {
        if (strcmp(temp->reg, reg) != 0) {
            fprintf(frame->out, "  fmv.s %s, %s\n", temp->reg, reg);
        }
        return;
    }
    RVSlot *slot = rv_find_slot(frame, value);
    if (slot != NULL) {
        rv_emit_mem(frame->out, "fsw", reg, slot->offset, "s0");
    }
}

static void rv_load_int_value(RVFrame *frame, IRValue *value, const char *reg) {
    FILE *out = frame->out;
    if (value == NULL) {
        fprintf(out, "  li %s, 0\n", reg);
        return;
    }
    switch (value->kind) {
        case IR_VALUE_CONST_INT:
            fprintf(out, "  li %s, %d\n", reg, value->data.int_value);
            return;
        case IR_VALUE_CONST_FLOAT:
            fprintf(out, "  li %s, %d\n", reg, value->data.float_bits);
            return;
        case IR_VALUE_CONST_ZERO:
            fprintf(out, "  li %s, 0\n", reg);
            return;
        case IR_VALUE_GLOBAL:
            fprintf(out, "  lla %s, %s\n", reg, rv_symbol_name(value->name));
            return;
        case IR_VALUE_PARAM:
        case IR_VALUE_INSTRUCTION: {
            RVRegTemp *temp = rv_find_reg_temp(frame, value);
            if (temp != NULL) {
                if (strcmp(reg, temp->reg) != 0) {
                    fprintf(out, "  mv %s, %s\n", reg, temp->reg);
                }
                return;
            }
            if (value->kind == IR_VALUE_INSTRUCTION) {
                IRInstruction *inst = value->data.instruction;
                if (inst != NULL && inst->kind == IR_INST_ALLOCA) {
                    RVSlot *slot = rv_find_slot(frame, value);
                    if (slot != NULL) {
                        rv_emit_addi(out, reg, "s0", slot->object_offset);
                        return;
                    }
                }
            }
            RVSlot *slot = rv_find_slot(frame, value);
            if (slot != NULL) {
                rv_emit_mem(out, slot->value_size == 8 ? "ld" : "lw", reg, slot->offset, "s0");
                return;
            }
            break;
        }
        default:
            break;
    }
    fprintf(out, "  li %s, 0\n", reg);
}

static void rv_load_float_value(RVFrame *frame, IRValue *value, const char *freg) {
    FILE *out = frame->out;
    if (value == NULL) {
        fprintf(out, "  fmv.w.x %s, zero\n", freg);
        return;
    }
    if (value->kind == IR_VALUE_CONST_FLOAT) {
        fprintf(out, "  li t6, %d\n", value->data.float_bits);
        fprintf(out, "  fmv.w.x %s, t6\n", freg);
        return;
    }
    if (value->kind == IR_VALUE_CONST_ZERO) {
        fprintf(out, "  fmv.w.x %s, zero\n", freg);
        return;
    }
    if (value->kind == IR_VALUE_CONST_INT) {
        fprintf(out, "  li t6, %d\n", value->data.int_value);
        fprintf(out, "  fcvt.s.w %s, t6\n", freg);
        return;
    }
    RVRegTemp *temp = rv_find_reg_temp(frame, value);
    if (temp != NULL) {
        if (strcmp(freg, temp->reg) != 0) {
            fprintf(out, "  fmv.s %s, %s\n", freg, temp->reg);
        }
        return;
    }
    RVSlot *slot = rv_find_slot(frame, value);
    if (slot != NULL) {
        rv_emit_mem(out, "flw", freg, slot->offset, "s0");
    } else {
        fprintf(out, "  fmv.w.x %s, zero\n", freg);
    }
}

static bool rv_function_param_uses_float_reg(IRFunction *callee, int index) {
    if (callee == NULL) {
        return false;
    }
    if (index < callee->params.count) {
        IRParameter *param = callee->params.items[index];
        return param->type != NULL && param->type->kind == IR_TYPE_FLOAT;
    }
    return mem_runtime_param_is_float_scalar(callee->name, index);
}

static bool rv_function_param_uses_int_reg(IRFunction *callee, int index) {
    if (callee == NULL) {
        return true;
    }
    if (index < callee->params.count) {
        IRParameter *param = callee->params.items[index];
        return param->type == NULL || param->type->kind != IR_TYPE_FLOAT;
    }
    if (mem_runtime_param_is_pointer(callee->name, index)) {
        return true;
    }
    return !mem_runtime_param_is_float_scalar(callee->name, index);
}

static int rv_call_stack_slots(IRInstruction *inst) {
    int int_regs = 0;
    int float_regs = 0;
    int stack_slots = 0;
    for (int i = 0; i < inst->data.call_inst.args.count; ++i) {
        if (rv_function_param_uses_float_reg(inst->data.call_inst.callee, i)) {
            if (float_regs < 8) {
                float_regs++;
            } else {
                stack_slots++;
            }
        } else if (rv_function_param_uses_int_reg(inst->data.call_inst.callee, i)) {
            if (int_regs < 8) {
                int_regs++;
            } else {
                stack_slots++;
            }
        }
    }
    return stack_slots;
}

static bool rv_phi_needs_scratch(RVFrame *frame, IRInstruction *phi) {
    return phi == NULL || phi->kind != IR_INST_PHI
        || rv_value_is_float(&phi->result)
        || rv_value_reg_home(frame, &phi->result) == NULL;
}

static int rv_block_phi_count(RVFrame *frame, IRBasicBlock *block) {
    int count = 0;
    for (IRInstruction *inst = block != NULL ? block->first_inst : NULL;
         inst != NULL && inst->kind == IR_INST_PHI; inst = inst->next) {
        if (rv_phi_needs_scratch(frame, inst)) {
            ++count;
        }
    }
    return count;
}

static void rv_prepare_frame(RVFrame *frame, IRFunction *function) {
    memset(frame, 0, sizeof(RVFrame));
    frame->function = function;
    frame->next_offset = -16;
    rv_plan_reg_temps(frame, function);
    for (int i = 0; i < function->params.count; ++i) {
        rv_add_value_slot(frame, &function->params.items[i]->value);
    }
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        int phi_count = rv_block_phi_count(frame, block);
        if (phi_count > frame->phi_scratch_slots) {
            frame->phi_scratch_slots = phi_count;
        }
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (rv_inst_needs_result_slot(frame, inst)) {
                RVSlot *slot = rv_add_value_slot(frame, &inst->result);
                if (inst->kind == IR_INST_ALLOCA) {
                    slot->object_size = align_to(rv_type_size(inst->data.alloca_inst.allocated_type), 8);
                    slot->object_offset = rv_alloc_frame_bytes(frame, slot->object_size);
                }
            }
            if (inst->kind == IR_INST_CALL) {
                int outgoing = rv_call_stack_slots(inst) * 8;
                if (outgoing > frame->max_outgoing_args) {
                    frame->max_outgoing_args = outgoing;
                }
            }
        }
    }
    if (frame->phi_scratch_slots > 0) {
        frame->phi_scratch_offset = rv_alloc_frame_bytes(frame, frame->phi_scratch_slots * 8);
    }
    for (int i = 0; i < 6; ++i) {
        if (frame->used_saved_regs[i]) {
            frame->saved_reg_offsets[i] = rv_alloc_frame_bytes(frame, 8);
        }
    }
    frame->frame_size = align_to(-frame->next_offset + frame->max_outgoing_args, 16);
}

static void rv_emit_global_init(FILE *out, IRInitializer *init, IRType *type) {
    if (init == NULL || init->kind == IR_INIT_ZERO) {
        fprintf(out, "  .zero %d\n", rv_type_size(type));
        return;
    }
    switch (init->kind) {
        case IR_INIT_INT:
            fprintf(out, "  .word %d\n", init->data.int_value);
            return;
        case IR_INIT_FLOAT:
            fprintf(out, "  .word %d\n", init->data.float_bits);
            return;
        case IR_INIT_ARRAY:
            for (int i = 0; i < init->data.array.count; ++i) {
                rv_emit_global_init(out, init->data.array.items[i],
                                    init->data.array.items[i]->type);
            }
            return;
        case IR_INIT_STRING:
            fprintf(out, "  .zero %d\n", init->data.string.length);
            return;
        case IR_INIT_ZERO:
            break;
    }
    fprintf(out, "  .zero %d\n", rv_type_size(type));
}

static bool rv_initializer_is_zero(IRInitializer *init) {
    if (init == NULL || init->kind == IR_INIT_ZERO) {
        return true;
    }
    switch (init->kind) {
        case IR_INIT_INT:
            return init->data.int_value == 0;
        case IR_INIT_FLOAT:
            return init->data.float_bits == float_bits_from_host(0.0f);
        case IR_INIT_ARRAY:
            for (int i = 0; i < init->data.array.count; ++i) {
                if (!rv_initializer_is_zero(init->data.array.items[i])) {
                    return false;
                }
            }
            return true;
        case IR_INIT_STRING:
            return init->data.string.length == 0;
        case IR_INIT_ZERO:
            return true;
    }
    return false;
}

static void rv_emit_globals(IRModule *module, FILE *out) {
    int current_section = 0;
    for (int i = 0; i < module->globals.count; ++i) {
        IRGlobal *global = module->globals.items[i];
        if (global->is_external) {
            continue;
        }
        bool is_zero = rv_initializer_is_zero(global->initializer);
        if (is_zero && current_section != 1) {
            fputs("  .bss\n", out);
            current_section = 1;
        } else if (!is_zero && current_section != 2) {
            fputs("  .data\n", out);
            current_section = 2;
        }
        fprintf(out, "  .globl %s\n  .align 3\n%s:\n",
                rv_symbol_name(global->name), rv_symbol_name(global->name));
        rv_emit_global_init(out, global->initializer, global->type);
    }
}

static void rv_emit_load_inst(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    rv_load_int_value(frame, inst->data.load_inst.ptr, "t0");
    if (rv_value_is_float(&inst->result)) {
        rv_emit_mem(out, "flw", "ft0", 0, "t0");
        rv_store_float_slot(frame, &inst->result, "ft0");
    } else if (rv_value_is_pointer(&inst->result)) {
        rv_emit_mem(out, "ld", "t1", 0, "t0");
        rv_store_int_slot(frame, &inst->result, "t1");
    } else {
        rv_emit_mem(out, "lw", "t1", 0, "t0");
        rv_store_int_slot(frame, &inst->result, "t1");
    }
}

static void rv_emit_store_inst(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    IRValue *value = inst->data.store_inst.value;
    rv_load_int_value(frame, inst->data.store_inst.ptr, "t0");
    if (rv_value_is_float(value)) {
        rv_load_float_value(frame, value, "ft0");
        rv_emit_mem(out, "fsw", "ft0", 0, "t0");
    } else if (rv_value_is_pointer(value)) {
        rv_load_int_value(frame, value, "t1");
        rv_emit_mem(out, "sd", "t1", 0, "t0");
    } else {
        rv_load_int_value(frame, value, "t1");
        rv_emit_mem(out, "sw", "t1", 0, "t0");
    }
}

static void rv_emit_int_binary_to_reg(RVFrame *frame, IRInstruction *inst, const char *op, const char *dst) {
    FILE *out = frame->out;
    int rhs_const = 0;
    int lhs_const = 0;
    if (inst->kind == IR_INST_ADD && rv_const_i32_value(inst->data.binary_inst.rhs, &rhs_const) && rv_imm12(rhs_const)) {
        rv_load_int_value(frame, inst->data.binary_inst.lhs, dst);
        fprintf(out, "  addiw %s, %s, %d\n", dst, dst, rhs_const);
        return;
    }
    if (inst->kind == IR_INST_ADD && rv_const_i32_value(inst->data.binary_inst.lhs, &lhs_const) && rv_imm12(lhs_const)) {
        rv_load_int_value(frame, inst->data.binary_inst.rhs, dst);
        fprintf(out, "  addiw %s, %s, %d\n", dst, dst, lhs_const);
        return;
    }
    if (inst->kind == IR_INST_SUB && rv_const_i32_value(inst->data.binary_inst.rhs, &rhs_const)
            && rhs_const != INT_MIN && rv_imm12(-rhs_const)) {
        rv_load_int_value(frame, inst->data.binary_inst.lhs, dst);
        fprintf(out, "  addiw %s, %s, %d\n", dst, dst, -rhs_const);
        return;
    }
    if (inst->kind == IR_INST_MUL && rv_const_i32_value(inst->data.binary_inst.rhs, &rhs_const)) {
        int shift = rv_pow2_shift(rhs_const);
        if (shift >= 0) {
            rv_load_int_value(frame, inst->data.binary_inst.lhs, dst);
            fprintf(out, "  slliw %s, %s, %d\n", dst, dst, shift);
            return;
        }
    }
    if (inst->kind == IR_INST_MUL && rv_const_i32_value(inst->data.binary_inst.lhs, &lhs_const)) {
        int shift = rv_pow2_shift(lhs_const);
        if (shift >= 0) {
            rv_load_int_value(frame, inst->data.binary_inst.rhs, dst);
            fprintf(out, "  slliw %s, %s, %d\n", dst, dst, shift);
            return;
        }
    }
    if (inst->kind == IR_INST_SDIV && rv_const_i32_value(inst->data.binary_inst.rhs, &rhs_const)) {
        int abs_rhs_const = rhs_const == INT_MIN ? 0 : (rhs_const < 0 ? -rhs_const : rhs_const);
        int shift = rv_pow2_shift(abs_rhs_const);
        if (shift >= 0) {
            rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
            if (shift == 0) {
                if (rhs_const > 0) {
                    fprintf(out, "  mv %s, t0\n", dst);
                } else {
                    fprintf(out, "  negw %s, t0\n", dst);
                }
                return;
            }
            fprintf(out, "  sraiw t1, t0, 31\n");
            rv_emit_and_i32_const(out, "t1", "t1", abs_rhs_const - 1);
            fprintf(out, "  addw t0, t0, t1\n");
            fprintf(out, "  sraiw t0, t0, %d\n", shift);
            if (rhs_const > 0) {
                fprintf(out, "  mv %s, t0\n", dst);
            } else {
                fprintf(out, "  negw %s, t0\n", dst);
            }
            return;
        }
        if (rv_compute_signed_div_magic(rhs_const, &(RVSignedDivMagic){0})) {
            rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
            rv_emit_signed_div_magic_quotient(out, dst, "t0", rhs_const);
            return;
        }
    }
    if (inst->kind == IR_INST_SREM && rv_const_i32_value(inst->data.binary_inst.rhs, &rhs_const)) {
        int abs_rhs_const = rhs_const == INT_MIN ? 0 : (rhs_const < 0 ? -rhs_const : rhs_const);
        int shift = rv_pow2_shift(abs_rhs_const);
        if (shift >= 0) {
            rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
            if (shift == 0) {
                fprintf(out, "  li %s, 0\n", dst);
                return;
            }
            fprintf(out, "  sraiw t1, t0, 31\n");
            rv_emit_and_i32_const(out, "t1", "t1", abs_rhs_const - 1);
            fprintf(out, "  addw %s, t0, t1\n", dst);
            rv_emit_and_i32_const(out, dst, dst, abs_rhs_const - 1);
            fprintf(out, "  subw %s, %s, t1\n", dst, dst);
            return;
        }
        if (rv_compute_signed_div_magic(rhs_const, &(RVSignedDivMagic){0})) {
            rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
            rv_emit_signed_div_magic_quotient(out, "t2", "t0", rhs_const);
            fprintf(out, "  li t1, %d\n", rhs_const);
            fprintf(out, "  mul t1, t2, t1\n");
            fprintf(out, "  subw %s, t0, t1\n", dst);
            return;
        }
    }
    rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
    rv_load_int_value(frame, inst->data.binary_inst.rhs, "t1");
    fprintf(out, "  %s %s, t0, t1\n", op, dst);
}

static void rv_emit_int_binary(RVFrame *frame, IRInstruction *inst, const char *op) {
    if (!rv_inst_has_result_home(frame, inst)) {
        return;
    }
    rv_emit_int_binary_to_reg(frame, inst, op, "t2");
    rv_store_int_slot(frame, &inst->result, "t2");
}

static void rv_emit_float_binary(RVFrame *frame, IRInstruction *inst, const char *op) {
    FILE *out = frame->out;
    rv_load_float_value(frame, inst->data.binary_inst.lhs, "ft0");
    rv_load_float_value(frame, inst->data.binary_inst.rhs, "ft1");
    fprintf(out, "  %s ft2, ft0, ft1\n", op);
    rv_store_float_slot(frame, &inst->result, "ft2");
}

static void rv_emit_icmp_to_reg(RVFrame *frame, IRInstruction *inst, const char *dst) {
    FILE *out = frame->out;
    int rhs_const = 0;
    if (rv_const_i32_value(inst->data.icmp_inst.rhs, &rhs_const)) {
        if (rhs_const == 0) {
            rv_load_int_value(frame, inst->data.icmp_inst.lhs, "t0");
            switch (inst->data.icmp_inst.pred) {
                case IR_ICMP_EQ:
                    fprintf(out, "  seqz %s, t0\n", dst);
                    return;
                case IR_ICMP_NE:
                    fprintf(out, "  snez %s, t0\n", dst);
                    return;
                default:
                    break;
            }
        }
        if (rv_imm12(rhs_const)) {
            rv_load_int_value(frame, inst->data.icmp_inst.lhs, "t0");
            switch (inst->data.icmp_inst.pred) {
                case IR_ICMP_SLT:
                    fprintf(out, "  slti %s, t0, %d\n", dst, rhs_const);
                    return;
                case IR_ICMP_SGE:
                    fprintf(out, "  slti %s, t0, %d\n  xori %s, %s, 1\n",
                            dst, rhs_const, dst, dst);
                    return;
                default:
                    break;
            }
        }
        if (rhs_const < INT_MAX && rv_imm12(rhs_const + 1)) {
            rv_load_int_value(frame, inst->data.icmp_inst.lhs, "t0");
            switch (inst->data.icmp_inst.pred) {
                case IR_ICMP_SLE:
                    fprintf(out, "  slti %s, t0, %d\n", dst, rhs_const + 1);
                    return;
                case IR_ICMP_SGT:
                    fprintf(out, "  slti %s, t0, %d\n  xori %s, %s, 1\n",
                            dst, rhs_const + 1, dst, dst);
                    return;
                default:
                    break;
            }
        }
    }
    rv_load_int_value(frame, inst->data.icmp_inst.lhs, "t0");
    rv_load_int_value(frame, inst->data.icmp_inst.rhs, "t1");
    switch (inst->data.icmp_inst.pred) {
        case IR_ICMP_EQ:
            fprintf(out, "  subw %s, t0, t1\n  seqz %s, %s\n", dst, dst, dst);
            break;
        case IR_ICMP_NE:
            fprintf(out, "  subw %s, t0, t1\n  snez %s, %s\n", dst, dst, dst);
            break;
        case IR_ICMP_SLT:
            fprintf(out, "  slt %s, t0, t1\n", dst);
            break;
        case IR_ICMP_SLE:
            fprintf(out, "  slt %s, t1, t0\n  xori %s, %s, 1\n", dst, dst, dst);
            break;
        case IR_ICMP_SGT:
            fprintf(out, "  slt %s, t1, t0\n", dst);
            break;
        case IR_ICMP_SGE:
            fprintf(out, "  slt %s, t0, t1\n  xori %s, %s, 1\n", dst, dst, dst);
            break;
    }
}

static void rv_emit_icmp(RVFrame *frame, IRInstruction *inst) {
    if (!rv_inst_has_result_home(frame, inst)) {
        return;
    }
    rv_emit_icmp_to_reg(frame, inst, "t2");
    rv_store_int_slot(frame, &inst->result, "t2");
}

static void rv_emit_fcmp(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    rv_load_float_value(frame, inst->data.fcmp_inst.lhs, "ft0");
    rv_load_float_value(frame, inst->data.fcmp_inst.rhs, "ft1");
    switch (inst->data.fcmp_inst.pred) {
        case IR_FCMP_OEQ:
            fputs("  feq.s t2, ft0, ft1\n", out);
            break;
        case IR_FCMP_ONE:
            fputs("  feq.s t2, ft0, ft1\n  xori t2, t2, 1\n", out);
            break;
        case IR_FCMP_OLT:
            fputs("  flt.s t2, ft0, ft1\n", out);
            break;
        case IR_FCMP_OLE:
            fputs("  fle.s t2, ft0, ft1\n", out);
            break;
        case IR_FCMP_OGT:
            fputs("  flt.s t2, ft1, ft0\n", out);
            break;
        case IR_FCMP_OGE:
            fputs("  fle.s t2, ft1, ft0\n", out);
            break;
    }
    rv_store_int_slot(frame, &inst->result, "t2");
}

static int rv_count_value_ref(IRValue *candidate, IRValue *value) {
    return candidate == value ? 1 : 0;
}

static int rv_count_value_list_uses(IRValueList *list, IRValue *value) {
    int count = 0;
    for (int i = 0; i < list->count; ++i) {
        count += rv_count_value_ref(list->items[i], value);
    }
    return count;
}

static int rv_count_instruction_uses(IRInstruction *inst, IRValue *value) {
    switch (inst->kind) {
        case IR_INST_ALLOCA:
            return 0;
        case IR_INST_PHI:
            return rv_count_value_list_uses(&inst->data.phi_inst.values, value);
        case IR_INST_LOAD:
            return rv_count_value_ref(inst->data.load_inst.ptr, value);
        case IR_INST_STORE:
            return rv_count_value_ref(inst->data.store_inst.value, value)
                 + rv_count_value_ref(inst->data.store_inst.ptr, value);
        case IR_INST_ADD:
        case IR_INST_SUB:
        case IR_INST_MUL:
        case IR_INST_SDIV:
        case IR_INST_SREM:
        case IR_INST_FADD:
        case IR_INST_FSUB:
        case IR_INST_FMUL:
        case IR_INST_FDIV:
            return rv_count_value_ref(inst->data.binary_inst.lhs, value)
                 + rv_count_value_ref(inst->data.binary_inst.rhs, value);
        case IR_INST_ICMP:
            return rv_count_value_ref(inst->data.icmp_inst.lhs, value)
                 + rv_count_value_ref(inst->data.icmp_inst.rhs, value);
        case IR_INST_FCMP:
            return rv_count_value_ref(inst->data.fcmp_inst.lhs, value)
                 + rv_count_value_ref(inst->data.fcmp_inst.rhs, value);
        case IR_INST_ZEXT:
        case IR_INST_SITOFP:
        case IR_INST_FPTOSI:
            return rv_count_value_ref(inst->data.cast_inst.value, value);
        case IR_INST_BR:
            return inst->data.br_inst.is_conditional
                ? rv_count_value_ref(inst->data.br_inst.condition, value) : 0;
        case IR_INST_RET:
            return rv_count_value_ref(inst->data.ret_inst.value, value);
        case IR_INST_CALL:
            return rv_count_value_list_uses(&inst->data.call_inst.args, value);
        case IR_INST_GETELEMENTPTR:
            return rv_count_value_ref(inst->data.gep_inst.base_ptr, value)
                 + rv_count_value_list_uses(&inst->data.gep_inst.indices, value);
        case IR_INST_BITCAST:
            return rv_count_value_ref(inst->data.bitcast_inst.value, value);
    }
    return 0;
}

static int rv_count_function_uses(IRFunction *function, IRValue *value) {
    int count = 0;
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            count += rv_count_instruction_uses(inst, value);
        }
    }
    return count;
}

static bool rv_block_has_phi(IRBasicBlock *block) {
    return block != NULL && block->first_inst != NULL && block->first_inst->kind == IR_INST_PHI;
}

static void rv_emit_phi_reg_moves(RVFrame *frame, IRBasicBlock *from, IRBasicBlock *to) {
    const char *dst_regs[16] = {0};
    const char *src_regs[16] = {0};
    IRValue *incoming_values[16] = {0};
    bool done[16] = {0};
    int count = 0;
    for (IRInstruction *phi = to->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        if (rv_phi_needs_scratch(frame, phi)) {
            continue;
        }
        IRValue *incoming = opt_phi_incoming_value(phi, from);
        const char *dst_reg = rv_value_reg_home(frame, &phi->result);
        if (incoming == NULL || dst_reg == NULL || count >= 16) {
            continue;
        }
        dst_regs[count] = dst_reg;
        src_regs[count] = rv_value_reg_home(frame, incoming);
        incoming_values[count] = incoming;
        ++count;
    }
    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < count; ++i) {
            if (done[i] || src_regs[i] == NULL) {
                continue;
            }
            if (strcmp(dst_regs[i], src_regs[i]) == 0) {
                done[i] = true;
                progress = true;
                continue;
            }
            bool dst_used_as_src = false;
            for (int j = 0; j < count; ++j) {
                if (i == j || done[j] || src_regs[j] == NULL) {
                    continue;
                }
                if (strcmp(src_regs[j], dst_regs[i]) == 0) {
                    dst_used_as_src = true;
                    break;
                }
            }
            if (!dst_used_as_src) {
                fprintf(frame->out, "  mv %s, %s\n", dst_regs[i], src_regs[i]);
                done[i] = true;
                progress = true;
            }
        }
        if (!progress) {
            for (int i = 0; i < count; ++i) {
                if (done[i] || src_regs[i] == NULL || strcmp(dst_regs[i], src_regs[i]) == 0) {
                    continue;
                }
                fprintf(frame->out, "  mv t6, %s\n", dst_regs[i]);
                for (int j = 0; j < count; ++j) {
                    if (!done[j] && src_regs[j] != NULL && strcmp(src_regs[j], dst_regs[i]) == 0) {
                        src_regs[j] = "t6";
                    }
                }
                progress = true;
                break;
            }
        }
    }
    for (int i = 0; i < count; ++i) {
        if (!done[i]) {
            rv_load_int_value(frame, incoming_values[i], dst_regs[i]);
            done[i] = true;
        }
    }
}

static void rv_emit_phi_moves(RVFrame *frame, IRBasicBlock *from, IRBasicBlock *to) {
    rv_emit_phi_reg_moves(frame, from, to);
    int scratch_index = 0;
    for (IRInstruction *phi = to->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        if (!rv_phi_needs_scratch(frame, phi)) {
            continue;
        }
        IRValue *incoming = opt_phi_incoming_value(phi, from);
        int scratch_offset = frame->phi_scratch_offset + scratch_index * 8;
        if (rv_value_is_float(&phi->result)) {
            rv_load_float_value(frame, incoming, "ft0");
            rv_emit_mem(frame->out, "fsw", "ft0", scratch_offset, "s0");
        } else {
            rv_load_int_value(frame, incoming, "t6");
            rv_emit_mem(frame->out, rv_value_is_pointer(&phi->result) ? "sd" : "sw", "t6", scratch_offset, "s0");
        }
        ++scratch_index;
    }
    scratch_index = 0;
    for (IRInstruction *phi = to->first_inst; phi != NULL && phi->kind == IR_INST_PHI; phi = phi->next) {
        if (!rv_phi_needs_scratch(frame, phi)) {
            continue;
        }
        int scratch_offset = frame->phi_scratch_offset + scratch_index * 8;
        if (rv_value_is_float(&phi->result)) {
            rv_emit_mem(frame->out, "flw", "ft0", scratch_offset, "s0");
            rv_store_float_slot(frame, &phi->result, "ft0");
        } else {
            rv_emit_mem(frame->out, rv_value_is_pointer(&phi->result) ? "ld" : "lw", "t6", scratch_offset, "s0");
            rv_store_int_slot(frame, &phi->result, "t6");
        }
        ++scratch_index;
    }
}

static void rv_emit_icmp_branch(RVFrame *frame, IRInstruction *cmp, IRInstruction *br) {
    FILE *out = frame->out;
    char *true_label = rv_block_label(frame->function, br->data.br_inst.true_block);
    char *false_label = rv_block_label(frame->function, br->data.br_inst.false_block);
    int const_value = 0;
    if (rv_const_i32_value(cmp->data.icmp_inst.rhs, &const_value) && const_value == 0) {
        rv_load_int_value(frame, cmp->data.icmp_inst.lhs, "t0");
        switch (cmp->data.icmp_inst.pred) {
            case IR_ICMP_EQ:
                fprintf(out, "  beqz t0, %s\n", true_label);
                break;
            case IR_ICMP_NE:
                fprintf(out, "  bnez t0, %s\n", true_label);
                break;
            case IR_ICMP_SLT:
                fprintf(out, "  blt t0, zero, %s\n", true_label);
                break;
            case IR_ICMP_SLE:
                fprintf(out, "  bge zero, t0, %s\n", true_label);
                break;
            case IR_ICMP_SGT:
                fprintf(out, "  blt zero, t0, %s\n", true_label);
                break;
            case IR_ICMP_SGE:
                fprintf(out, "  bge t0, zero, %s\n", true_label);
                break;
        }
        fprintf(out, "  j %s\n", false_label);
        return;
    }
    if (rv_const_i32_value(cmp->data.icmp_inst.lhs, &const_value) && const_value == 0) {
        rv_load_int_value(frame, cmp->data.icmp_inst.rhs, "t0");
        switch (cmp->data.icmp_inst.pred) {
            case IR_ICMP_EQ:
                fprintf(out, "  beqz t0, %s\n", true_label);
                break;
            case IR_ICMP_NE:
                fprintf(out, "  bnez t0, %s\n", true_label);
                break;
            case IR_ICMP_SLT:
                fprintf(out, "  blt zero, t0, %s\n", true_label);
                break;
            case IR_ICMP_SLE:
                fprintf(out, "  bge t0, zero, %s\n", true_label);
                break;
            case IR_ICMP_SGT:
                fprintf(out, "  blt t0, zero, %s\n", true_label);
                break;
            case IR_ICMP_SGE:
                fprintf(out, "  bge zero, t0, %s\n", true_label);
                break;
        }
        fprintf(out, "  j %s\n", false_label);
        return;
    }
    rv_load_int_value(frame, cmp->data.icmp_inst.lhs, "t0");
    rv_load_int_value(frame, cmp->data.icmp_inst.rhs, "t1");
    switch (cmp->data.icmp_inst.pred) {
        case IR_ICMP_EQ:
            fprintf(out, "  beq t0, t1, %s\n", true_label);
            break;
        case IR_ICMP_NE:
            fprintf(out, "  bne t0, t1, %s\n", true_label);
            break;
        case IR_ICMP_SLT:
            fprintf(out, "  blt t0, t1, %s\n", true_label);
            break;
        case IR_ICMP_SLE:
            fprintf(out, "  bge t1, t0, %s\n", true_label);
            break;
        case IR_ICMP_SGT:
            fprintf(out, "  blt t1, t0, %s\n", true_label);
            break;
        case IR_ICMP_SGE:
            fprintf(out, "  bge t0, t1, %s\n", true_label);
            break;
    }
    fprintf(out, "  j %s\n", false_label);
}

static void rv_emit_fcmp_branch(RVFrame *frame, IRInstruction *cmp, IRInstruction *br) {
    FILE *out = frame->out;
    char *true_label = rv_block_label(frame->function, br->data.br_inst.true_block);
    char *false_label = rv_block_label(frame->function, br->data.br_inst.false_block);
    rv_load_float_value(frame, cmp->data.fcmp_inst.lhs, "ft0");
    rv_load_float_value(frame, cmp->data.fcmp_inst.rhs, "ft1");
    switch (cmp->data.fcmp_inst.pred) {
        case IR_FCMP_OEQ:
            fputs("  feq.s t0, ft0, ft1\n", out);
            fprintf(out, "  bnez t0, %s\n", true_label);
            break;
        case IR_FCMP_ONE:
            fputs("  feq.s t0, ft0, ft1\n", out);
            fprintf(out, "  beqz t0, %s\n", true_label);
            break;
        case IR_FCMP_OLT:
            fputs("  flt.s t0, ft0, ft1\n", out);
            fprintf(out, "  bnez t0, %s\n", true_label);
            break;
        case IR_FCMP_OLE:
            fputs("  fle.s t0, ft0, ft1\n", out);
            fprintf(out, "  bnez t0, %s\n", true_label);
            break;
        case IR_FCMP_OGT:
            fputs("  flt.s t0, ft1, ft0\n", out);
            fprintf(out, "  bnez t0, %s\n", true_label);
            break;
        case IR_FCMP_OGE:
            fputs("  fle.s t0, ft1, ft0\n", out);
            fprintf(out, "  bnez t0, %s\n", true_label);
            break;
    }
    fprintf(out, "  j %s\n", false_label);
}

static bool rv_can_fuse_compare_branch(IRFunction *function, IRInstruction *cmp) {
    IRInstruction *br = cmp->next;
    if (br == NULL || br->kind != IR_INST_BR || !br->data.br_inst.is_conditional) {
        return false;
    }
    if (br->data.br_inst.condition != &cmp->result) {
        return false;
    }
    if (rv_block_has_phi(br->data.br_inst.true_block) || rv_block_has_phi(br->data.br_inst.false_block)) {
        return false;
    }
    return rv_count_function_uses(function, &cmp->result) == 1;
}

static void rv_emit_compare_branch(RVFrame *frame, IRInstruction *cmp, IRInstruction *br) {
    if (cmp->kind == IR_INST_ICMP) {
        rv_emit_icmp_branch(frame, cmp, br);
    } else {
        rv_emit_fcmp_branch(frame, cmp, br);
    }
}

static void rv_emit_gep_to_reg(RVFrame *frame, IRInstruction *inst, const char *dst) {
    FILE *out = frame->out;
    IRType *current = inst->data.gep_inst.source_element_type;
    rv_load_int_value(frame, inst->data.gep_inst.base_ptr, "t0");
    for (int i = 0; i < inst->data.gep_inst.indices.count; ++i) {
        IRValue *index = inst->data.gep_inst.indices.items[i];
        int stride = 0;
        if (i == 0) {
            stride = rv_type_size(current);
        } else {
            if (current != NULL && current->kind == IR_TYPE_ARRAY) {
                current = current->data.array.element;
            }
            stride = rv_type_size(current);
        }
        int const_index = 0;
        if (rv_const_i32_value(index, &const_index)) {
            long long offset = (long long)const_index * (long long)stride;
            if (offset != 0) {
                fprintf(out, "  li t1, %lld\n", offset);
                fputs("  add t0, t0, t1\n", out);
            }
            continue;
        }
        rv_load_int_value(frame, index, "t1");
        if (stride != 1) {
            fprintf(out, "  li t2, %d\n", stride);
            fputs("  mul t1, t1, t2\n", out);
        }
        fputs("  add t0, t0, t1\n", out);
    }
    if (strcmp(dst, "t0") != 0) {
        fprintf(out, "  mv %s, t0\n", dst);
    }
}

static void rv_emit_gep(RVFrame *frame, IRInstruction *inst) {
    if (!rv_inst_has_result_home(frame, inst)) {
        return;
    }
    rv_emit_gep_to_reg(frame, inst, "t0");
    rv_store_int_slot(frame, &inst->result, "t0");
}

static void rv_emit_call(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    int int_regs = 0;
    int float_regs = 0;
    int stack_slots = 0;
    for (int i = 0; i < inst->data.call_inst.args.count; ++i) {
        IRValue *arg = inst->data.call_inst.args.items[i];
        if (rv_function_param_uses_float_reg(inst->data.call_inst.callee, i)) {
            if (float_regs < 8) {
                fprintf(out, "  # arg%d -> fa%d\n", i, float_regs);
                rv_load_float_value(frame, arg, str_printf("fa%d", float_regs));
                float_regs++;
            } else {
                rv_load_float_value(frame, arg, "ft0");
                rv_emit_mem(out, "fsw", "ft0", stack_slots * 8, "sp");
                stack_slots++;
            }
        } else if (rv_function_param_uses_int_reg(inst->data.call_inst.callee, i)) {
            if (int_regs < 8) {
                fprintf(out, "  # arg%d -> a%d\n", i, int_regs);
                rv_load_int_value(frame, arg, str_printf("a%d", int_regs));
                int_regs++;
            } else {
                rv_load_int_value(frame, arg, "t0");
                rv_emit_mem(out, "sd", "t0", stack_slots * 8, "sp");
                stack_slots++;
            }
        }
    }
    fprintf(out, "  call %s\n", inst->data.call_inst.callee != NULL
                                  ? rv_symbol_name(inst->data.call_inst.callee->name)
                                  : "unknown_callee");
    if (inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID) {
        if (rv_value_is_float(&inst->result)) {
            rv_store_float_slot(frame, &inst->result, "fa0");
        } else {
            rv_store_int_slot(frame, &inst->result, "a0");
        }
    }
}

static void rv_emit_instruction(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    switch (inst->kind) {
        case IR_INST_ALLOCA:
            return;
        case IR_INST_LOAD:
            rv_emit_load_inst(frame, inst);
            return;
        case IR_INST_STORE:
            rv_emit_store_inst(frame, inst);
            return;
        case IR_INST_PHI:
            return;
        case IR_INST_ADD:
            rv_emit_int_binary(frame, inst, "addw");
            return;
        case IR_INST_SUB:
            rv_emit_int_binary(frame, inst, "subw");
            return;
        case IR_INST_MUL:
            rv_emit_int_binary(frame, inst, "mulw");
            return;
        case IR_INST_SDIV:
            rv_emit_int_binary(frame, inst, "divw");
            return;
        case IR_INST_SREM:
            rv_emit_int_binary(frame, inst, "remw");
            return;
        case IR_INST_FADD:
            rv_emit_float_binary(frame, inst, "fadd.s");
            return;
        case IR_INST_FSUB:
            rv_emit_float_binary(frame, inst, "fsub.s");
            return;
        case IR_INST_FMUL:
            rv_emit_float_binary(frame, inst, "fmul.s");
            return;
        case IR_INST_FDIV:
            rv_emit_float_binary(frame, inst, "fdiv.s");
            return;
        case IR_INST_ICMP:
            rv_emit_icmp(frame, inst);
            return;
        case IR_INST_FCMP:
            rv_emit_fcmp(frame, inst);
            return;
        case IR_INST_ZEXT:
            if (!rv_inst_has_result_home(frame, inst)) {
                return;
            }
            rv_load_int_value(frame, inst->data.cast_inst.value, "t0");
            fputs("  andi t0, t0, 1\n", out);
            rv_store_int_slot(frame, &inst->result, "t0");
            return;
        case IR_INST_SITOFP:
            rv_load_int_value(frame, inst->data.cast_inst.value, "t0");
            fputs("  fcvt.s.w ft0, t0\n", out);
            rv_store_float_slot(frame, &inst->result, "ft0");
            return;
        case IR_INST_FPTOSI:
            rv_load_float_value(frame, inst->data.cast_inst.value, "ft0");
            fputs("  fcvt.w.s t0, ft0, rtz\n", out);
            rv_store_int_slot(frame, &inst->result, "t0");
            return;
        case IR_INST_BR: {
            char *true_label = rv_block_label(frame->function, inst->data.br_inst.true_block);
            if (inst->data.br_inst.is_conditional) {
                char *false_label = rv_block_label(frame->function, inst->data.br_inst.false_block);
                rv_load_int_value(frame, inst->data.br_inst.condition, "t0");
                if (rv_block_has_phi(inst->data.br_inst.true_block) ||
                        rv_block_has_phi(inst->data.br_inst.false_block)) {
                    char *copy_true = str_printf(".L_%s_phi_%d",
                                                 rv_symbol_name(frame->function->name),
                                                 frame->phi_label_id++);
                    fprintf(out, "  bnez t0, %s\n", copy_true);
                    rv_emit_phi_moves(frame, inst->parent, inst->data.br_inst.false_block);
                    fprintf(out, "  j %s\n", false_label);
                    fprintf(out, "%s:\n", copy_true);
                    rv_emit_phi_moves(frame, inst->parent, inst->data.br_inst.true_block);
                    fprintf(out, "  j %s\n", true_label);
                } else {
                    fprintf(out, "  bnez t0, %s\n", true_label);
                    fprintf(out, "  j %s\n", false_label);
                }
            } else {
                rv_emit_phi_moves(frame, inst->parent, inst->data.br_inst.true_block);
                fprintf(out, "  j %s\n", true_label);
            }
            return;
        }
        case IR_INST_RET:
            if (inst->data.ret_inst.value != NULL) {
                if (rv_value_is_float(inst->data.ret_inst.value)) {
                    rv_load_float_value(frame, inst->data.ret_inst.value, "fa0");
                } else {
                    rv_load_int_value(frame, inst->data.ret_inst.value, "a0");
                }
            }
            fprintf(out, "  j .L_%s_return\n", rv_symbol_name(frame->function->name));
            return;
        case IR_INST_CALL:
            rv_emit_call(frame, inst);
            return;
        case IR_INST_GETELEMENTPTR:
            rv_emit_gep(frame, inst);
            return;
        case IR_INST_BITCAST:
            if (!rv_inst_has_result_home(frame, inst)) {
                return;
            }
            rv_load_int_value(frame, inst->data.bitcast_inst.value, "t0");
            rv_store_int_slot(frame, &inst->result, "t0");
            return;
    }
}

static void rv_store_incoming_params(RVFrame *frame) {
    FILE *out = frame->out;
    int int_regs = 0;
    int float_regs = 0;
    int stack_slots = 0;
    for (int i = 0; i < frame->function->params.count; ++i) {
        IRParameter *param = frame->function->params.items[i];
        RVSlot *slot = rv_find_slot(frame, &param->value);
        if (slot == NULL) {
            continue;
        }
        if (rv_value_is_float(&param->value)) {
            if (float_regs < 8) {
                rv_store_float_slot(frame, &param->value, str_printf("fa%d", float_regs));
                float_regs++;
            } else {
                rv_emit_mem(out, "flw", "ft0", stack_slots * 8, "s0");
                rv_store_float_slot(frame, &param->value, "ft0");
                stack_slots++;
            }
        } else {
            if (int_regs < 8) {
                rv_store_int_slot(frame, &param->value, str_printf("a%d", int_regs));
                int_regs++;
            } else {
                rv_emit_mem(out, "ld", "t0", stack_slots * 8, "s0");
                rv_store_int_slot(frame, &param->value, "t0");
                stack_slots++;
            }
        }
    }
}

static void rv_emit_function(IRFunction *function, FILE *out) {
    RVFrame frame;
    rv_prepare_frame(&frame, function);
    frame.out = out;
    fprintf(out, "  .globl %s\n  .align 2\n%s:\n",
            rv_symbol_name(function->name), rv_symbol_name(function->name));
    rv_emit_addi(out, "sp", "sp", -frame.frame_size);
    rv_emit_mem(out, "sd", "ra", frame.frame_size - 8, "sp");
    rv_emit_mem(out, "sd", "s0", frame.frame_size - 16, "sp");
    rv_emit_addi(out, "s0", "sp", frame.frame_size);
    for (int i = 0; i < 6; ++i) {
        if (frame.used_saved_regs[i]) {
            rv_emit_mem(out, "sd", g_rv_loop_regs[i], frame.saved_reg_offsets[i], "s0");
        }
    }
    rv_store_incoming_params(&frame);
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        char *label = rv_block_label(function, block);
        fprintf(out, "%s:\n", label);
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if ((inst->kind == IR_INST_ICMP || inst->kind == IR_INST_FCMP)
                    && rv_can_fuse_compare_branch(function, inst)) {
                rv_emit_compare_branch(&frame, inst, inst->next);
                inst = inst->next;
                continue;
            }
            rv_emit_instruction(&frame, inst);
        }
    }
    fprintf(out, ".L_%s_return:\n", rv_symbol_name(function->name));
    for (int i = 0; i < 6; ++i) {
        if (frame.used_saved_regs[i]) {
            rv_emit_mem(out, "ld", g_rv_loop_regs[i], frame.saved_reg_offsets[i], "s0");
        }
    }
    rv_emit_mem(out, "ld", "ra", frame.frame_size - 8, "sp");
    rv_emit_mem(out, "ld", "s0", frame.frame_size - 16, "sp");
    rv_emit_addi(out, "sp", "sp", frame.frame_size);
    fputs("  ret\n\n", out);
}

void emit_riscv_from_ir(IRModule *module, FILE *out) {
    fputs("  .attribute arch, \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0\"\n", out);
    fputs("  .option nopic\n", out);
    fputs("  .option norelax\n", out);
    rv_emit_globals(module, out);
    fputs("  .text\n", out);
    for (int i = 0; i < module->functions.count; ++i) {
        IRFunction *function = module->functions.items[i];
        if (!function->is_external) {
            rv_emit_function(function, out);
        }
    }
}

static void generate_program_asm(Program *program, FILE *out) {
    IRModule *module = ast_to_ir(program);
    optimize_ir_basic_blocks(module);
    emit_riscv_from_ir(module, out);
}

int main(int argc, char **argv) {
    const char *input_path = parse_input_path(argc, argv);
    const char *output_path = parse_output_path(argc, argv);
    if (input_path == NULL) {
        return 1;
    }
    yyin = fopen(input_path, "r");
    if (yyin == NULL) {
        return 1;
    }
    if (yyparse() != 0 || g_program == NULL) {
        fclose(yyin);
        return 1;
    }
    if (has_flag(argc, argv, "--emit-mid-ir") || has_flag(argc, argv, "-emit-mid-ir")) {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fclose(yyin);
            return 1;
        }
        generate_program_mid_ir(g_program, out);
        fclose(out);
    } else if (has_flag(argc, argv, "--emit-mem-ir") || has_flag(argc, argv, "-emit-mem-ir")) {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fclose(yyin);
            return 1;
        }
        generate_program_mem_ir(g_program, out);
        fclose(out);
    } else if (has_flag(argc, argv, "--emit-llvm") || has_flag(argc, argv, "-emit-llvm")) {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fclose(yyin);
            return 1;
        }
        generate_program_ir(g_program, out);
        fclose(out);
    } else {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fclose(yyin);
            return 1;
        }
        generate_program_asm(g_program, out);
        fclose(out);
    }
    fclose(yyin);
    return 0;
}
