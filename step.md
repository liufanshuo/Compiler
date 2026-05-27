不要在代码中调用外部编译器工具！

在结构体定义中：

IRInstruction 内部持有了 IRBasicBlock *parent;

IRBasicBlock 内部持有了 IRInstruction *first_inst; 和 IRFunction *parent;

IRFunction 内部持有了 IRBasicBlock *first_block;

C 语言编译器在顺序编译时，如果 IRInstruction 在前面，它就不认得 IRBasicBlock。

修改方案： 必须在所有结构体定义的最上方，统一加入前置声明（Forward Declarations）。