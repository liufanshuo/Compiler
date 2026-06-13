真正的全函数寄存器分配

现在后端有 rv_plan_reg_temps_for_block() 和 loop persistent reg，但整体还是偏“局部临时寄存器 + 栈槽”。

建议做一个简化版 linear scan：

1. 在 SSA IR 上统计每个 value 的 live interval
2. 按 start 排序
3. 优先分配 t/a 临时寄存器
4. live across call 的值优先给 s 寄存器或 spill
5. 常量、简单 GEP 支持 rematerialization，不占栈槽

不用一上来做图着色。Linear scan 对你当前 IR 已经够用，能明显减少：

sw ...
lw ...
sd ...
ld ...

尤其是循环内部的 stack traffic。