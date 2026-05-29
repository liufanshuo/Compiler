把 icmp -> zext -> icmp ne -> br 合并

你现在的条件经常是：

%cmp = icmp slt i32 %i, 1000000
%z = zext i1 %cmp to i32
%c = icmp ne i32 %z, 0
br i1 %c, ...

应该变成：

blt t0, t1, true_label
j false_label

你已经有部分 compare-branch fusion，但前面的 zext + icmp ne 会挡住它。加一个 IR peephole：

icmp ne (zext i1 X to i32), 0  => X
icmp eq (zext i1 X to i32), 0  => not X