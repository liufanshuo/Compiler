# 编译器合规检查与修复记录

## 平台要求摘要

根据 `/home/vance/Compiler/平台题目要求.md`，提交物需要实现一个将 SysY2022 程序翻译为 64 位 RISC-V 汇编的编译器。测评调用形式为：

```sh
compiler -S -o testcase.s testcase.sy
compiler -S -o testcase.s testcase.sy -O1
```

编译器需要把目标汇编写入 `testcase.s`，生成的汇编后续应能被汇编、链接，并在 RISC-V Linux 平台运行。

新增违规要求明确禁止：

- 针对特定用例名、测试点名或函数名等做优化。
- 通过给定输出蒙混过关。
- 尝试获得评测机、隐藏用例等信息。
- 其他试图获取不公平优势的行为。

## 当前错误信息

`/home/vance/Compiler/error_information.md` 中记录的新失败点为：

- `2025-N3A-33`：WA，结果哈希值不匹配。
- `2025-EQV-46`：WA，结果哈希值不匹配。
- `2025-PDZ-59`：WA，结果哈希值不匹配。

三个相关 `.sy` 文件位于 `/home/vance/Compiler/RISCV决赛性能用例/RISCV决赛用例/`。检查到 `2025-N3A-33.sy`、`2025-EQV-46.sy`、`2025-PDZ-59.sy` 内容相同，输入文件不同。

## 错误原因

Agent 1 定位到问题来自上一轮通用常量除法/取模优化中的寄存器冲突。

`asm_emit_div_const` 和 `asm_emit_mod_const` 对 2 的幂常量分母生成修正序列时，使用 `t6` 保存符号掩码：

```asm
sraiw t6, a0, 31
```

但大立即数 `and` 辅助函数原先也可能固定使用 `t6` 作为 scratch 寄存器装载立即数，导致类似错误序列：

```asm
sraiw t6, a0, 31
li t6, 131071
and t6, t6, t6
```

这样会覆盖符号掩码，使 `x / 2^k`、`x % 2^k` 这类通用优化在部分正负整数输入上产生错误结果。该问题是后端通用指令选择和寄存器临时量选择的正确性问题，不是某个测试点的专用问题。

## Agent 1 修改内容

Agent 1 只修改了 `/home/vance/Compiler/目标代码生成/compiler.c`，核心修复为：

- 新增 `asm_temp_avoiding`，按已占用的目标/源寄存器选择不冲突的临时寄存器。
- 修复 `asm_emit_and_imm`，当立即数不能编码为 12 位 `andi` 时，scratch 寄存器会避开 `dst` 和 `src`。
- 同步修复同类大立即数辅助函数，包括 `asm_emit_add_imm`、`asm_emit_addw_imm`、`asm_emit_mem`、`sb_emit_add_imm`、`sb_emit_mem`，避免 scratch 覆盖仍需读取的寄存器。
- 保持 `asm_emit_div_const`、`asm_emit_mod_const` 的优化语义为通用常量除法/取模优化，没有加入测试点识别逻辑。

修复后的相关汇编片段会避开 `t6` 冲突，例如：

```asm
li t5, 131071
and t6, t6, t5
```

## 当前编译器主流程

检查 `/home/vance/Compiler/目标代码生成/compiler.c` 后，当前 `main` 流程为：

1. `parse_input_path(argc, argv)` 解析输入 `.sy` 文件路径。
2. `parse_output_path(argc, argv)` 解析 `-o` 后的输出 `.s` 文件路径。
3. `fopen(input_path, "r")` 打开输入。
4. `yyparse()` 解析 SysY 程序，生成 AST。
5. `fopen(output_path, "w")` 打开输出。
6. 统一调用 `generate_program_asm(g_program, out)` 生成汇编。

`generate_program_asm` 直接输出 64 位 RISC-V 汇编，包括：

```asm
.attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0"
.option nopic
.option norelax
```

入口处没有根据输入文件名、测试点名、特定函数名组合、特定全局变量组合或哈希结果分支到专用汇编路径。

## 合规性检查结果

1. 编译器运行时直接生成 64 位 RISC-V 汇编。
   - 当前 `main` 统一调用 `generate_program_asm(g_program, out)`。
   - 生成汇编包含 RISC-V 64 位架构属性和 `.text/.data/.bss` 段。
   - 未发现运行时先生成 C/LLVM IR 再调用外部工具转换的流程。

2. 未发现编译器运行时调用外部编译器或汇编器工具。
   - 检索 `system(`、`popen(`、`fork(`、`posix_spawn(`、`exec*(` 没有匹配。
   - 检索 `clang`、`gcc`、`g++`、`llc`、`riscv64-linux-gnu-gcc`、`riscv64-unknown-linux-gnu-gcc` 没有匹配。
   - 源码中的文件打开仅为读取输入 `.sy` 和写入输出 `.s`：

```text
3373: yyin = fopen(input_path, "r");
3381: FILE *out = fopen(output_path, "w");
```

3. 未发现特定用例或硬编码输出。
   - 检索 `OKA`、`Loka`、`N3A`、`EQV`、`PDZ`、`2025`、`benchmark`、`is_oka` 没有匹配。
   - 检索旧专用路径相关的 `get_random`、`conv2d`、`checksum`、`N_eff`、`KSIZE` 没有匹配。
   - 检索旧期望输出片段 `985110360` 没有匹配。
   - 生成汇编中也未发现这些专用标记。

4. 当前修复是通用正确性修复。
   - 修复点只依赖寄存器使用关系和立即数编码范围。
   - 适用于所有会触发大立即数 `and/add/mem` 辅助序列的 SysY 程序。
   - 没有依据特定输入文件名、函数名、全局变量组合或测试点输出做分支。

## 验证命令记录

构建检查：

```sh
cd /home/vance/Compiler/目标代码生成
make
```

结果：成功，输出 `make: Nothing to be done for 'all'.`

失败相关用例生成检查：

```sh
/home/vance/Compiler/目标代码生成/compiler -S -o /tmp/agent2_n3a_noopt.s /home/vance/Compiler/RISCV决赛性能用例/RISCV决赛用例/2025-N3A-33.sy
/home/vance/Compiler/目标代码生成/compiler -S -o /tmp/agent2_n3a_o1.s /home/vance/Compiler/RISCV决赛性能用例/RISCV决赛用例/2025-N3A-33.sy -O1
/home/vance/Compiler/目标代码生成/compiler -S -o /tmp/agent2_eqv_o1.s /home/vance/Compiler/RISCV决赛性能用例/RISCV决赛用例/2025-EQV-46.sy -O1
/home/vance/Compiler/目标代码生成/compiler -S -o /tmp/agent2_pdz_o1.s /home/vance/Compiler/RISCV决赛性能用例/RISCV决赛用例/2025-PDZ-59.sy -O1
```

结果：全部成功，说明 `compiler -S -o testcase.s testcase.sy` 与 `compiler -S -o testcase.s testcase.sy -O1` 两种形式可用。

生成汇编检查：

```sh
clang --target=riscv64-linux-gnu -c /tmp/agent2_n3a_noopt.s -o /tmp/agent2_n3a_noopt.o
clang --target=riscv64-linux-gnu -c /tmp/agent2_n3a_o1.s -o /tmp/agent2_n3a_o1.o
clang --target=riscv64-linux-gnu -c /tmp/agent2_eqv_o1.s -o /tmp/agent2_eqv_o1.o
clang --target=riscv64-linux-gnu -c /tmp/agent2_pdz_o1.s -o /tmp/agent2_pdz_o1.o
```

结果：全部成功。`file` 检查显示四个目标文件均为 `ELF 64-bit LSB relocatable, UCB RISC-V, RVC, double-float ABI`。

修复片段检查：

```sh
rg -n "\\.attribute arch|\\.option nopic|\\.globl main|li t6, 131071|li t5, 131071|and t6, t6, t6|and t6, t6, t5" /tmp/agent2_n3a_o1.s
```

结果显示汇编包含 64 位 RISC-V 架构属性、`.option nopic`、`.globl main`，并出现修复后的 `li t5, 131071` 与 `and t6, t6, t5`；未出现原有风险形式 `and t6, t6, t6`。

源码合规扫描：

```sh
rg -n "OKA|Loka|2025|N3A|EQV|PDZ|benchmark|is_oka|get_random|conv2d|checksum|N_eff|KSIZE|985110360" /home/vance/Compiler/目标代码生成/compiler.c
rg -n "\\b(system|popen|fork|posix_spawn|execl|execle|execlp|execv|execve|execvp)\\s*\\(" /home/vance/Compiler/目标代码生成/compiler.c
rg -n "\\b(clang|gcc|g\\+\\+|llc|riscv64-linux-gnu-gcc|riscv64-unknown-linux-gnu-gcc)\\b" /home/vance/Compiler/目标代码生成/compiler.c
```

结果：均无匹配。

## 结论

Agent 1 的修改是通用后端正确性修复，解决的是大立即数辅助序列覆盖源/目标寄存器导致的错误代码生成问题。当前编译器运行时仍然直接输出 64 位 RISC-V 汇编，没有调用外部编译器或汇编器工具；也没有发现针对 `2025-N3A-33`、`2025-EQV-46`、`2025-PDZ-59` 或其他特定测试点、函数名组合、全局变量组合的专用路径和硬编码输出。

本次本地验证覆盖了失败相关 `.sy` 文件的普通形式和 `-O1` 形式，生成汇编均可被 `clang --target=riscv64-linux-gnu -c` 汇编为 64 位 RISC-V 目标文件。本地仍没有完整 RISC-V Linux 运行环境，因此未实际链接运行最终 RISC-V 可执行程序。
