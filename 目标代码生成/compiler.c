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

static bool mem_can_inline_call(MemIRGen *gen, MemFunctionMeta *meta, ExprList *args) {
    if (gen == NULL || meta == NULL || meta->ast_func == NULL || meta->function == NULL ||
        meta->function->is_external || meta->ret_type == TYPE_VOID ||
        meta->ast_func == NULL || args->count != meta->params.count ||
        gen->inline_depth >= 16) {
        return false;
    }
    if (gen->current_function == meta->function) {
        return false;
    }
    return mem_simple_return_expr(meta->ast_func) != NULL ||
           mem_simple_if_return_stmt(meta->ast_func) != NULL ||
           mem_straightline_return_expr(meta->ast_func) != NULL;
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
    } else {
        int last = meta->ast_func->block->items.count - 1;
        for (int i = 0; i < last; ++i) {
            mem_gen_decl(gen, (Decl *)meta->ast_func->block->items.items[i], false);
        }
        result = mem_gen_expr(gen, straightline_return);
    }
    gen->inline_depth--;
    mem_pop_scope(gen);
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

static void generate_program_mem_ir(Program *program, FILE *out) {
    IRModule *module = ast_to_ir(program);
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

typedef struct {
    IRFunction *function;
    RVSlot *slots;
    int next_offset;
    int frame_size;
    int max_outgoing_args;
    FILE *out;
} RVFrame;

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

static void rv_store_int_slot(RVFrame *frame, IRValue *value, const char *reg) {
    RVSlot *slot = rv_find_slot(frame, value);
    if (slot != NULL) {
        rv_emit_mem(frame->out, slot->value_size == 8 ? "sd" : "sw", reg, slot->offset, "s0");
    }
}

static void rv_store_float_slot(RVFrame *frame, IRValue *value, const char *reg) {
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

static void rv_prepare_frame(RVFrame *frame, IRFunction *function) {
    memset(frame, 0, sizeof(RVFrame));
    frame->function = function;
    frame->next_offset = -16;
    for (int i = 0; i < function->params.count; ++i) {
        rv_add_value_slot(frame, &function->params.items[i]->value);
    }
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            if (inst->result_type != NULL && inst->result_type->kind != IR_TYPE_VOID) {
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

static void rv_emit_int_binary(RVFrame *frame, IRInstruction *inst, const char *op) {
    FILE *out = frame->out;
    rv_load_int_value(frame, inst->data.binary_inst.lhs, "t0");
    rv_load_int_value(frame, inst->data.binary_inst.rhs, "t1");
    fprintf(out, "  %s t2, t0, t1\n", op);
    rv_store_int_slot(frame, &inst->result, "t2");
}

static void rv_emit_float_binary(RVFrame *frame, IRInstruction *inst, const char *op) {
    FILE *out = frame->out;
    rv_load_float_value(frame, inst->data.binary_inst.lhs, "ft0");
    rv_load_float_value(frame, inst->data.binary_inst.rhs, "ft1");
    fprintf(out, "  %s ft2, ft0, ft1\n", op);
    rv_store_float_slot(frame, &inst->result, "ft2");
}

static void rv_emit_icmp(RVFrame *frame, IRInstruction *inst) {
    FILE *out = frame->out;
    rv_load_int_value(frame, inst->data.icmp_inst.lhs, "t0");
    rv_load_int_value(frame, inst->data.icmp_inst.rhs, "t1");
    switch (inst->data.icmp_inst.pred) {
        case IR_ICMP_EQ:
            fputs("  subw t2, t0, t1\n  seqz t2, t2\n", out);
            break;
        case IR_ICMP_NE:
            fputs("  subw t2, t0, t1\n  snez t2, t2\n", out);
            break;
        case IR_ICMP_SLT:
            fputs("  slt t2, t0, t1\n", out);
            break;
        case IR_ICMP_SLE:
            fputs("  slt t2, t1, t0\n  xori t2, t2, 1\n", out);
            break;
        case IR_ICMP_SGT:
            fputs("  slt t2, t1, t0\n", out);
            break;
        case IR_ICMP_SGE:
            fputs("  slt t2, t0, t1\n  xori t2, t2, 1\n", out);
            break;
    }
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

static void rv_emit_gep(RVFrame *frame, IRInstruction *inst) {
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
        rv_load_int_value(frame, index, "t1");
        if (stride != 1) {
            fprintf(out, "  li t2, %d\n", stride);
            fputs("  mul t1, t1, t2\n", out);
        }
        fputs("  add t0, t0, t1\n", out);
    }
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
        case IR_INST_ALLOCA: {
            RVSlot *slot = rv_find_slot(frame, &inst->result);
            if (slot != NULL) {
                rv_emit_addi(out, "t0", "s0", slot->object_offset);
                rv_store_int_slot(frame, &inst->result, "t0");
            }
            return;
        }
        case IR_INST_LOAD:
            rv_emit_load_inst(frame, inst);
            return;
        case IR_INST_STORE:
            rv_emit_store_inst(frame, inst);
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
                fprintf(out, "  bnez t0, %s\n", true_label);
                fprintf(out, "  j %s\n", false_label);
            } else {
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
    rv_store_incoming_params(&frame);
    for (int bi = 0; bi < function->blocks.count; ++bi) {
        IRBasicBlock *block = function->blocks.items[bi];
        char *label = rv_block_label(function, block);
        fprintf(out, "%s:\n", label);
        for (IRInstruction *inst = block->first_inst; inst != NULL; inst = inst->next) {
            rv_emit_instruction(&frame, inst);
        }
    }
    fprintf(out, ".L_%s_return:\n", rv_symbol_name(function->name));
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
