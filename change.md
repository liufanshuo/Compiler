1. 定义真正的“内存 IR”数据结构
你目前的 generate_program_ir 只是用 sb_appendf 把字符串拼起来。而现代架构需要你在内存中用 C 结构体把这些指令显式地表达出来。你需要定义类似下面的结构：
```C
typedef enum {
    IR_ALLOCA, IR_LOAD, IR_STORE,
    IR_ADD, IR_SUB, IR_MUL, IR_SDIV, IR_SREM,
    IR_BR, IR_RET, IR_CALL, IR_GETELEMENTPTR
    // ... 对齐你列出的所有硬性指令
} IRInstKind;

// 抽象的一条 IR 指令
typedef struct IRInstruction {
    IRInstKind kind;
    char *dest;          // 虚拟寄存器名，如 "%t0"
    char *op1;           // 操作数 1
    char *op2;           // 操作数 2
    TypeSpec type;       // 类型标记
    struct IRInstruction *next;
} IRInstruction;

// 基本块
typedef struct IRBasicBlock {
    char *label;
    IRInstruction *first_inst;
    IRInstruction *last_inst;
    struct IRBasicBlock *next;
} IRBasicBlock;
```

2. 让 generate_program_ir 重构为 Lowerer
你需要修改当前的 generate_program_ir()。遍历 AST 的时候，不再调用 sb_appendf 打印文本，而是去 malloc 上面定义的 IRInstruction 节点，把它们串成一条链表，挂在对应的 IRBasicBlock 和 FuncDef 下面。

比如当遇到 EXPR_BINARY 节点时：

旧做法：emit_func(gen, "  %s = add i32 %s, %s\n", ...)

新做法：创建一个 IRInstruction，将 kind 设为 IR_ADD，op1 和 op2 指向子节点返回的虚拟寄存器，然后拼接到当前基本块的末尾。

3. 彻底重写 RISC-V 后端
这是重构中最大的一步。你需要干掉你目前多达上千行的 asm_gen_expr、asm_gen_stmt、asm_try_gen_binary_no_stack 等直接面对 AST 的复杂递归函数。

你全新的 RISC-V 后端入口应该长这样：

```C
void emit_riscv_from_ir(ProgramIR *ordered_ir, FILE *out) {
    for (IRFunction *func = ordered_ir->funcs; func; func = func->next) {
        // 1. 进行图着色或者线性扫描寄存器分配（让虚拟寄存器映射到 a0-a7, t0-t6）
        RegisterAllocation(func);
        
        // 2. 平坦地遍历每一条内存 IR 指令，直接发射汇编
        for (IRBasicBlock *bb = func->blocks; bb; bb = bb->next) {
            fprintf(out, "%s:\n", bb->label);
            for (IRInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                // 根据分配好的寄存器映射，直接转换成 RISC-V 汇编
                emit_riscv_inst(inst, out); 
            }
        }
    }
}
```