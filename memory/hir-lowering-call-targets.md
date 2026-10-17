# HIR Call Target And Receiver Syntax

`methodDeclFromTarget` returns the declaration for any resolved function, including free generic calls with an explicit leading `self` parameter. Its result alone does not mean the source call has an implicit receiver. `HirLowerModern::lowerCall` adds a receiver argument only when the callee is a `Field` or `Arrow` expression with a receiver operand, the resolved declaration has an owner, and its first parameter is `self`.
