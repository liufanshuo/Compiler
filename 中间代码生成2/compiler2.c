#include "compiler2.h"

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

static ParseConstBinding *g_parse_consts = NULL;
#define PARSE_CONST_BUCKETS 4096
static ParseConstBinding *g_parse_const_buckets[PARSE_CONST_BUCKETS];

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
    bool is_const_scalar;
    int const_scalar;
    int *const_flat;
    char *llvm_name;
    char *flat_type;
    Symbol *next;
    Symbol *hash_next;
};

struct FunctionSymbol {
    char *name;
    char *llvm_name;
    TypeSpec ret_type;
    ParamList params;
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

Decl *make_decl(bool is_const, DeclItemList items) {
    Decl *decl = (Decl *)xmalloc(sizeof(Decl));
    decl->is_const = is_const;
    decl->items = items;
    return decl;
}

Param *make_param(char *name, bool is_array, IntList dims) {
    Param *param = (Param *)xmalloc(sizeof(Param));
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
    if (strcmp(func->name, "main") == 0) {
        meta->llvm_name = xstrdup("main");
    } else {
        meta->llvm_name = str_printf("f%d", gen->function_id++);
    }
    meta->ret_type = func->ret_type;
    meta->params = func->params;
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

static int eval_const_expr(IRGen *gen, Expr *expr) {
    switch (expr->kind) {
        case EXPR_NUMBER:
            return expr->data.number;
        case EXPR_LVAL:
            return eval_const_lval(gen, expr->data.lval);
        case EXPR_UNARY: {
            int v = eval_const_expr(gen, expr->data.unary.operand);
            switch (expr->data.unary.op) {
                case UNARY_PLUS: return v;
                case UNARY_MINUS: return -v;
                case UNARY_NOT: return !v;
            }
            return 0;
        }
        case EXPR_BINARY: {
            int lhs = eval_const_expr(gen, expr->data.binary.lhs);
            int rhs = eval_const_expr(gen, expr->data.binary.rhs);
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
            return make_value(str_printf("%d", expr->data.number), 0, NULL, false, false);
        case EXPR_LVAL:
            return gen_lval_expr(gen, expr->data.lval);
        case EXPR_GETINT: {
            char *tmp = new_temp(gen);
            emit_func(gen, "  %s = call i32 @getint()\n", tmp);
            return make_value(tmp, 0, NULL, false, false);
        }
        case EXPR_CALL: {
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
                emit_func(gen, "  call void @putint(i32 %s)\n", i32v);
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putch") == 0) {
                Value arg = gen_expr(gen, expr->data.call.args.items[0]);
                char *i32v = ensure_i32(gen, arg);
                emit_func(gen, "  call void @putch(i32 %s)\n", i32v);
                return make_value(xstrdup("0"), 0, NULL, false, false);
            }
            if (strcmp(expr->data.call.name, "putarray") == 0) {
                Value n = gen_expr(gen, expr->data.call.args.items[0]);
                Value arr = gen_expr(gen, expr->data.call.args.items[1]);
                char *i32v = ensure_i32(gen, n);
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
    emit_func(gen, "  call void @putch(i32 %d)\n", ch);
}

static void gen_printf(IRGen *gen, char *format, ExprList *args) {
    int arg_index = 0;
    int len = (int)strlen(format);
    for (int i = 1; i < len - 1; ++i) {
        if (format[i] == '%' && i + 1 < len - 1 && format[i + 1] == 'd') {
            Value v = gen_expr(gen, args->items[arg_index++]);
            char *i32v = ensure_i32(gen, v);
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
    sb_append(&gen.globals, "source_filename = \"output.ll\"\n");
    sb_append(&gen.globals, "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n");
    sb_append(&gen.globals, "target triple = \"x86_64-pc-linux-gnu\"\n\n");
    sb_append(&gen.globals, "declare i32 @getint()\n");
    sb_append(&gen.globals, "declare i32 @getch()\n");
    sb_append(&gen.globals, "declare i32 @getarray(i32*)\n");
    sb_append(&gen.globals, "declare i8* @memset(i8*, i32, i64)\n");
    sb_append(&gen.globals, "declare void @putint(i32)\n");
    sb_append(&gen.globals, "declare void @putch(i32)\n");
    sb_append(&gen.globals, "declare void @putarray(i32, i32*)\n\n");

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

int main(void) {
    yyin = fopen("testfile.txt", "r");
    if (yyin == NULL) {
        return 1;
    }
    if (yyparse() != 0 || g_program == NULL) {
        fclose(yyin);
        return 1;
    }
    FILE *out = fopen("output.ll", "w");
    if (out == NULL) {
        fclose(yyin);
        return 1;
    }
    generate_program_ir(g_program, out);
    fclose(out);
    fclose(yyin);
    return 0;
}
