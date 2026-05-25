#define _POSIX_C_SOURCE 200809L

#include "compiler.h"

#include <errno.h>
#include <ctype.h>
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
    int dim_count;
    int *dims;
    bool is_pointer_value;
    bool is_param_array;
} Value;

typedef struct {
    char *ptr;
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

static char *llvm_type_from_dims(const int *dims, int count) {
    if (count == 0) {
        return xstrdup("i32");
    }
    char *sub = llvm_type_from_dims(dims + 1, count - 1);
    char *res = str_printf("[%d x %s]", dims[0], sub);
    free(sub);
    return res;
}

static char *llvm_flat_array_type(int total) {
    return str_printf("[%d x i32]", total);
}

static char *llvm_subarray_ptr_type(const int *dims, int dim_count) {
    if (dim_count <= 1) {
        return xstrdup("i32*");
    }
    char *sub = llvm_type_from_dims(dims + 1, dim_count - 1);
    char *res = str_printf("%s*", sub);
    free(sub);
    return res;
}

static char *llvm_param_type(const Param *param) {
    if (!param->is_array) {
        return xstrdup("i32");
    }
    char *sub = llvm_type_from_dims(param->dims.data, param->dims.count);
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

static int *const_init_to_flat(IRGen *gen, InitVal *init, const int *dims, int dim_count) {
    int total = object_slot_count(dims, dim_count);
    int *flat = (int *)xmalloc(sizeof(int) * (size_t)total);
    for (int i = 0; i < total; ++i) {
        flat[i] = 0;
    }
    Expr **slots = init_to_expr_slots(init, dims, dim_count, total);
    for (int i = 0; i < total; ++i) {
        if (slots[i] != NULL) {
            flat[i] = eval_const_expr(gen, slots[i]);
        }
    }
    free(slots);
    return flat;
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

static char *const_scalar_to_text(int value) {
    return str_printf("%d", value);
}

static char *const_array_to_text(const int *flat, const int *dims, int dim_count, int *pos) {
    if (dim_count == 0) {
        char *res = const_scalar_to_text(flat[(*pos)++]);
        return res;
    }
    char *subtype = llvm_type_from_dims(dims + 1, dim_count - 1);
    StrBuf sb;
    sb_init(&sb);
    sb_appendf(&sb, "[");
    for (int i = 0; i < dims[0]; ++i) {
        char *child = const_array_to_text(flat, dims + 1, dim_count - 1, pos);
        if (i > 0) {
            sb_appendf(&sb, ", ");
        }
        sb_appendf(&sb, "%s %s", subtype, child);
        free(child);
    }
    sb_appendf(&sb, "]");
    free(subtype);
    return sb.data;
}

static char *const_flat_array_to_text(const int *flat, int total) {
    StrBuf sb;
    sb_init(&sb);
    sb_append(&sb, "[");
    for (int i = 0; i < total; ++i) {
        if (i > 0) {
            sb_append(&sb, ", ");
        }
        sb_appendf(&sb, "i32 %d", flat[i]);
    }
    sb_append(&sb, "]");
    return sb.data;
}

static char *ensure_i32(IRGen *gen, Value v) {
    if (!v.is_pointer_value) {
        return xstrdup(v.value);
    }
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = load i32, i32* %s\n", tmp, v.value);
    return tmp;
}

static char *emit_icmp_to_i32(IRGen *gen, const char *pred, const char *lhs, const char *rhs) {
    char *icmp = new_temp(gen);
    char *res = new_temp(gen);
    emit_func(gen, "  %s = icmp %s i32 %s, %s\n", icmp, pred, lhs, rhs);
    emit_func(gen, "  %s = zext i1 %s to i32\n", res, icmp);
    return res;
}

static char *emit_nonzero_i1(IRGen *gen, const char *value) {
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = icmp ne i32 %s, 0\n", tmp, value);
    return tmp;
}

static Value make_value(char *value, int dim_count, int *dims, bool is_pointer_value, bool is_param_array) {
    Value v;
    v.value = value;
    v.dim_count = dim_count;
    v.dims = dims;
    v.is_pointer_value = is_pointer_value;
    v.is_param_array = is_param_array;
    return v;
}

static Address make_address(char *ptr, int dim_count, int *dims, bool is_param_array) {
    Address a;
    a.ptr = ptr;
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
        return make_address(ptr, remain, sym->info.dims + lval->indices.count, false);
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
            char *elem_type = llvm_type_from_dims(dims, dim_count);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 %s\n", tmp, elem_type, elem_type, ptr, idx);
            ptr = tmp;
            free(elem_type);
        } else {
            char *agg_type = llvm_type_from_dims(dims - 1 + 1, dim_count);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 %s\n", tmp, agg_type, agg_type, ptr, idx);
            ptr = tmp;
            free(agg_type);
            dims++;
            dim_count--;
        }
        is_param_array = false;
    }
    return make_address(ptr, dim_count, dims, is_param_array);
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
                return make_value(ptr, 0, NULL, false, true);
            }
            char *sub_type = llvm_type_from_dims(sym->info.dims + 1, remain - 1);
            char *cast = new_temp(gen);
            emit_func(gen, "  %s = bitcast i32* %s to %s*\n", cast, ptr, sub_type);
            free(sub_type);
            return make_value(cast, remain - 1, sym->info.dims + 1, false, true);
        }
        char *ptr = emit_flat_element_ptr(gen, sym, &indices);
        if (remain <= 0) {
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = load i32, i32* %s\n", tmp, ptr);
            return make_value(tmp, 0, NULL, false, false);
        }
        if (remain == 1) {
            return make_value(ptr, 0, NULL, false, true);
        }
        char *sub_type = llvm_subarray_ptr_type(sym->info.dims + lval->indices.count, remain);
        char *cast = new_temp(gen);
        emit_func(gen, "  %s = bitcast i32* %s to %s\n", cast, ptr, sub_type);
        free(sub_type);
        return make_value(cast, remain - 1, sym->info.dims + lval->indices.count + 1, false, true);
    }
    if (lval->indices.count == 0 && sym->info.is_param_array) {
        return make_value(sym->llvm_name, sym->info.dim_count, sym->info.dims, false, true);
    }
    if (lval->indices.count == 0 && sym->info.dim_count > 0) {
        if (sym->info.is_param_array) {
            return make_value(sym->llvm_name, sym->info.dim_count, sym->info.dims, false, true);
        }
        char *agg_type = llvm_type_from_dims(sym->info.dims, sym->info.dim_count);
        char *sub_type = llvm_type_from_dims(sym->info.dims + 1, sym->info.dim_count - 1);
        char *tmp = new_temp(gen);
        emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 0\n", tmp, agg_type, agg_type, sym->llvm_name);
        free(agg_type);
        free(sub_type);
        return make_value(tmp, sym->info.dim_count - 1, sym->info.dims + 1, false, true);
    }
    Address addr = gen_lval_address(gen, lval);
    if (addr.dim_count == 0) {
        char *tmp = new_temp(gen);
        emit_func(gen, "  %s = load i32, i32* %s\n", tmp, addr.ptr);
        return make_value(tmp, 0, NULL, false, false);
    }
    char *agg_type = llvm_type_from_dims(addr.dims, addr.dim_count);
    char *tmp = new_temp(gen);
    emit_func(gen, "  %s = getelementptr %s, %s* %s, i32 0, i32 0\n", tmp, agg_type, agg_type, addr.ptr);
    free(agg_type);
    return make_value(tmp, addr.dim_count - 1, addr.dims + 1, false, true);
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
    return make_value(tmp, 0, NULL, false, false);
}

Value gen_expr(IRGen *gen, Expr *expr) {
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_FLOAT_NUMBER:
            return make_value(str_printf("%d", expr->data.number), 0, NULL, false, false);
        case EXPR_LVAL:
            return gen_lval_expr(gen, expr->data.lval);
        case EXPR_GETINT: {
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = call i32 @getint()\n", tmp);
            return make_value(tmp, 0, NULL, false, false);
        }
        case EXPR_CALL: {
            if (strcmp(expr->data.call.name, "starttime") == 0) {
                emit_func(gen, "  call void @_sysy_starttime(i32 0)\n");
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "stoptime") == 0) {
                emit_func(gen, "  call void @_sysy_stoptime(i32 0)\n");
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getch") == 0) {
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call i32 @getch()\n", tmp);
                return make_value(tmp, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "getarray") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = call i32 @getarray(i32* %s)\n", tmp, arg.value);
                return make_value(tmp, 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putint") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *i32v = ensure_i32(gen, arg);
                emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putint(i32 %s)\n", i32v);
                return make_value(xstrdup("0"), 0, NULL, false, false);
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
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putarray") == 0) {
                Value n = gen_expr(gen, expr->data.call.args.items[0]);
                Value arr = gen_expr(gen, expr->data.call.args.items[1]);
                char *i32v = ensure_i32(gen, n);
                emit_func(gen, "  store i32 1, i32* @__sysy_output_state\n");
                emit_func(gen, "  call void @putarray(i32 %s, i32* %s)\n", i32v, arr.value);
                return make_value(xstrdup("0"), 0, NULL, false, false);
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
                    type = llvm_param_type(meta->params.items[i]);
                } else {
                    type = xstrdup("i32");
                }
                sb_appendf(&sb, "%s %s", type, arg.value);
                free(type);
            }
            if (meta != NULL && meta->ret_type == TYPE_VOID) {
                emit_func(gen, "  call void @%s(%s)\n", callee, sb.data ? sb.data : "");
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = call i32 @%s(%s)\n", tmp, callee, sb.data ? sb.data : "");
            return make_value(tmp, 0, NULL, false, false);
        }
        case EXPR_UNARY: {
            Value operand = gen_expr(gen, expr->data.unary.operand);
            char *op = ensure_i32(gen, operand);
            if (expr->data.unary.op == UNARY_PLUS) {
                return make_value(op, 0, NULL, false, false);
            }
            if (expr->data.unary.op == UNARY_MINUS) {
                char *tmp = new_temp(gen);
                emit_func(gen, "  %s = sub i32 0, %s\n", tmp, op);
                return make_value(tmp, 0, NULL, false, false);
            }
            char *icmp = new_temp(gen);
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = icmp eq i32 %s, 0\n", icmp, op);
            emit_func(gen, "  %s = zext i1 %s to i32\n", tmp, icmp);
            return make_value(tmp, 0, NULL, false, false);
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                return gen_short_circuit_expr(gen, expr);
            }
            Value lhs_v = gen_expr(gen, expr->data.binary.lhs);
            Value rhs_v = gen_expr(gen, expr->data.binary.rhs);
            char *lhs = ensure_i32(gen, lhs_v);
            char *rhs = ensure_i32(gen, rhs_v);
            char *tmp = new_temp(gen);
            switch (op) {
                case BIN_ADD:
                    emit_func(gen, "  %s = add i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, 0, NULL, false, false);
                case BIN_SUB:
                    emit_func(gen, "  %s = sub i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, 0, NULL, false, false);
                case BIN_MUL:
                    emit_func(gen, "  %s = mul i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, 0, NULL, false, false);
                case BIN_DIV:
                    emit_func(gen, "  %s = sdiv i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, 0, NULL, false, false);
                case BIN_MOD:
                    emit_func(gen, "  %s = srem i32 %s, %s\n", tmp, lhs, rhs);
                    return make_value(tmp, 0, NULL, false, false);
                case BIN_LT:
                    return make_value(emit_icmp_to_i32(gen, "slt", lhs, rhs), 0, NULL, false, false);
                case BIN_GT:
                    return make_value(emit_icmp_to_i32(gen, "sgt", lhs, rhs), 0, NULL, false, false);
                case BIN_LE:
                    return make_value(emit_icmp_to_i32(gen, "sle", lhs, rhs), 0, NULL, false, false);
                case BIN_GE:
                    return make_value(emit_icmp_to_i32(gen, "sge", lhs, rhs), 0, NULL, false, false);
                case BIN_EQ:
                    return make_value(emit_icmp_to_i32(gen, "eq", lhs, rhs), 0, NULL, false, false);
                case BIN_NE:
                    return make_value(emit_icmp_to_i32(gen, "ne", lhs, rhs), 0, NULL, false, false);
                default:
                    return make_value(xstrdup("0"), 0, NULL, false, false);
            }
        }
    }
    return make_value(xstrdup("0"), 0, NULL, false, false);
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
    char *i32v = ensure_i32(gen, cond);
    char *i1 = emit_nonzero_i1(gen, i32v);
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
        if (format[i] == '%' && i + 1 < len - 1 && format[i + 1] == 'd') {
            Value v = gen_expr(gen, args->items[arg_index++]);
            char *i32v = ensure_i32(gen, v);
            emit_func(gen, "  store i32 0, i32* @__sysy_output_state\n");
            emit_func(gen, "  call void @putint(i32 %s)\n", i32v);
            ++i;
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
            char *i32v = ensure_i32(gen, v);
            emit_func(gen, "  store i32 %s, i32* %s\n", i32v, addr.ptr);
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
                char *i32v = ensure_i32(gen, v);
                emit_func(gen, "  ret i32 %s\n", i32v);
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
        char *i32v = ensure_i32(gen, v);
        emit_func(gen, "  store i32 %s, i32* %s\n", i32v, elem_ptr);
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
                sym->const_scalar = eval_const_expr(gen, item->init->expr);
            }
            if (is_global) {
                int init_val = 0;
                if (item->init != NULL) {
                    init_val = eval_const_expr(gen, item->init->expr);
                }
                sym->llvm_name = str_printf("@g%d", gen->global_id++);
                emit_global(gen, "%s = dso_local global i32 %d\n", sym->llvm_name, init_val);
            } else {
                sym->llvm_name = emit_alloca(gen, "i32");
                if (item->init != NULL) {
                    Value v = gen_expr(gen, item->init->expr);
                    char *i32v = ensure_i32(gen, v);
                    emit_func(gen, "  store i32 %s, i32* %s\n", i32v, sym->llvm_name);
                } else {
                    emit_func(gen, "  store i32 0, i32* %s\n", sym->llvm_name);
                }
            }
        } else {
            sym->flat_type = llvm_flat_array_type(sym->info.total_slots);
            if (decl->is_const && item->init != NULL) {
                sym->const_flat = const_init_to_flat(gen, item->init, item->dims.data, item->dims.count);
            }
            if (is_global) {
                sym->info.is_flat_storage = true;
                sym->llvm_name = str_printf("@g%d", gen->global_id++);
                if (item->init == NULL) {
                    emit_global(gen, "%s = dso_local global %s zeroinitializer\n", sym->llvm_name, sym->flat_type);
                } else {
                    int *flat = const_init_to_flat(gen, item->init, item->dims.data, item->dims.count);
                    char *text = const_flat_array_to_text(flat, sym->info.total_slots);
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
               func->ret_type == TYPE_INT ? "i32" : "void", function_llvm_name(gen, func->name));
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
            sym->llvm_name = emit_alloca(gen, "i32");
            emit_func(gen, "  store i32 %%p%d, i32* %s\n", i, sym->llvm_name);
        } else {
            sym->llvm_name = str_printf("%%p%d", i);
        }
    }
    gen_block(gen, func->block, false);
    if (!gen->current_block_terminated) {
        if (func->ret_type == TYPE_INT) {
            emit_func(gen, "  ret i32 0\n");
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

void generate_program_ir(Program *program, FILE *out) {
    IRGen gen;
    memset(&gen, 0, sizeof(gen));
    gen.out = out;
    sb_init(&gen.globals);
    sb_init(&gen.functions);
    push_scope(&gen);
    sb_append(&gen.globals, "source_filename = \"sysy.ll\"\n");
    sb_append(&gen.globals, "target triple = \"riscv64-unknown-linux-gnu\"\n\n");
    sb_append(&gen.globals, "@__sysy_output_state = dso_local global i32 2\n\n");
    sb_append(&gen.globals, "declare i32 @getint()\n");
    sb_append(&gen.globals, "declare i32 @getch()\n");
    sb_append(&gen.globals, "declare i32 @getarray(i32*)\n");
    sb_append(&gen.globals, "declare i8* @memset(i8*, i32, i64)\n");
    sb_append(&gen.globals, "declare void @putint(i32)\n");
    sb_append(&gen.globals, "declare void @putch(i32)\n");
    sb_append(&gen.globals, "declare void @putarray(i32, i32*)\n\n");
    sb_append(&gen.globals, "declare void @_sysy_starttime(i32)\n");
    sb_append(&gen.globals, "declare void @_sysy_stoptime(i32)\n\n");

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

int yyparse(void);

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

typedef struct {
    FILE *out;
    StrBuf globals;
    StrBuf functions;
    StrBuf *current_allocas;
    StrBuf *current_body;
    Scope *scopes;
    FunctionSymbol *functions_meta;
    int ir_global_id;
    int ir_function_id;
    int temp_id;
    int ir_label_id;
    TypeSpec ir_current_ret_type;
    bool ir_current_block_terminated;
    StringList ir_break_labels;
    StringList ir_continue_labels;
    StrBuf data;
    StrBuf bss;
    StrBuf text;
    StrBuf body;
    int global_id;
    int function_id;
    int label_id;
    int next_stack_offset;
    int frame_size;
    TypeSpec current_ret_type;
    char *return_label;
    bool current_block_terminated;
    StringList break_labels;
    StringList continue_labels;
} AsmGen;

typedef struct {
    int dim_count;
    int *dims;
    bool is_pointer;
    bool is_param_array;
} AsmValue;

static int align_to(int value, int align) {
    return (value + align - 1) / align * align;
}

static void asm_emit(AsmGen *gen, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vappendf(&gen->body, fmt, ap);
    va_end(ap);
}

static char *asm_new_label(AsmGen *gen, const char *prefix) {
    return str_printf(".L_%s_%d", prefix, gen->label_id++);
}

static void asm_mark_label(AsmGen *gen, const char *label) {
    asm_emit(gen, "%s:\n", label);
    gen->current_block_terminated = false;
}

static void asm_load_imm(AsmGen *gen, const char *reg, int value) {
    asm_emit(gen, "  li %s, %d\n", reg, value);
}

static bool asm_fits_imm12(int value) {
    return value >= -2048 && value <= 2047;
}

static const char *asm_temp_avoiding(const char *a, const char *b) {
    if ((a == NULL || strcmp(a, "t6") != 0) &&
        (b == NULL || strcmp(b, "t6") != 0)) {
        return "t6";
    }
    if ((a == NULL || strcmp(a, "t5") != 0) &&
        (b == NULL || strcmp(b, "t5") != 0)) {
        return "t5";
    }
    return "t4";
}

static bool asm_abs_power_of_two(int value, int *shift, int *abs_value) {
    if (value == 0 || value == INT_MIN) {
        return false;
    }
    int v = value < 0 ? -value : value;
    if ((v & (v - 1)) != 0) {
        return false;
    }
    int s = 0;
    while ((1 << s) != v) {
        ++s;
    }
    if (shift != NULL) {
        *shift = s;
    }
    if (abs_value != NULL) {
        *abs_value = v;
    }
    return true;
}

static void asm_emit_add_imm(AsmGen *gen, const char *dst, const char *base, int imm) {
    if (imm == 0) {
        if (strcmp(dst, base) != 0) {
            asm_emit(gen, "  mv %s, %s\n", dst, base);
        }
        return;
    }
    if (asm_fits_imm12(imm)) {
        asm_emit(gen, "  addi %s, %s, %d\n", dst, base, imm);
        return;
    }
    const char *tmp = asm_temp_avoiding(dst, base);
    asm_emit(gen, "  li %s, %d\n", tmp, imm);
    asm_emit(gen, "  add %s, %s, %s\n", dst, base, tmp);
}

static void asm_emit_addw_imm(AsmGen *gen, const char *dst, const char *src, int imm) {
    if (imm == 0) {
        if (strcmp(dst, src) != 0) {
            asm_emit(gen, "  mv %s, %s\n", dst, src);
        }
        return;
    }
    if (asm_fits_imm12(imm)) {
        asm_emit(gen, "  addiw %s, %s, %d\n", dst, src, imm);
        return;
    }
    const char *tmp = asm_temp_avoiding(dst, src);
    asm_emit(gen, "  li %s, %d\n", tmp, imm);
    asm_emit(gen, "  addw %s, %s, %s\n", dst, src, tmp);
}

static void asm_emit_subw_imm(AsmGen *gen, const char *dst, const char *src, int imm) {
    if (imm != INT_MIN) {
        asm_emit_addw_imm(gen, dst, src, -imm);
        return;
    }
    asm_load_imm(gen, "t6", imm);
    asm_emit(gen, "  subw %s, %s, t6\n", dst, src);
}

static void asm_emit_mem(AsmGen *gen, const char *op, const char *reg, int offset, const char *base) {
    if (asm_fits_imm12(offset)) {
        asm_emit(gen, "  %s %s, %d(%s)\n", op, reg, offset, base);
        return;
    }
    const char *tmp = asm_temp_avoiding(reg, base);
    asm_emit(gen, "  li %s, %d\n", tmp, offset);
    asm_emit(gen, "  add %s, %s, %s\n", tmp, base, tmp);
    asm_emit(gen, "  %s %s, 0(%s)\n", op, reg, tmp);
}

static void sb_emit_add_imm(StrBuf *sb, const char *dst, const char *base, int imm) {
    if (imm == 0) {
        if (strcmp(dst, base) != 0) {
            sb_appendf(sb, "  mv %s, %s\n", dst, base);
        }
        return;
    }
    if (asm_fits_imm12(imm)) {
        sb_appendf(sb, "  addi %s, %s, %d\n", dst, base, imm);
        return;
    }
    const char *tmp = asm_temp_avoiding(dst, base);
    sb_appendf(sb, "  li %s, %d\n", tmp, imm);
    sb_appendf(sb, "  add %s, %s, %s\n", dst, base, tmp);
}

static void sb_emit_mem(StrBuf *sb, const char *op, const char *reg, int offset, const char *base) {
    if (asm_fits_imm12(offset)) {
        sb_appendf(sb, "  %s %s, %d(%s)\n", op, reg, offset, base);
        return;
    }
    const char *tmp = asm_temp_avoiding(reg, base);
    sb_appendf(sb, "  li %s, %d\n", tmp, offset);
    sb_appendf(sb, "  add %s, %s, %s\n", tmp, base, tmp);
    sb_appendf(sb, "  %s %s, 0(%s)\n", op, reg, tmp);
}

static void asm_push_a0(AsmGen *gen) {
    asm_emit(gen, "  addi sp, sp, -16\n");
    asm_emit(gen, "  sd a0, 8(sp)\n");
}

static void asm_pop_to(AsmGen *gen, const char *reg) {
    asm_emit(gen, "  ld %s, 8(sp)\n", reg);
    asm_emit(gen, "  addi sp, sp, 16\n");
}

static void asm_load_symbol_addr(AsmGen *gen, Symbol *sym, const char *reg) {
    if (sym->is_global) {
        asm_emit(gen, "  lla %s, %s\n", reg, sym->llvm_name);
    } else {
        asm_emit_add_imm(gen, reg, "s0", sym->stack_offset);
    }
}

static AsmValue asm_make_value(int dim_count, int *dims, bool is_pointer, bool is_param_array) {
    AsmValue v;
    v.dim_count = dim_count;
    v.dims = dims;
    v.is_pointer = is_pointer;
    v.is_param_array = is_param_array;
    return v;
}

static void asm_gen_expr(AsmGen *gen, Expr *expr);
static void asm_gen_cond(AsmGen *gen, Expr *expr, const char *true_label, const char *false_label);
static TypeSpec asm_expr_type(AsmGen *gen, Expr *expr);

static int asm_alloc_stack(AsmGen *gen, int bytes) {
    bytes = align_to(bytes, 4);
    gen->next_stack_offset -= bytes;
    return gen->next_stack_offset;
}

static FunctionSymbol *asm_add_function_meta(AsmGen *gen, FuncDef *func) {
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

static const char *asm_function_name(AsmGen *gen, const char *name) {
    FunctionSymbol *meta = lookup_function_meta((IRGen *)gen, name);
    return meta != NULL ? meta->llvm_name : name;
}

static void asm_emit_load(AsmGen *gen, const char *reg, int offset, const char *base) {
    asm_emit_mem(gen, "lw", reg, offset, base);
}

static void asm_emit_store(AsmGen *gen, const char *reg, int offset, const char *base) {
    asm_emit_mem(gen, "sw", reg, offset, base);
}

static void asm_emit_and_imm(AsmGen *gen, const char *dst, const char *src, int imm) {
    if (asm_fits_imm12(imm)) {
        asm_emit(gen, "  andi %s, %s, %d\n", dst, src, imm);
        return;
    }
    const char *tmp = asm_temp_avoiding(dst, src);
    asm_load_imm(gen, tmp, imm);
    asm_emit(gen, "  and %s, %s, %s\n", dst, src, tmp);
}

static bool asm_emit_mul_const(AsmGen *gen, const char *dst, const char *src, int value) {
    int shift = 0;
    int abs_value = 0;
    if (value == 0) {
        asm_load_imm(gen, dst, 0);
        return true;
    }
    if (value == 1) {
        if (strcmp(dst, src) != 0) {
            asm_emit(gen, "  mv %s, %s\n", dst, src);
        }
        return true;
    }
    if (value == -1) {
        asm_emit(gen, "  negw %s, %s\n", dst, src);
        return true;
    }
    if (asm_abs_power_of_two(value, &shift, &abs_value)) {
        asm_emit(gen, "  slliw %s, %s, %d\n", dst, src, shift);
        if (value < 0) {
            asm_emit(gen, "  negw %s, %s\n", dst, dst);
        }
        return true;
    }
    return false;
}

static bool asm_emit_div_const(AsmGen *gen, const char *dst, const char *src, int value) {
    int shift = 0;
    int abs_value = 0;
    if (value == 0) {
        return false;
    }
    if (value == 1) {
        if (strcmp(dst, src) != 0) {
            asm_emit(gen, "  mv %s, %s\n", dst, src);
        }
        return true;
    }
    if (value == -1) {
        asm_emit(gen, "  negw %s, %s\n", dst, src);
        return true;
    }
    if (!asm_abs_power_of_two(value, &shift, &abs_value)) {
        return false;
    }
    asm_emit(gen, "  sraiw t6, %s, 31\n", src);
    asm_emit_and_imm(gen, "t6", "t6", abs_value - 1);
    asm_emit(gen, "  addw %s, %s, t6\n", dst, src);
    asm_emit(gen, "  sraiw %s, %s, %d\n", dst, dst, shift);
    if (value < 0) {
        asm_emit(gen, "  negw %s, %s\n", dst, dst);
    }
    return true;
}

static bool asm_emit_mod_const(AsmGen *gen, const char *dst, const char *src, int value) {
    int shift = 0;
    int abs_value = 0;
    if (value == 0) {
        return false;
    }
    if (value == 1 || value == -1) {
        asm_load_imm(gen, dst, 0);
        return true;
    }
    if (!asm_abs_power_of_two(value, &shift, &abs_value)) {
        return false;
    }
    asm_emit(gen, "  sraiw t6, %s, 31\n", src);
    asm_emit_and_imm(gen, "t6", "t6", abs_value - 1);
    asm_emit(gen, "  addw t6, %s, t6\n", src);
    asm_emit(gen, "  sraiw t6, t6, %d\n", shift);
    asm_emit(gen, "  slliw t6, t6, %d\n", shift);
    asm_emit(gen, "  subw %s, %s, t6\n", dst, src);
    return true;
}

static void asm_scale_index(AsmGen *gen, int stride) {
    if (stride != 1) {
        int shift = 0;
        int abs_value = 0;
        if (asm_abs_power_of_two(stride, &shift, &abs_value) && stride > 0) {
            asm_emit(gen, "  slli a0, a0, %d\n", shift);
        } else {
            asm_emit(gen, "  li t0, %d\n", stride);
            asm_emit(gen, "  mul a0, a0, t0\n");
        }
    }
}

static bool asm_eval_int_const_expr(AsmGen *gen, Expr *expr, int *out) {
    if (expr == NULL) {
        *out = 0;
        return true;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            *out = expr->data.number;
            return true;
        case EXPR_LVAL: {
            LVal *lval = expr->data.lval;
            Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
            if (sym != NULL && sym->is_const_scalar && lval->indices.count == 0 &&
                sym->value_type == TYPE_INT) {
                *out = sym->const_scalar;
                return true;
            }
            return false;
        }
        case EXPR_UNARY: {
            int value = 0;
            if (!asm_eval_int_const_expr(gen, expr->data.unary.operand, &value)) {
                return false;
            }
            switch (expr->data.unary.op) {
                case UNARY_PLUS: *out = value; return true;
                case UNARY_MINUS: *out = -value; return true;
                case UNARY_NOT: *out = !value; return true;
            }
            return false;
        }
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            int lhs = 0;
            int rhs = 0;
            if (!asm_eval_int_const_expr(gen, expr->data.binary.lhs, &lhs)) {
                return false;
            }
            if (op == BIN_AND) {
                if (!lhs) {
                    *out = 0;
                    return true;
                }
                return asm_eval_int_const_expr(gen, expr->data.binary.rhs, &rhs) &&
                       (*out = (rhs != 0), true);
            }
            if (op == BIN_OR) {
                if (lhs) {
                    *out = 1;
                    return true;
                }
                return asm_eval_int_const_expr(gen, expr->data.binary.rhs, &rhs) &&
                       (*out = (rhs != 0), true);
            }
            if (!asm_eval_int_const_expr(gen, expr->data.binary.rhs, &rhs)) {
                return false;
            }
            if ((op == BIN_DIV || op == BIN_MOD) && rhs == 0) {
                return false;
            }
            switch (op) {
                case BIN_ADD: *out = lhs + rhs; return true;
                case BIN_SUB: *out = lhs - rhs; return true;
                case BIN_MUL: *out = lhs * rhs; return true;
                case BIN_DIV: *out = lhs / rhs; return true;
                case BIN_MOD: *out = lhs % rhs; return true;
                case BIN_LT: *out = lhs < rhs; return true;
                case BIN_GT: *out = lhs > rhs; return true;
                case BIN_LE: *out = lhs <= rhs; return true;
                case BIN_GE: *out = lhs >= rhs; return true;
                case BIN_EQ: *out = lhs == rhs; return true;
                case BIN_NE: *out = lhs != rhs; return true;
                case BIN_AND:
                case BIN_OR:
                    return false;
            }
            return false;
        }
        default:
            return false;
    }
}

static AsmValue asm_lval_address(AsmGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    if (sym == NULL) {
        asm_load_imm(gen, "a0", 0);
        return asm_make_value(0, NULL, false, false);
    }
    if (sym->info.is_param_array) {
        asm_emit_mem(gen, "ld", "t1", sym->stack_offset, "s0");
    } else {
        asm_load_symbol_addr(gen, sym, "t1");
    }
    for (int i = 0; i < lval->indices.count; ++i) {
        int stride = sym->info.is_param_array
                         ? product_dims(sym->info.dims, i, sym->info.dim_count)
                         : product_dims(sym->info.dims, i + 1, sym->info.dim_count);
        int index_value = 0;
        if (asm_eval_int_const_expr(gen, lval->indices.items[i], &index_value)) {
            int offset = index_value * stride * 4;
            if (offset != 0) {
                asm_emit_add_imm(gen, "t1", "t1", offset);
            }
        } else {
            asm_emit(gen, "  mv a0, t1\n");
            asm_push_a0(gen);
            asm_gen_expr(gen, lval->indices.items[i]);
            asm_scale_index(gen, stride * 4);
            asm_emit(gen, "  mv t0, a0\n");
            asm_pop_to(gen, "t1");
            asm_emit(gen, "  add t1, t1, t0\n");
        }
    }
    asm_emit(gen, "  mv a0, t1\n");
    int remain = (sym->info.is_param_array ? sym->info.dim_count + 1 : sym->info.dim_count)
                 - lval->indices.count;
    if (remain < 0) {
        remain = 0;
    }
    int dim_offset = sym->info.is_param_array ? lval->indices.count - 1 : lval->indices.count;
    if (dim_offset < 0) {
        dim_offset = 0;
    }
    if (dim_offset > sym->info.dim_count) {
        dim_offset = sym->info.dim_count;
    }
    return asm_make_value(remain, sym->info.dims + dim_offset, true, sym->info.is_param_array);
}

static AsmValue asm_gen_lval_expr(AsmGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    if (sym != NULL && sym->is_const_scalar && lval->indices.count == 0) {
        asm_load_imm(gen, "a0", sym->const_scalar);
        return asm_make_value(0, NULL, false, false);
    }
    if (sym != NULL && sym->info.is_param_array && lval->indices.count == 0) {
        asm_emit_mem(gen, "ld", "a0", sym->stack_offset, "s0");
        return asm_make_value(sym->info.dim_count, sym->info.dims, true, true);
    }
    AsmValue addr = asm_lval_address(gen, lval);
    if (addr.dim_count == 0) {
        asm_emit_load(gen, "a0", 0, "a0");
        return asm_make_value(0, NULL, false, false);
    }
    if (addr.dim_count == 1) {
        return asm_make_value(0, NULL, true, true);
    }
    return asm_make_value(addr.dim_count - 1, addr.dims + 1, true, true);
}

static TypeSpec asm_lval_type(AsmGen *gen, LVal *lval) {
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    return sym != NULL ? sym->value_type : TYPE_INT;
}

static TypeSpec asm_expr_type(AsmGen *gen, Expr *expr) {
    if (expr == NULL) {
        return TYPE_INT;
    }
    switch (expr->kind) {
        case EXPR_FLOAT_NUMBER:
            return TYPE_FLOAT;
        case EXPR_LVAL:
            return asm_lval_type(gen, expr->data.lval);
        case EXPR_CALL: {
            if (strcmp(expr->data.call.name, "getfloat") == 0) {
                return TYPE_FLOAT;
            }
            FunctionSymbol *meta = lookup_function_meta((IRGen *)gen, expr->data.call.name);
            return meta != NULL ? meta->ret_type : TYPE_INT;
        }
        case EXPR_UNARY:
            return expr->data.unary.op == UNARY_NOT ? TYPE_INT : asm_expr_type(gen, expr->data.unary.operand);
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_LT || op == BIN_GT || op == BIN_LE || op == BIN_GE ||
                op == BIN_EQ || op == BIN_NE || op == BIN_AND || op == BIN_OR) {
                return TYPE_INT;
            }
            return (asm_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT ||
                    asm_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
        }
        default:
            return TYPE_INT;
    }
}

static bool asm_expr_is_pointer_value(AsmGen *gen, Expr *expr) {
    if (expr == NULL || expr->kind != EXPR_LVAL) {
        return false;
    }
    LVal *lval = expr->data.lval;
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    if (sym == NULL) {
        return false;
    }
    if (sym->info.is_param_array) {
        return lval->indices.count < sym->info.dim_count + 1;
    }
    return lval->indices.count < sym->info.dim_count;
}

static Symbol *asm_scalar_lval_target(AsmGen *gen, LVal *lval) {
    if (lval == NULL || lval->indices.count != 0) {
        return NULL;
    }
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    if (sym == NULL || sym->is_function || sym->info.is_param_array ||
        sym->info.dim_count != 0) {
        return NULL;
    }
    return sym;
}

static void asm_store_scalar_reg(AsmGen *gen, Symbol *sym, const char *reg) {
    if (sym->is_global) {
        const char *tmp = asm_temp_avoiding(reg, NULL);
        asm_emit(gen, "  lla %s, %s\n", tmp, sym->llvm_name);
        asm_emit_store(gen, reg, 0, tmp);
    } else {
        asm_emit_store(gen, reg, sym->stack_offset, "s0");
    }
}

static TypeSpec asm_call_param_type(const char *name, FunctionSymbol *meta, int index) {
    if (meta != NULL && index < meta->params.count) {
        return meta->params.items[index]->type;
    }
    if ((strcmp(name, "getfarray") == 0 && index == 0) ||
        (strcmp(name, "putfarray") == 0 && index == 1)) {
        return TYPE_FLOAT;
    }
    return TYPE_INT;
}

static bool asm_call_param_is_array(const char *name, FunctionSymbol *meta, int index) {
    if (meta != NULL && index < meta->params.count) {
        return meta->params.items[index]->is_array;
    }
    return (strcmp(name, "getarray") == 0 && index == 0) ||
           (strcmp(name, "getfarray") == 0 && index == 0) ||
           (strcmp(name, "putarray") == 0 && index == 1) ||
           (strcmp(name, "putfarray") == 0 && index == 1);
}

static void asm_gen_float_to_freg(AsmGen *gen, Expr *expr, const char *freg) {
    TypeSpec type = asm_expr_type(gen, expr);
    asm_gen_expr(gen, expr);
    if (type == TYPE_FLOAT) {
        asm_emit(gen, "  fmv.w.x %s, a0\n", freg);
    } else {
        asm_emit(gen, "  fcvt.s.w %s, a0\n", freg);
    }
}

static void asm_convert_a0(AsmGen *gen, TypeSpec from, TypeSpec to) {
    if (from == to) {
        return;
    }
    if (from == TYPE_INT && to == TYPE_FLOAT) {
        asm_emit(gen, "  fcvt.s.w ft0, a0\n");
        asm_emit(gen, "  fmv.x.w a0, ft0\n");
        return;
    }
    if (from == TYPE_FLOAT && to == TYPE_INT) {
        asm_emit(gen, "  fmv.w.x ft0, a0\n");
        asm_emit(gen, "  fcvt.w.s a0, ft0, rtz\n");
        return;
    }
}

static void asm_float_truth_to_a0(AsmGen *gen, Expr *expr) {
    asm_gen_float_to_freg(gen, expr, "ft0");
    asm_emit(gen, "  fmv.w.x ft1, zero\n");
    asm_emit(gen, "  feq.s a0, ft0, ft1\n");
    asm_emit(gen, "  xori a0, a0, 1\n");
}

static void asm_gen_float_binary(AsmGen *gen, BinaryOp op, Expr *lhs, Expr *rhs) {
    asm_gen_float_to_freg(gen, lhs, "ft0");
    asm_emit(gen, "  addi sp, sp, -16\n");
    asm_emit(gen, "  fsw ft0, 12(sp)\n");
    asm_gen_float_to_freg(gen, rhs, "ft1");
    asm_emit(gen, "  flw ft0, 12(sp)\n");
    asm_emit(gen, "  addi sp, sp, 16\n");
    switch (op) {
        case BIN_ADD:
            asm_emit(gen, "  fadd.s ft0, ft0, ft1\n");
            asm_emit(gen, "  fmv.x.w a0, ft0\n");
            return;
        case BIN_SUB:
            asm_emit(gen, "  fsub.s ft0, ft0, ft1\n");
            asm_emit(gen, "  fmv.x.w a0, ft0\n");
            return;
        case BIN_MUL:
            asm_emit(gen, "  fmul.s ft0, ft0, ft1\n");
            asm_emit(gen, "  fmv.x.w a0, ft0\n");
            return;
        case BIN_DIV:
            asm_emit(gen, "  fdiv.s ft0, ft0, ft1\n");
            asm_emit(gen, "  fmv.x.w a0, ft0\n");
            return;
        case BIN_LT: asm_emit(gen, "  flt.s a0, ft0, ft1\n"); return;
        case BIN_GT: asm_emit(gen, "  flt.s a0, ft1, ft0\n"); return;
        case BIN_LE: asm_emit(gen, "  fle.s a0, ft0, ft1\n"); return;
        case BIN_GE: asm_emit(gen, "  fle.s a0, ft1, ft0\n"); return;
        case BIN_EQ: asm_emit(gen, "  feq.s a0, ft0, ft1\n"); return;
        case BIN_NE:
            asm_emit(gen, "  feq.s a0, ft0, ft1\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return;
        default:
            asm_load_imm(gen, "a0", 0);
            return;
    }
}

static void asm_call_function(AsmGen *gen, const char *name, ExprList *args, FunctionSymbol *meta) {
    int count = args->count;
    for (int i = 0; i < count; ++i) {
        asm_gen_expr(gen, args->items[i]);
        TypeSpec arg_type = asm_expr_type(gen, args->items[i]);
        TypeSpec param_type = asm_call_param_type(name, meta, i);
        bool param_array = asm_call_param_is_array(name, meta, i);
        if (!param_array && !asm_expr_is_pointer_value(gen, args->items[i])) {
            asm_convert_a0(gen, arg_type, param_type);
        }
        asm_push_a0(gen);
    }
    int call_area = align_to(count * 8, 16);
    if (call_area > 0) {
        asm_emit_add_imm(gen, "sp", "sp", -call_area);
    }
    int int_reg = 0;
    int float_reg = 0;
    int stack_arg = 0;
    for (int i = 0; i < count; ++i) {
        TypeSpec param_type = asm_call_param_type(name, meta, i);
        bool param_array = asm_call_param_is_array(name, meta, i) || asm_expr_is_pointer_value(gen, args->items[i]);
        int off = call_area + 8 + (count - 1 - i) * 16;
        if (param_type == TYPE_FLOAT && !param_array && float_reg < 8) {
            char *arg_reg = str_printf("fa%d", float_reg++);
            asm_emit_mem(gen, "flw", arg_reg, off, "sp");
            free(arg_reg);
        } else if (param_type == TYPE_FLOAT && !param_array) {
            asm_emit_mem(gen, "flw", "ft0", off, "sp");
            asm_emit_mem(gen, "fsw", "ft0", stack_arg * 8, "sp");
            stack_arg++;
        } else if (int_reg < 8) {
            char *arg_reg = str_printf("a%d", int_reg++);
            asm_emit_mem(gen, "ld", arg_reg, off, "sp");
            free(arg_reg);
        } else {
            asm_emit_mem(gen, "ld", "t0", off, "sp");
            asm_emit_mem(gen, "sd", "t0", stack_arg * 8, "sp");
            stack_arg++;
        }
    }
    asm_emit(gen, "  call %s\n", name);
    if (call_area + count * 16 > 0) {
        asm_emit_add_imm(gen, "sp", "sp", call_area + count * 16);
    }
}

static void asm_store_incoming_param(AsmGen *gen, Param *param, Symbol *sym,
                                     int *int_reg, int *float_reg, int *stack_arg) {
    bool is_float_scalar = param->type == TYPE_FLOAT && !param->is_array;
    if (is_float_scalar && *float_reg < 8) {
        char *arg_reg = str_printf("fa%d", (*float_reg)++);
        asm_emit_mem(gen, "fsw", arg_reg, sym->stack_offset, "s0");
        free(arg_reg);
    } else if (is_float_scalar) {
        asm_emit_mem(gen, "flw", "ft0", (*stack_arg) * 8, "s0");
        asm_emit_mem(gen, "fsw", "ft0", sym->stack_offset, "s0");
        (*stack_arg)++;
    } else if (*int_reg < 8) {
        char *arg_reg = str_printf("a%d", (*int_reg)++);
        if (param->is_array) {
            asm_emit_mem(gen, "sd", arg_reg, sym->stack_offset, "s0");
        } else {
            asm_emit_store(gen, arg_reg, sym->stack_offset, "s0");
        }
        free(arg_reg);
    } else {
        asm_emit_mem(gen, "ld", "t0", (*stack_arg) * 8, "s0");
        if (param->is_array) {
            asm_emit_mem(gen, "sd", "t0", sym->stack_offset, "s0");
        } else {
            asm_emit_store(gen, "t0", sym->stack_offset, "s0");
        }
        (*stack_arg)++;
    }
}

static AsmValue asm_gen_short_circuit_expr(AsmGen *gen, Expr *expr) {
    char *true_label = asm_new_label(gen, "logic_true");
    char *false_label = asm_new_label(gen, "logic_false");
    char *end_label = asm_new_label(gen, "logic_end");
    asm_gen_cond(gen, expr, true_label, false_label);
    asm_mark_label(gen, true_label);
    asm_load_imm(gen, "a0", 1);
    asm_emit(gen, "  j %s\n", end_label);
    gen->current_block_terminated = true;
    asm_mark_label(gen, false_label);
    asm_load_imm(gen, "a0", 0);
    asm_mark_label(gen, end_label);
    return asm_make_value(0, NULL, false, false);
}

static int asm_inline_param_index(FunctionSymbol *meta, const char *name) {
    for (int i = 0; i < meta->params.count; ++i) {
        if (strcmp(meta->params.items[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static bool asm_inline_expr_is_simple_int(FunctionSymbol *meta, Expr *expr) {
    if (expr == NULL) {
        return true;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return true;
        case EXPR_LVAL:
            return expr->data.lval->indices.count == 0 &&
                   asm_inline_param_index(meta, expr->data.lval->name) >= 0;
        case EXPR_UNARY:
            return asm_inline_expr_is_simple_int(meta, expr->data.unary.operand);
        case EXPR_BINARY:
            return expr->data.binary.op != BIN_AND &&
                   expr->data.binary.op != BIN_OR &&
                   asm_inline_expr_is_simple_int(meta, expr->data.binary.lhs) &&
                   asm_inline_expr_is_simple_int(meta, expr->data.binary.rhs);
        default:
            return false;
    }
}

static bool asm_arg_is_side_effect_free(Expr *expr) {
    if (expr == NULL) {
        return true;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_FLOAT_NUMBER:
            return true;
        case EXPR_LVAL:
            for (int i = 0; i < expr->data.lval->indices.count; ++i) {
                if (!asm_arg_is_side_effect_free(expr->data.lval->indices.items[i])) {
                    return false;
                }
            }
            return true;
        case EXPR_UNARY:
            return asm_arg_is_side_effect_free(expr->data.unary.operand);
        case EXPR_BINARY:
            return asm_arg_is_side_effect_free(expr->data.binary.lhs) &&
                   asm_arg_is_side_effect_free(expr->data.binary.rhs);
        default:
            return false;
    }
}

static Expr *asm_inline_return_expr(FunctionSymbol *meta) {
    if (meta == NULL || meta->func == NULL || meta->ret_type != TYPE_INT ||
        meta->func->block == NULL || meta->func->block->items.count != 1 ||
        meta->func->block->items.kinds[0] != BLOCK_ITEM_STMT) {
        return NULL;
    }
    for (int i = 0; i < meta->params.count; ++i) {
        Param *param = meta->params.items[i];
        if (param->type != TYPE_INT || param->is_array) {
            return NULL;
        }
    }
    Stmt *stmt = (Stmt *)meta->func->block->items.items[0];
    if (stmt->kind != STMT_RETURN || stmt->data.return_expr == NULL) {
        return NULL;
    }
    if (!asm_inline_expr_is_simple_int(meta, stmt->data.return_expr)) {
        return NULL;
    }
    return stmt->data.return_expr;
}

static void asm_gen_inline_int_expr(AsmGen *gen, Expr *expr, FunctionSymbol *meta, ExprList *args) {
    if (expr == NULL) {
        asm_load_imm(gen, "a0", 0);
        return;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            asm_load_imm(gen, "a0", expr->data.number);
            return;
        case EXPR_LVAL: {
            int index = asm_inline_param_index(meta, expr->data.lval->name);
            if (index >= 0 && index < args->count) {
                asm_gen_expr(gen, args->items[index]);
                return;
            }
            asm_load_imm(gen, "a0", 0);
            return;
        }
        case EXPR_UNARY:
            asm_gen_inline_int_expr(gen, expr->data.unary.operand, meta, args);
            if (expr->data.unary.op == UNARY_MINUS) {
                asm_emit(gen, "  negw a0, a0\n");
            } else if (expr->data.unary.op == UNARY_NOT) {
                asm_emit(gen, "  seqz a0, a0\n");
            }
            return;
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            asm_gen_inline_int_expr(gen, expr->data.binary.lhs, meta, args);
            asm_push_a0(gen);
            asm_gen_inline_int_expr(gen, expr->data.binary.rhs, meta, args);
            asm_emit(gen, "  mv t1, a0\n");
            asm_pop_to(gen, "t0");
            switch (op) {
                case BIN_ADD: asm_emit(gen, "  addw a0, t0, t1\n"); return;
                case BIN_SUB: asm_emit(gen, "  subw a0, t0, t1\n"); return;
                case BIN_MUL: asm_emit(gen, "  mulw a0, t0, t1\n"); return;
                case BIN_DIV: asm_emit(gen, "  divw a0, t0, t1\n"); return;
                case BIN_MOD: asm_emit(gen, "  remw a0, t0, t1\n"); return;
                case BIN_LT: asm_emit(gen, "  slt a0, t0, t1\n"); return;
                case BIN_GT: asm_emit(gen, "  slt a0, t1, t0\n"); return;
                case BIN_LE:
                    asm_emit(gen, "  slt a0, t1, t0\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    return;
                case BIN_GE:
                    asm_emit(gen, "  slt a0, t0, t1\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    return;
                case BIN_EQ:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  seqz a0, a0\n");
                    return;
                case BIN_NE:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  snez a0, a0\n");
                    return;
                default:
                    asm_load_imm(gen, "a0", 0);
                    return;
            }
        }
        default:
            asm_load_imm(gen, "a0", 0);
            return;
    }
}

static bool asm_try_inline_call(AsmGen *gen, FunctionSymbol *meta, ExprList *args) {
    Expr *ret = asm_inline_return_expr(meta);
    if (ret == NULL || args->count != meta->params.count) {
        return false;
    }
    for (int i = 0; i < args->count; ++i) {
        if (!asm_arg_is_side_effect_free(args->items[i])) {
            return false;
        }
    }
    asm_gen_inline_int_expr(gen, ret, meta, args);
    return true;
}

static void asm_emit_compare_const_rhs(AsmGen *gen, BinaryOp op, int rhs) {
    switch (op) {
        case BIN_LT:
            if (asm_fits_imm12(rhs)) {
                asm_emit(gen, "  slti a0, a0, %d\n", rhs);
            } else {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  slt a0, a0, t1\n");
            }
            return;
        case BIN_GT:
            asm_load_imm(gen, "t1", rhs);
            asm_emit(gen, "  slt a0, t1, a0\n");
            return;
        case BIN_LE:
            asm_load_imm(gen, "t1", rhs);
            asm_emit(gen, "  slt a0, t1, a0\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return;
        case BIN_GE:
            if (asm_fits_imm12(rhs)) {
                asm_emit(gen, "  slti a0, a0, %d\n", rhs);
            } else {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  slt a0, a0, t1\n");
            }
            asm_emit(gen, "  xori a0, a0, 1\n");
            return;
        case BIN_EQ:
            if (rhs == 0) {
                asm_emit(gen, "  seqz a0, a0\n");
            } else {
                asm_emit_subw_imm(gen, "a0", "a0", rhs);
                asm_emit(gen, "  seqz a0, a0\n");
            }
            return;
        case BIN_NE:
            if (rhs == 0) {
                asm_emit(gen, "  snez a0, a0\n");
            } else {
                asm_emit_subw_imm(gen, "a0", "a0", rhs);
                asm_emit(gen, "  snez a0, a0\n");
            }
            return;
        default:
            return;
    }
}

static bool asm_try_gen_binary_const_rhs(AsmGen *gen, BinaryOp op, Expr *lhs, int rhs) {
    if ((op == BIN_DIV || op == BIN_MOD) && rhs == 0) {
        return false;
    }
    switch (op) {
        case BIN_ADD:
            asm_gen_expr(gen, lhs);
            asm_emit_addw_imm(gen, "a0", "a0", rhs);
            return true;
        case BIN_SUB:
            asm_gen_expr(gen, lhs);
            asm_emit_subw_imm(gen, "a0", "a0", rhs);
            return true;
        case BIN_MUL:
            asm_gen_expr(gen, lhs);
            if (!asm_emit_mul_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  mulw a0, a0, t1\n");
            }
            return true;
        case BIN_DIV:
            asm_gen_expr(gen, lhs);
            if (!asm_emit_div_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  divw a0, a0, t1\n");
            }
            return true;
        case BIN_MOD:
            asm_gen_expr(gen, lhs);
            if (!asm_emit_mod_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  remw a0, a0, t1\n");
            }
            return true;
        case BIN_LT:
        case BIN_GT:
        case BIN_LE:
        case BIN_GE:
        case BIN_EQ:
        case BIN_NE:
            asm_gen_expr(gen, lhs);
            asm_emit_compare_const_rhs(gen, op, rhs);
            return true;
        default:
            return false;
    }
}

static bool asm_try_gen_binary_const_lhs(AsmGen *gen, BinaryOp op, int lhs, Expr *rhs) {
    switch (op) {
        case BIN_ADD:
            return asm_try_gen_binary_const_rhs(gen, BIN_ADD, rhs, lhs);
        case BIN_MUL:
            return asm_try_gen_binary_const_rhs(gen, BIN_MUL, rhs, lhs);
        case BIN_SUB:
            asm_gen_expr(gen, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  subw a0, t0, a0\n");
            return true;
        case BIN_LT:
            asm_gen_expr(gen, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, t0, a0\n");
            return true;
        case BIN_GT:
            asm_gen_expr(gen, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, a0, t0\n");
            return true;
        case BIN_LE:
            asm_gen_expr(gen, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, a0, t0\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return true;
        case BIN_GE:
            asm_gen_expr(gen, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, t0, a0\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return true;
        case BIN_EQ:
        case BIN_NE:
            return asm_try_gen_binary_const_rhs(gen, op, rhs, lhs);
        default:
            return false;
    }
}

static void asm_gen_expr(AsmGen *gen, Expr *expr) {
    if (expr == NULL) {
        asm_load_imm(gen, "a0", 0);
        return;
    }
    int const_value = 0;
    if (asm_eval_int_const_expr(gen, expr, &const_value)) {
        asm_load_imm(gen, "a0", const_value);
        return;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_FLOAT_NUMBER:
            asm_load_imm(gen, "a0", expr->data.number);
            return;
        case EXPR_LVAL:
            asm_gen_lval_expr(gen, expr->data.lval);
            return;
        case EXPR_GETINT:
            asm_emit(gen, "  call getint\n");
            return;
        case EXPR_CALL: {
            const char *name = expr->data.call.name;
            if (strcmp(name, "starttime") == 0) {
                asm_load_imm(gen, "a0", 0);
                asm_emit(gen, "  call _sysy_starttime\n");
                asm_load_imm(gen, "a0", 0);
                return;
            }
            if (strcmp(name, "stoptime") == 0) {
                asm_load_imm(gen, "a0", 0);
                asm_emit(gen, "  call _sysy_stoptime\n");
                asm_load_imm(gen, "a0", 0);
                return;
            }
            if (strcmp(name, "getfloat") == 0) {
                asm_emit(gen, "  call getfloat\n");
                asm_emit(gen, "  fmv.x.w a0, fa0\n");
                return;
            }
            if (strcmp(name, "putfloat") == 0 && expr->data.call.args.count > 0) {
                asm_gen_expr(gen, expr->data.call.args.items[0]);
                asm_convert_a0(gen, asm_expr_type(gen, expr->data.call.args.items[0]), TYPE_FLOAT);
                asm_emit(gen, "  fmv.w.x fa0, a0\n");
                asm_emit(gen, "  call putfloat\n");
                asm_load_imm(gen, "a0", 0);
                return;
            }
            FunctionSymbol *meta = lookup_function_meta((IRGen *)gen, name);
            if (asm_try_inline_call(gen, meta, &expr->data.call.args)) {
                return;
            }
            asm_call_function(gen, asm_function_name(gen, name), &expr->data.call.args, meta);
            if (meta != NULL && meta->ret_type == TYPE_FLOAT) {
                asm_emit(gen, "  fmv.x.w a0, fa0\n");
            }
            return;
        }
        case EXPR_UNARY:
            if (expr->data.unary.op == UNARY_MINUS && asm_expr_type(gen, expr->data.unary.operand) == TYPE_FLOAT) {
                asm_gen_float_to_freg(gen, expr->data.unary.operand, "ft0");
                asm_emit(gen, "  fneg.s ft0, ft0\n");
                asm_emit(gen, "  fmv.x.w a0, ft0\n");
                return;
            }
            if (expr->data.unary.op == UNARY_NOT && asm_expr_type(gen, expr->data.unary.operand) == TYPE_FLOAT) {
                asm_float_truth_to_a0(gen, expr->data.unary.operand);
                asm_emit(gen, "  xori a0, a0, 1\n");
                return;
            }
            asm_gen_expr(gen, expr->data.unary.operand);
            if (expr->data.unary.op == UNARY_MINUS) {
                asm_emit(gen, "  negw a0, a0\n");
            } else if (expr->data.unary.op == UNARY_NOT) {
                asm_emit(gen, "  seqz a0, a0\n");
            }
            return;
        case EXPR_BINARY: {
            BinaryOp op = expr->data.binary.op;
            if (op == BIN_AND || op == BIN_OR) {
                asm_gen_short_circuit_expr(gen, expr);
                return;
            }
            if ((asm_expr_type(gen, expr->data.binary.lhs) == TYPE_FLOAT ||
                 asm_expr_type(gen, expr->data.binary.rhs) == TYPE_FLOAT) &&
                op != BIN_MOD) {
                asm_gen_float_binary(gen, op, expr->data.binary.lhs, expr->data.binary.rhs);
                return;
            }
            int rhs_const = 0;
            if (asm_eval_int_const_expr(gen, expr->data.binary.rhs, &rhs_const) &&
                asm_try_gen_binary_const_rhs(gen, op, expr->data.binary.lhs, rhs_const)) {
                return;
            }
            int lhs_const = 0;
            if (asm_eval_int_const_expr(gen, expr->data.binary.lhs, &lhs_const) &&
                asm_try_gen_binary_const_lhs(gen, op, lhs_const, expr->data.binary.rhs)) {
                return;
            }
            asm_gen_expr(gen, expr->data.binary.lhs);
            asm_push_a0(gen);
            asm_gen_expr(gen, expr->data.binary.rhs);
            asm_emit(gen, "  mv t1, a0\n");
            asm_pop_to(gen, "t0");
            switch (op) {
                case BIN_ADD: asm_emit(gen, "  addw a0, t0, t1\n"); return;
                case BIN_SUB: asm_emit(gen, "  subw a0, t0, t1\n"); return;
                case BIN_MUL: asm_emit(gen, "  mulw a0, t0, t1\n"); return;
                case BIN_DIV: asm_emit(gen, "  divw a0, t0, t1\n"); return;
                case BIN_MOD: asm_emit(gen, "  remw a0, t0, t1\n"); return;
                case BIN_LT: asm_emit(gen, "  slt a0, t0, t1\n"); return;
                case BIN_GT: asm_emit(gen, "  slt a0, t1, t0\n"); return;
                case BIN_LE:
                    asm_emit(gen, "  slt a0, t1, t0\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    return;
                case BIN_GE:
                    asm_emit(gen, "  slt a0, t0, t1\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    return;
                case BIN_EQ:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  seqz a0, a0\n");
                    return;
                case BIN_NE:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  snez a0, a0\n");
                    return;
                default:
                    asm_load_imm(gen, "a0", 0);
                    return;
            }
        }
    }
}

static void asm_gen_cond(AsmGen *gen, Expr *expr, const char *true_label, const char *false_label) {
    int const_value = 0;
    if (asm_eval_int_const_expr(gen, expr, &const_value)) {
        asm_emit(gen, "  j %s\n", const_value ? true_label : false_label);
        gen->current_block_terminated = true;
        return;
    }
    if (expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_OR) {
        char *mid = asm_new_label(gen, "lor_rhs");
        asm_gen_cond(gen, expr->data.binary.lhs, true_label, mid);
        asm_mark_label(gen, mid);
        asm_gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    if (expr->kind == EXPR_BINARY && expr->data.binary.op == BIN_AND) {
        char *mid = asm_new_label(gen, "land_rhs");
        asm_gen_cond(gen, expr->data.binary.lhs, mid, false_label);
        asm_mark_label(gen, mid);
        asm_gen_cond(gen, expr->data.binary.rhs, true_label, false_label);
        return;
    }
    if (asm_expr_type(gen, expr) == TYPE_FLOAT) {
        asm_float_truth_to_a0(gen, expr);
        asm_emit(gen, "  bnez a0, %s\n", true_label);
        asm_emit(gen, "  j %s\n", false_label);
        gen->current_block_terminated = true;
        return;
    }
    asm_gen_expr(gen, expr);
    asm_emit(gen, "  bnez a0, %s\n", true_label);
    asm_emit(gen, "  j %s\n", false_label);
    gen->current_block_terminated = true;
}

typedef enum {
    DAG_VALUE_CONST,
    DAG_VALUE_VAR,
    DAG_VALUE_UNARY,
    DAG_VALUE_BINARY
} AsmDagValueKind;

typedef struct {
    AsmDagValueKind kind;
    int value;
    Symbol *sym;
    UnaryOp unary_op;
    BinaryOp binary_op;
    int lhs;
    int rhs;
    int slot;
    int ref_count;
    bool emitted;
} AsmDagNode;

typedef struct {
    Symbol *sym;
    int node;
} AsmDagBinding;

typedef struct {
    AsmDagNode *nodes;
    int node_count;
    int node_capacity;
    AsmDagBinding *env;
    int env_count;
    int env_capacity;
    AsmDagBinding *targets;
    int target_count;
    int target_capacity;
    bool has_duplicate;
    bool has_overwrite;
    bool has_const_fold;
    bool has_const_propagation;
} AsmDagBlock;

static bool asm_dag_binary_commutative(BinaryOp op) {
    return op == BIN_ADD || op == BIN_MUL || op == BIN_EQ || op == BIN_NE;
}

static bool asm_dag_eval_binary(BinaryOp op, int lhs, int rhs, int *out) {
    if ((op == BIN_DIV || op == BIN_MOD) && rhs == 0) {
        return false;
    }
    switch (op) {
        case BIN_ADD: *out = lhs + rhs; return true;
        case BIN_SUB: *out = lhs - rhs; return true;
        case BIN_MUL: *out = lhs * rhs; return true;
        case BIN_DIV: *out = lhs / rhs; return true;
        case BIN_MOD: *out = lhs % rhs; return true;
        case BIN_LT: *out = lhs < rhs; return true;
        case BIN_GT: *out = lhs > rhs; return true;
        case BIN_LE: *out = lhs <= rhs; return true;
        case BIN_GE: *out = lhs >= rhs; return true;
        case BIN_EQ: *out = lhs == rhs; return true;
        case BIN_NE: *out = lhs != rhs; return true;
        case BIN_AND:
        case BIN_OR:
            return false;
    }
    return false;
}

static bool asm_dag_symbol_is_local_int_scalar(Symbol *sym) {
    return sym != NULL && !sym->is_global && !sym->info.is_param_array &&
           sym->value_type == TYPE_INT && sym->info.dim_count == 0;
}

static Symbol *asm_dag_lval_local_int_scalar(AsmGen *gen, LVal *lval) {
    if (lval == NULL || lval->indices.count != 0) {
        return NULL;
    }
    Symbol *sym = lookup_symbol((IRGen *)gen, lval->name);
    return asm_dag_symbol_is_local_int_scalar(sym) ? sym : NULL;
}

static bool asm_dag_expr_supported(AsmGen *gen, Expr *expr) {
    if (expr == NULL) {
        return true;
    }
    if (asm_expr_type(gen, expr) != TYPE_INT) {
        return false;
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return true;
        case EXPR_LVAL: {
            Symbol *sym = lookup_symbol((IRGen *)gen, expr->data.lval->name);
            if (sym != NULL && sym->is_const_scalar && expr->data.lval->indices.count == 0 &&
                sym->value_type == TYPE_INT) {
                return true;
            }
            return asm_dag_lval_local_int_scalar(gen, expr->data.lval) != NULL;
        }
        case EXPR_UNARY:
            return asm_dag_expr_supported(gen, expr->data.unary.operand);
        case EXPR_BINARY:
            if (expr->data.binary.op == BIN_AND || expr->data.binary.op == BIN_OR) {
                return false;
            }
            return asm_dag_expr_supported(gen, expr->data.binary.lhs) &&
                   asm_dag_expr_supported(gen, expr->data.binary.rhs);
        default:
            return false;
    }
}

static bool asm_dag_stmt_supported(AsmGen *gen, Stmt *stmt) {
    if (stmt == NULL || stmt->kind != STMT_ASSIGN) {
        return false;
    }
    if (asm_dag_lval_local_int_scalar(gen, stmt->data.assign_stmt.lval) == NULL) {
        return false;
    }
    return asm_dag_expr_supported(gen, stmt->data.assign_stmt.expr);
}

static int asm_dag_find_binding(AsmDagBinding *items, int count, Symbol *sym) {
    for (int i = 0; i < count; ++i) {
        if (items[i].sym == sym) {
            return i;
        }
    }
    return -1;
}

static void asm_dag_set_binding(AsmDagBinding **items, int *count, int *capacity,
                                Symbol *sym, int node, bool *overwritten) {
    int index = asm_dag_find_binding(*items, *count, sym);
    if (index >= 0) {
        (*items)[index].node = node;
        if (overwritten != NULL) {
            *overwritten = true;
        }
        return;
    }
    ensure_capacity((void **)items, capacity, sizeof(AsmDagBinding), *count + 1);
    (*items)[*count].sym = sym;
    (*items)[*count].node = node;
    ++(*count);
}

static void asm_dag_init(AsmDagBlock *dag) {
    memset(dag, 0, sizeof(*dag));
}

static void asm_dag_free(AsmDagBlock *dag) {
    free(dag->nodes);
    free(dag->env);
    free(dag->targets);
}

static int asm_dag_add_node(AsmDagBlock *dag, AsmDagNode node) {
    ensure_capacity((void **)&dag->nodes, &dag->node_capacity, sizeof(AsmDagNode), dag->node_count + 1);
    dag->nodes[dag->node_count] = node;
    return dag->node_count++;
}

static int asm_dag_node_const(AsmDagBlock *dag, int value) {
    for (int i = 0; i < dag->node_count; ++i) {
        if (dag->nodes[i].kind == DAG_VALUE_CONST && dag->nodes[i].value == value) {
            return i;
        }
    }
    AsmDagNode node;
    memset(&node, 0, sizeof(node));
    node.kind = DAG_VALUE_CONST;
    node.value = value;
    node.slot = INT_MIN;
    return asm_dag_add_node(dag, node);
}

static int asm_dag_node_var(AsmDagBlock *dag, Symbol *sym) {
    for (int i = 0; i < dag->node_count; ++i) {
        if (dag->nodes[i].kind == DAG_VALUE_VAR && dag->nodes[i].sym == sym) {
            return i;
        }
    }
    AsmDagNode node;
    memset(&node, 0, sizeof(node));
    node.kind = DAG_VALUE_VAR;
    node.sym = sym;
    node.slot = INT_MIN;
    return asm_dag_add_node(dag, node);
}

static int asm_dag_node_unary(AsmDagBlock *dag, UnaryOp op, int child) {
    AsmDagNode *c = &dag->nodes[child];
    if (op == UNARY_PLUS) {
        if (c->kind == DAG_VALUE_CONST) {
            dag->has_const_fold = true;
        }
        return child;
    }
    if (c->kind == DAG_VALUE_CONST) {
        dag->has_const_fold = true;
        return asm_dag_node_const(dag, op == UNARY_MINUS ? -c->value : !c->value);
    }
    for (int i = 0; i < dag->node_count; ++i) {
        if (dag->nodes[i].kind == DAG_VALUE_UNARY &&
            dag->nodes[i].unary_op == op && dag->nodes[i].lhs == child) {
            dag->has_duplicate = true;
            return i;
        }
    }
    AsmDagNode node;
    memset(&node, 0, sizeof(node));
    node.kind = DAG_VALUE_UNARY;
    node.unary_op = op;
    node.lhs = child;
    node.slot = INT_MIN;
    return asm_dag_add_node(dag, node);
}

static int asm_dag_node_binary(AsmDagBlock *dag, BinaryOp op, int lhs, int rhs) {
    AsmDagNode *l = &dag->nodes[lhs];
    AsmDagNode *r = &dag->nodes[rhs];
    int folded = 0;
    if (l->kind == DAG_VALUE_CONST && r->kind == DAG_VALUE_CONST &&
        asm_dag_eval_binary(op, l->value, r->value, &folded)) {
        dag->has_const_fold = true;
        return asm_dag_node_const(dag, folded);
    }
    if (asm_dag_binary_commutative(op) && rhs < lhs) {
        int tmp = lhs;
        lhs = rhs;
        rhs = tmp;
        l = &dag->nodes[lhs];
        r = &dag->nodes[rhs];
    }
    if (op == BIN_ADD && r->kind == DAG_VALUE_CONST && r->value == 0) {
        dag->has_const_fold = true;
        return lhs;
    }
    if (op == BIN_ADD && l->kind == DAG_VALUE_CONST && l->value == 0) {
        dag->has_const_fold = true;
        return rhs;
    }
    if (op == BIN_SUB && r->kind == DAG_VALUE_CONST && r->value == 0) {
        dag->has_const_fold = true;
        return lhs;
    }
    if (op == BIN_MUL && r->kind == DAG_VALUE_CONST && r->value == 1) {
        dag->has_const_fold = true;
        return lhs;
    }
    if (op == BIN_MUL && l->kind == DAG_VALUE_CONST && l->value == 1) {
        dag->has_const_fold = true;
        return rhs;
    }
    if (op == BIN_MUL &&
        ((r->kind == DAG_VALUE_CONST && r->value == 0) ||
         (l->kind == DAG_VALUE_CONST && l->value == 0))) {
        dag->has_const_fold = true;
        return asm_dag_node_const(dag, 0);
    }
    if (op == BIN_DIV && r->kind == DAG_VALUE_CONST && r->value == 1) {
        dag->has_const_fold = true;
        return lhs;
    }
    if (op == BIN_MOD && r->kind == DAG_VALUE_CONST && (r->value == 1 || r->value == -1)) {
        dag->has_const_fold = true;
        return asm_dag_node_const(dag, 0);
    }
    if (lhs == rhs) {
        if (op == BIN_EQ || op == BIN_LE || op == BIN_GE) {
            dag->has_const_fold = true;
            return asm_dag_node_const(dag, 1);
        }
        if (op == BIN_NE || op == BIN_LT || op == BIN_GT || op == BIN_SUB) {
            dag->has_const_fold = true;
            return asm_dag_node_const(dag, 0);
        }
    }
    for (int i = 0; i < dag->node_count; ++i) {
        if (dag->nodes[i].kind == DAG_VALUE_BINARY &&
            dag->nodes[i].binary_op == op && dag->nodes[i].lhs == lhs && dag->nodes[i].rhs == rhs) {
            dag->has_duplicate = true;
            return i;
        }
    }
    AsmDagNode node;
    memset(&node, 0, sizeof(node));
    node.kind = DAG_VALUE_BINARY;
    node.binary_op = op;
    node.lhs = lhs;
    node.rhs = rhs;
    node.slot = INT_MIN;
    return asm_dag_add_node(dag, node);
}

static int asm_dag_build_expr(AsmGen *gen, AsmDagBlock *dag, Expr *expr) {
    if (expr == NULL) {
        return asm_dag_node_const(dag, 0);
    }
    switch (expr->kind) {
        case EXPR_NUMBER:
            return asm_dag_node_const(dag, expr->data.number);
        case EXPR_LVAL: {
            Symbol *sym = lookup_symbol((IRGen *)gen, expr->data.lval->name);
            if (sym != NULL && sym->is_const_scalar && expr->data.lval->indices.count == 0 &&
                sym->value_type == TYPE_INT) {
                return asm_dag_node_const(dag, sym->const_scalar);
            }
            int index = asm_dag_find_binding(dag->env, dag->env_count, sym);
            if (index >= 0) {
                if (dag->nodes[dag->env[index].node].kind == DAG_VALUE_CONST) {
                    dag->has_const_propagation = true;
                }
                return dag->env[index].node;
            }
            return asm_dag_node_var(dag, sym);
        }
        case EXPR_UNARY:
            return asm_dag_node_unary(dag, expr->data.unary.op,
                                      asm_dag_build_expr(gen, dag, expr->data.unary.operand));
        case EXPR_BINARY:
            return asm_dag_node_binary(dag, expr->data.binary.op,
                                       asm_dag_build_expr(gen, dag, expr->data.binary.lhs),
                                       asm_dag_build_expr(gen, dag, expr->data.binary.rhs));
        default:
            return asm_dag_node_const(dag, 0);
    }
}

static void asm_dag_add_stmt(AsmGen *gen, AsmDagBlock *dag, Stmt *stmt) {
    Symbol *target = asm_dag_lval_local_int_scalar(gen, stmt->data.assign_stmt.lval);
    int node = asm_dag_build_expr(gen, dag, stmt->data.assign_stmt.expr);
    asm_dag_set_binding(&dag->env, &dag->env_count, &dag->env_capacity,
                        target, node, NULL);
    asm_dag_set_binding(&dag->targets, &dag->target_count, &dag->target_capacity,
                        target, node, &dag->has_overwrite);
}

static void asm_dag_count_node_refs(AsmDagBlock *dag) {
    for (int i = 0; i < dag->node_count; ++i) {
        dag->nodes[i].ref_count = 0;
    }
    for (int i = 0; i < dag->target_count; ++i) {
        dag->nodes[dag->targets[i].node].ref_count++;
    }
    for (int i = 0; i < dag->node_count; ++i) {
        AsmDagNode *node = &dag->nodes[i];
        if (node->kind == DAG_VALUE_UNARY) {
            dag->nodes[node->lhs].ref_count++;
        } else if (node->kind == DAG_VALUE_BINARY) {
            dag->nodes[node->lhs].ref_count++;
            dag->nodes[node->rhs].ref_count++;
        }
    }
}

static bool asm_dag_node_depends_on_var(AsmDagBlock *dag, int id, Symbol *sym) {
    AsmDagNode *node = &dag->nodes[id];
    switch (node->kind) {
        case DAG_VALUE_VAR:
            return node->sym == sym;
        case DAG_VALUE_UNARY:
            return asm_dag_node_depends_on_var(dag, node->lhs, sym);
        case DAG_VALUE_BINARY:
            return asm_dag_node_depends_on_var(dag, node->lhs, sym) ||
                   asm_dag_node_depends_on_var(dag, node->rhs, sym);
        default:
            return false;
    }
}

static bool asm_dag_can_store_direct(AsmDagBlock *dag) {
    if (dag->has_overwrite) {
        return false;
    }
    for (int i = 0; i < dag->target_count; ++i) {
        Symbol *target = dag->targets[i].sym;
        for (int j = i + 1; j < dag->target_count; ++j) {
            if (asm_dag_node_depends_on_var(dag, dag->targets[j].node, target)) {
                return false;
            }
        }
    }
    return true;
}

static void asm_dag_emit_node(AsmGen *gen, AsmDagBlock *dag, int id);

static bool asm_dag_emit_binary_const_rhs(AsmGen *gen, AsmDagBlock *dag,
                                          BinaryOp op, int lhs, int rhs) {
    if ((op == BIN_DIV || op == BIN_MOD) && rhs == 0) {
        return false;
    }
    switch (op) {
        case BIN_ADD:
            asm_dag_emit_node(gen, dag, lhs);
            asm_emit_addw_imm(gen, "a0", "a0", rhs);
            return true;
        case BIN_SUB:
            asm_dag_emit_node(gen, dag, lhs);
            asm_emit_subw_imm(gen, "a0", "a0", rhs);
            return true;
        case BIN_MUL:
            asm_dag_emit_node(gen, dag, lhs);
            if (!asm_emit_mul_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  mulw a0, a0, t1\n");
            }
            return true;
        case BIN_DIV:
            asm_dag_emit_node(gen, dag, lhs);
            if (!asm_emit_div_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  divw a0, a0, t1\n");
            }
            return true;
        case BIN_MOD:
            asm_dag_emit_node(gen, dag, lhs);
            if (!asm_emit_mod_const(gen, "a0", "a0", rhs)) {
                asm_load_imm(gen, "t1", rhs);
                asm_emit(gen, "  remw a0, a0, t1\n");
            }
            return true;
        case BIN_LT:
        case BIN_GT:
        case BIN_LE:
        case BIN_GE:
        case BIN_EQ:
        case BIN_NE:
            asm_dag_emit_node(gen, dag, lhs);
            asm_emit_compare_const_rhs(gen, op, rhs);
            return true;
        default:
            return false;
    }
}

static bool asm_dag_emit_binary_const_lhs(AsmGen *gen, AsmDagBlock *dag,
                                          BinaryOp op, int lhs, int rhs) {
    switch (op) {
        case BIN_ADD:
            return asm_dag_emit_binary_const_rhs(gen, dag, BIN_ADD, rhs, lhs);
        case BIN_MUL:
            return asm_dag_emit_binary_const_rhs(gen, dag, BIN_MUL, rhs, lhs);
        case BIN_SUB:
            asm_dag_emit_node(gen, dag, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  subw a0, t0, a0\n");
            return true;
        case BIN_LT:
            asm_dag_emit_node(gen, dag, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, t0, a0\n");
            return true;
        case BIN_GT:
            asm_dag_emit_node(gen, dag, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, a0, t0\n");
            return true;
        case BIN_LE:
            asm_dag_emit_node(gen, dag, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, a0, t0\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return true;
        case BIN_GE:
            asm_dag_emit_node(gen, dag, rhs);
            asm_load_imm(gen, "t0", lhs);
            asm_emit(gen, "  slt a0, t0, a0\n");
            asm_emit(gen, "  xori a0, a0, 1\n");
            return true;
        case BIN_EQ:
        case BIN_NE:
            return asm_dag_emit_binary_const_rhs(gen, dag, op, rhs, lhs);
        default:
            return false;
    }
}

static void asm_dag_emit_node(AsmGen *gen, AsmDagBlock *dag, int id) {
    AsmDagNode *node = &dag->nodes[id];
    if ((node->kind == DAG_VALUE_UNARY || node->kind == DAG_VALUE_BINARY) && node->emitted) {
        asm_emit_load(gen, "a0", node->slot, "s0");
        return;
    }
    switch (node->kind) {
        case DAG_VALUE_CONST:
            asm_load_imm(gen, "a0", node->value);
            return;
        case DAG_VALUE_VAR:
            asm_emit_load(gen, "a0", node->sym->stack_offset, "s0");
            return;
        case DAG_VALUE_UNARY:
            asm_dag_emit_node(gen, dag, node->lhs);
            if (node->unary_op == UNARY_MINUS) {
                asm_emit(gen, "  negw a0, a0\n");
            } else if (node->unary_op == UNARY_NOT) {
                asm_emit(gen, "  seqz a0, a0\n");
            }
            break;
        case DAG_VALUE_BINARY:
            if (dag->nodes[node->rhs].kind == DAG_VALUE_CONST &&
                asm_dag_emit_binary_const_rhs(gen, dag, node->binary_op, node->lhs,
                                              dag->nodes[node->rhs].value)) {
                break;
            }
            if (dag->nodes[node->lhs].kind == DAG_VALUE_CONST &&
                asm_dag_emit_binary_const_lhs(gen, dag, node->binary_op,
                                              dag->nodes[node->lhs].value, node->rhs)) {
                break;
            }
            asm_dag_emit_node(gen, dag, node->lhs);
            asm_push_a0(gen);
            asm_dag_emit_node(gen, dag, node->rhs);
            asm_emit(gen, "  mv t1, a0\n");
            asm_pop_to(gen, "t0");
            switch (node->binary_op) {
                case BIN_ADD: asm_emit(gen, "  addw a0, t0, t1\n"); break;
                case BIN_SUB: asm_emit(gen, "  subw a0, t0, t1\n"); break;
                case BIN_MUL: asm_emit(gen, "  mulw a0, t0, t1\n"); break;
                case BIN_DIV: asm_emit(gen, "  divw a0, t0, t1\n"); break;
                case BIN_MOD: asm_emit(gen, "  remw a0, t0, t1\n"); break;
                case BIN_LT: asm_emit(gen, "  slt a0, t0, t1\n"); break;
                case BIN_GT: asm_emit(gen, "  slt a0, t1, t0\n"); break;
                case BIN_LE:
                    asm_emit(gen, "  slt a0, t1, t0\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    break;
                case BIN_GE:
                    asm_emit(gen, "  slt a0, t0, t1\n");
                    asm_emit(gen, "  xori a0, a0, 1\n");
                    break;
                case BIN_EQ:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  seqz a0, a0\n");
                    break;
                case BIN_NE:
                    asm_emit(gen, "  subw a0, t0, t1\n");
                    asm_emit(gen, "  snez a0, a0\n");
                    break;
                default:
                    asm_load_imm(gen, "a0", 0);
                    break;
            }
            break;
    }
    if (node->ref_count > 1) {
        if (node->slot == INT_MIN) {
            node->slot = asm_alloc_stack(gen, 4);
        }
        asm_emit_store(gen, "a0", node->slot, "s0");
        node->emitted = true;
    }
}

static bool asm_try_gen_dag_block(AsmGen *gen, Stmt **stmts, int count) {
    (void)gen;
    (void)stmts;
    (void)count;
    return false;
#if 0
    if (count < 2) {
        return false;
    }
    AsmDagBlock dag;
    asm_dag_init(&dag);
    for (int i = 0; i < count; ++i) {
        asm_dag_add_stmt(gen, &dag, stmts[i]);
    }
    if (!dag.has_duplicate && !dag.has_const_fold && !dag.has_const_propagation) {
        asm_dag_free(&dag);
        return false;
    }
    asm_dag_count_node_refs(&dag);
    if (asm_dag_can_store_direct(&dag)) {
        for (int i = 0; i < dag.target_count; ++i) {
            asm_dag_emit_node(gen, &dag, dag.targets[i].node);
            asm_emit_store(gen, "a0", dag.targets[i].sym->stack_offset, "s0");
        }
        asm_dag_free(&dag);
        return true;
    }
    int *result_slots = (int *)xmalloc(sizeof(int) * (size_t)dag.target_count);
    for (int i = 0; i < dag.target_count; ++i) {
        asm_dag_emit_node(gen, &dag, dag.targets[i].node);
        result_slots[i] = asm_alloc_stack(gen, 4);
        asm_emit_store(gen, "a0", result_slots[i], "s0");
    }
    for (int i = 0; i < dag.target_count; ++i) {
        asm_emit_load(gen, "a0", result_slots[i], "s0");
        asm_emit_store(gen, "a0", dag.targets[i].sym->stack_offset, "s0");
    }
    free(result_slots);
    asm_dag_free(&dag);
    return true;
#endif
}

static void asm_gen_decl(AsmGen *gen, Decl *decl, bool is_global);
static void asm_gen_stmt(AsmGen *gen, Stmt *stmt);

static void asm_gen_block(AsmGen *gen, Block *block, bool new_scope) {
    if (new_scope) {
        push_scope((IRGen *)gen);
    }
    for (int i = 0; i < block->items.count;) {
        if (block->items.kinds[i] == BLOCK_ITEM_DECL) {
            asm_gen_decl(gen, (Decl *)block->items.items[i], false);
            ++i;
        } else {
            Stmt *stmt = (Stmt *)block->items.items[i];
            if (asm_dag_stmt_supported(gen, stmt)) {
                int start = i;
                while (i < block->items.count && block->items.kinds[i] == BLOCK_ITEM_STMT &&
                       asm_dag_stmt_supported(gen, (Stmt *)block->items.items[i])) {
                    ++i;
                }
                int count = i - start;
                Stmt **stmts = (Stmt **)xmalloc(sizeof(Stmt *) * (size_t)count);
                for (int j = 0; j < count; ++j) {
                    stmts[j] = (Stmt *)block->items.items[start + j];
                }
                if (asm_try_gen_dag_block(gen, stmts, count)) {
                    free(stmts);
                    continue;
                }
                free(stmts);
                for (int j = start; j < i; ++j) {
                    asm_gen_stmt(gen, (Stmt *)block->items.items[j]);
                }
                continue;
            }
            asm_gen_stmt(gen, stmt);
            ++i;
        }
    }
    if (new_scope) {
        pop_scope((IRGen *)gen);
    }
}

static void asm_gen_printf(AsmGen *gen, char *format, ExprList *args) {
    int arg_index = 0;
    int len = (int)strlen(format);
    for (int i = 1; i < len - 1; ++i) {
        if (format[i] == '%' && i + 1 < len - 1) {
            char spec = format[++i];
            if (spec == 'd') {
                Expr *arg = args->items[arg_index++];
                asm_gen_expr(gen, arg);
                asm_convert_a0(gen, asm_expr_type(gen, arg), TYPE_INT);
                asm_emit(gen, "  call putint\n");
            } else if (spec == 'c') {
                Expr *arg = args->items[arg_index++];
                asm_gen_expr(gen, arg);
                asm_convert_a0(gen, asm_expr_type(gen, arg), TYPE_INT);
                asm_emit(gen, "  call putch\n");
            } else if (spec == 'a' || spec == 'f') {
                Expr *arg = args->items[arg_index++];
                asm_gen_expr(gen, arg);
                asm_convert_a0(gen, asm_expr_type(gen, arg), TYPE_FLOAT);
                asm_emit(gen, "  fmv.w.x fa0, a0\n");
                asm_emit(gen, "  call putfloat\n");
            } else if (spec == '%') {
                asm_load_imm(gen, "a0", '%');
                asm_emit(gen, "  call putch\n");
            } else {
                asm_load_imm(gen, "a0", '%');
                asm_emit(gen, "  call putch\n");
                asm_load_imm(gen, "a0", (unsigned char)spec);
                asm_emit(gen, "  call putch\n");
            }
        } else if (format[i] == '\\' && i + 1 < len - 1) {
            int ch = format[i + 1] == 'n' ? 10 : (format[i + 1] == 't' ? 9 : format[i + 1]);
            asm_load_imm(gen, "a0", ch);
            asm_emit(gen, "  call putch\n");
            ++i;
        } else {
            asm_load_imm(gen, "a0", (unsigned char)format[i]);
            asm_emit(gen, "  call putch\n");
        }
    }
}

static void asm_gen_stmt(AsmGen *gen, Stmt *stmt) {
    switch (stmt->kind) {
        case STMT_ASSIGN: {
            Symbol *scalar = asm_scalar_lval_target(gen, stmt->data.assign_stmt.lval);
            if (scalar != NULL) {
                asm_gen_expr(gen, stmt->data.assign_stmt.expr);
                asm_convert_a0(gen, asm_expr_type(gen, stmt->data.assign_stmt.expr),
                               scalar->value_type);
                asm_store_scalar_reg(gen, scalar, "a0");
                return;
            }
            asm_lval_address(gen, stmt->data.assign_stmt.lval);
            asm_push_a0(gen);
            asm_gen_expr(gen, stmt->data.assign_stmt.expr);
            asm_convert_a0(gen, asm_expr_type(gen, stmt->data.assign_stmt.expr),
                           asm_lval_type(gen, stmt->data.assign_stmt.lval));
            asm_emit(gen, "  mv t0, a0\n");
            asm_pop_to(gen, "t1");
            asm_emit_store(gen, "t0", 0, "t1");
            return;
        }
        case STMT_EXPR:
            asm_gen_expr(gen, stmt->data.expr_stmt);
            return;
        case STMT_BLOCK:
            asm_gen_block(gen, stmt->data.block_stmt, true);
            return;
        case STMT_IF: {
            char *then_label = asm_new_label(gen, "if_then");
            char *else_label = asm_new_label(gen, "if_else");
            char *end_label = asm_new_label(gen, "if_end");
            asm_gen_cond(gen, stmt->data.if_stmt.cond, then_label,
                         stmt->data.if_stmt.else_stmt ? else_label : end_label);
            asm_mark_label(gen, then_label);
            asm_gen_stmt(gen, stmt->data.if_stmt.then_stmt);
            if (!gen->current_block_terminated) {
                asm_emit(gen, "  j %s\n", end_label);
                gen->current_block_terminated = true;
            }
            if (stmt->data.if_stmt.else_stmt != NULL) {
                asm_mark_label(gen, else_label);
                asm_gen_stmt(gen, stmt->data.if_stmt.else_stmt);
                if (!gen->current_block_terminated) {
                    asm_emit(gen, "  j %s\n", end_label);
                    gen->current_block_terminated = true;
                }
            }
            asm_mark_label(gen, end_label);
            return;
        }
        case STMT_WHILE: {
            char *cond_label = asm_new_label(gen, "while_cond");
            char *body_label = asm_new_label(gen, "while_body");
            char *end_label = asm_new_label(gen, "while_end");
            asm_mark_label(gen, cond_label);
            string_list_push(&gen->break_labels, end_label);
            string_list_push(&gen->continue_labels, cond_label);
            asm_gen_cond(gen, stmt->data.while_stmt.cond, body_label, end_label);
            asm_mark_label(gen, body_label);
            asm_gen_stmt(gen, stmt->data.while_stmt.body);
            if (!gen->current_block_terminated) {
                asm_emit(gen, "  j %s\n", cond_label);
                gen->current_block_terminated = true;
            }
            gen->break_labels.count--;
            gen->continue_labels.count--;
            asm_mark_label(gen, end_label);
            return;
        }
        case STMT_BREAK:
            asm_emit(gen, "  j %s\n", gen->break_labels.items[gen->break_labels.count - 1]);
            gen->current_block_terminated = true;
            return;
        case STMT_CONTINUE:
            asm_emit(gen, "  j %s\n", gen->continue_labels.items[gen->continue_labels.count - 1]);
            gen->current_block_terminated = true;
            return;
        case STMT_RETURN:
            if (stmt->data.return_expr == NULL) {
                asm_load_imm(gen, "a0", 0);
            } else {
                asm_gen_expr(gen, stmt->data.return_expr);
                asm_convert_a0(gen, asm_expr_type(gen, stmt->data.return_expr), gen->current_ret_type);
            }
            if (gen->current_ret_type == TYPE_FLOAT) {
                asm_emit(gen, "  fmv.w.x fa0, a0\n");
            }
            asm_emit(gen, "  j %s\n", gen->return_label);
            gen->current_block_terminated = true;
            return;
        case STMT_PRINTF:
            asm_gen_printf(gen, stmt->data.printf_stmt.format, &stmt->data.printf_stmt.args);
            return;
    }
}

static void asm_zero_local(AsmGen *gen, Symbol *sym) {
    if (sym->info.total_slots <= 8) {
        for (int i = 0; i < sym->info.total_slots; ++i) {
            asm_emit_store(gen, "zero", sym->stack_offset + i * 4, "s0");
        }
        return;
    }
    char *loop_label = asm_new_label(gen, "zero_loop");
    char *end_label = asm_new_label(gen, "zero_end");
    asm_emit_add_imm(gen, "t0", "s0", sym->stack_offset);
    asm_emit(gen, "  li t1, %d\n", sym->info.total_slots);
    asm_mark_label(gen, loop_label);
    asm_emit(gen, "  beqz t1, %s\n", end_label);
    asm_emit(gen, "  sw zero, 0(t0)\n");
    asm_emit(gen, "  addi t0, t0, 4\n");
    asm_emit(gen, "  addi t1, t1, -1\n");
    asm_emit(gen, "  j %s\n", loop_label);
    gen->current_block_terminated = true;
    asm_mark_label(gen, end_label);
}

static void asm_emit_global_flat(StrBuf *data, const int *flat, int total) {
    int i = 0;
    while (i < total) {
        if (flat[i] == 0) {
            int start = i;
            while (i < total && flat[i] == 0) {
                ++i;
            }
            sb_appendf(data, "  .zero %d\n", (i - start) * 4);
            continue;
        }
        sb_appendf(data, "  .word %d\n", flat[i]);
        ++i;
    }
}

static void asm_gen_decl(AsmGen *gen, Decl *decl, bool is_global) {
    for (int i = 0; i < decl->items.count; ++i) {
        DeclItem *item = decl->items.items[i];
        Symbol *sym = scope_add_symbol((IRGen *)gen, item->name);
        sym->is_global = is_global;
        sym->value_type = decl->type;
        sym->info.dim_count = item->dims.count;
        sym->info.dims = copy_dims(item->dims.data, item->dims.count);
        sym->info.total_slots = object_slot_count(item->dims.data, item->dims.count);
        sym->info.is_const = decl->is_const;
        sym->info.is_param_array = false;
        sym->info.is_flat_storage = item->dims.count > 0;
        if (item->dims.count == 0) {
            if (decl->is_const && item->init != NULL) {
                sym->is_const_scalar = true;
                sym->const_scalar = decl->type == TYPE_FLOAT
                                        ? eval_const_float_bits((IRGen *)gen, item->init->expr)
                                        : eval_const_expr((IRGen *)gen, item->init->expr);
            }
            if (is_global) {
                int init_val = 0;
                if (item->init != NULL) {
                    init_val = decl->type == TYPE_FLOAT
                                   ? eval_const_float_bits((IRGen *)gen, item->init->expr)
                                   : eval_const_expr((IRGen *)gen, item->init->expr);
                }
                sym->llvm_name = str_printf("g%d", gen->global_id++);
                sb_appendf(&gen->data, "  .globl %s\n  .align 2\n%s:\n  .word %d\n",
                           sym->llvm_name, sym->llvm_name, init_val);
            } else {
                sym->stack_offset = asm_alloc_stack(gen, 4);
                if (item->init != NULL) {
                    asm_gen_expr(gen, item->init->expr);
                    asm_convert_a0(gen, asm_expr_type(gen, item->init->expr), decl->type);
                    asm_emit_store(gen, "a0", sym->stack_offset, "s0");
                } else {
                    asm_emit_store(gen, "zero", sym->stack_offset, "s0");
                }
            }
        } else {
            if (decl->is_const && item->init != NULL) {
                sym->const_flat = const_init_to_flat_typed((IRGen *)gen, item->init, item->dims.data, item->dims.count, decl->type);
            }
            if (is_global) {
                sym->llvm_name = str_printf("g%d", gen->global_id++);
                int *flat = item->init != NULL
                                ? const_init_to_flat_typed((IRGen *)gen, item->init, item->dims.data, item->dims.count, decl->type)
                                : NULL;
                if (flat == NULL) {
                    sb_appendf(&gen->bss, "  .globl %s\n  .align 2\n%s:\n  .zero %d\n",
                               sym->llvm_name, sym->llvm_name, sym->info.total_slots * 4);
                } else {
                    sb_appendf(&gen->data, "  .globl %s\n  .align 2\n%s:\n",
                               sym->llvm_name, sym->llvm_name);
                    asm_emit_global_flat(&gen->data, flat, sym->info.total_slots);
                }
            } else {
                sym->stack_offset = asm_alloc_stack(gen, sym->info.total_slots * 4);
                asm_zero_local(gen, sym);
                if (item->init != NULL) {
                    Expr **slots = init_to_expr_slots(item->init, item->dims.data, item->dims.count, sym->info.total_slots);
                    for (int j = 0; j < sym->info.total_slots; ++j) {
                        if (slots[j] != NULL) {
                            asm_gen_expr(gen, slots[j]);
                            asm_convert_a0(gen, asm_expr_type(gen, slots[j]), decl->type);
                            asm_emit_store(gen, "a0", sym->stack_offset + j * 4, "s0");
                        }
                    }
                    free(slots);
                }
            }
        }
    }
}

static void asm_gen_function(AsmGen *gen, FuncDef *func) {
    StrBuf saved_body = gen->body;
    sb_init(&gen->body);
    gen->next_stack_offset = -24;
    gen->return_label = asm_new_label(gen, "return");
    gen->current_ret_type = func->ret_type;
    gen->current_block_terminated = false;
    push_scope((IRGen *)gen);

    int int_reg = 0;
    int float_reg = 0;
    int stack_arg = 0;
    for (int i = 0; i < func->params.count; ++i) {
        Param *param = func->params.items[i];
        Symbol *sym = scope_add_symbol((IRGen *)gen, param->name);
        sym->info.is_const = false;
        sym->value_type = param->type;
        sym->info.dim_count = param->dims.count;
        sym->info.dims = copy_dims(param->dims.data, param->dims.count);
        sym->info.total_slots = param->is_array ? 1 : 1;
        sym->info.is_param_array = param->is_array;
        sym->stack_offset = asm_alloc_stack(gen, 8);
        asm_store_incoming_param(gen, param, sym, &int_reg, &float_reg, &stack_arg);
    }

    asm_gen_block(gen, func->block, false);
    if (!gen->current_block_terminated) {
        asm_load_imm(gen, "a0", 0);
        if (gen->current_ret_type == TYPE_FLOAT) {
            asm_emit(gen, "  fmv.w.x fa0, a0\n");
        }
        asm_emit(gen, "  j %s\n", gen->return_label);
    }
    asm_mark_label(gen, gen->return_label);
    gen->frame_size = align_to(-gen->next_stack_offset + 16, 16);

    const char *name = asm_function_name(gen, func->name);
    sb_appendf(&gen->text, "  .globl %s\n  .align 2\n%s:\n", name, name);
    sb_emit_add_imm(&gen->text, "sp", "sp", -gen->frame_size);
    sb_emit_mem(&gen->text, "sd", "ra", gen->frame_size - 8, "sp");
    sb_emit_mem(&gen->text, "sd", "s0", gen->frame_size - 16, "sp");
    sb_emit_add_imm(&gen->text, "s0", "sp", gen->frame_size);
    sb_append(&gen->text, gen->body.data ? gen->body.data : "");
    sb_emit_mem(&gen->text, "ld", "ra", gen->frame_size - 8, "sp");
    sb_emit_mem(&gen->text, "ld", "s0", gen->frame_size - 16, "sp");
    sb_emit_add_imm(&gen->text, "sp", "sp", gen->frame_size);
    sb_append(&gen->text, "  ret\n\n");

    pop_scope((IRGen *)gen);
    gen->body = saved_body;
}

static void generate_program_asm(Program *program, FILE *out) {
    AsmGen gen;
    memset(&gen, 0, sizeof(gen));
    sb_init(&gen.data);
    sb_init(&gen.bss);
    sb_init(&gen.text);
    sb_init(&gen.body);
    memset(g_function_buckets, 0, sizeof(g_function_buckets));
    push_scope((IRGen *)&gen);
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            asm_add_function_meta(&gen, item->data.func);
        }
    }
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_DECL) {
            asm_gen_decl(&gen, item->data.decl, true);
        }
    }
    for (int i = 0; i < program->items.count; ++i) {
        TopLevelItem *item = program->items.items[i];
        if (item->kind == TOP_LEVEL_FUNC) {
            asm_gen_function(&gen, item->data.func);
        }
    }
    fputs("  .attribute arch, \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0\"\n", out);
    fputs("  .option nopic\n", out);
    fputs("  .option norelax\n", out);
    if (gen.data.len > 0) {
        fputs("  .data\n", out);
        fputs(gen.data.data, out);
    }
    if (gen.bss.len > 0) {
        fputs("  .bss\n", out);
        fputs(gen.bss.data, out);
    }
    fputs("  .text\n", out);
    fputs(gen.text.data ? gen.text.data : "", out);
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
    FILE *out = fopen(output_path, "w");
    if (out == NULL) {
        fclose(yyin);
        return 1;
    }
    generate_program_asm(g_program, out);
    fclose(out);
    fclose(yyin);
    return 0;
}
