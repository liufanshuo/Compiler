 # 严格合规的 SysY2022 -> RISC-V 性能优化计划

  ## Summary

  在遵循 /home/vance/Compiler/harness.md 的严格保守边界下，后续优化只做通用编译器优化：不新增按用例名、函数名、固定输出或特定算法模板匹配的逻辑。当前重点放在 目标代码生成/
  compiler.c 的 IR 优化、循环寻址优化、寄存器分配和汇编 peephole 上，目标是减少热点循环里的 lw/sw/ld/sd/mv/li/j 数量。

  ## Key Changes

  - 合规清理：
      - 禁止新增 system/popen/fork/exec 或任何外部编译器调用。
      - 禁止新增按测试文件名、函数名、固定常量输出或算法模板识别的优化。
      - 严格模式下停用或移除边界敏感优化入口：ast_match_mod_multiply_func、ast_match_digit_extract_func 及其 RISC-V 特化发射；递归 memo 若无法证明为纯通用优化，也先关闭。

  - IR 级通用优化：
      - 保留并强化 mem2reg、死代码删除、常量折叠、公共子表达式删除、块内 load/store forwarding。
      - 启用并验证通用 loop pointer induction strength reduction，把循环内 base + i * stride 的重复 GEP 转成循环递增地址。
      - 强化 LICM，只提升无副作用、别名安全的循环不变量，包括数组基址、行基址、常量乘法结果。

  - RISC-V 后端优化：
      - 扩展热点循环寄存器 home：优先给归纳变量、循环不变数组基址、行基址、频繁使用的 phi/param 分配 s1-s11。
      - 扩展短生命周期临时寄存器缓存：对无调用基本块使用更多 caller-saved 寄存器，减少单次使用值落栈。
      - 优化 load/store 地址生成：更多单用户 GEP 直接折叠进访存，不为中间地址分配栈槽。
      - 改进分支生成与布局：条件比较直接生成 blt/bge/beq/bne，按 fallthrough 消除 j next_label，避免先生成 0/1 布尔临时值再分支。
      - 扩展 peephole：删除 mv x,x、相邻 sw/lw 或 sd/ld 往返、重复 li、重复 slli、无效 addi 0、跳到下一标签的 j。

  ## Test Plan

  - 构建与静态合规：
      - make 编译编译器。
      - 扫描确认无 system(、popen(、fork(、exec*( 外部调用。
      - 检查无按用例名、文件名、固定输出、非运行时库函数名的优化分支。

  - 正确性：
      - 编译并运行全部 原始公开测试点/functional_easy、functional_hard。
      - 编译并运行 functional_recover/functional 和 functional_recover/h_functional。
      - 对数组别名、负数除法/取模、浮点、短路、phi cycle、递归、break/continue 补充小型回归用例。

  - 性能：
      - 对 原始公开测试点/performance_easy 和 RISCV决赛性能用例/RISCV决赛用例 统计生成汇编的指令构成。

  ## Assumptions

  - 优化入口集中在 目标代码生成/compiler.c，不改变命令行接口：compiler -S -o testcase.s testcase.sy。
  - 允许开发/验证阶段使用本机编译器和测试工具，但生成的编译器代码本身不能调用外部编译器或工具。
  - 优先级是严格合规和正确性，其次是性能；不采用任何可能被解释为针对特定测试或特定算法模板的新增优化。