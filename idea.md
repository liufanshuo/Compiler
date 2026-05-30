做简单循环优化

在 mem2reg 之后再做：

LICM：循环不变量外提
CSE/GVN：重复表达式消除
Loop strength reduction：数组下标地址递推
IndVar simplify：归纳变量规范化
Loop unroll：小循环可展开