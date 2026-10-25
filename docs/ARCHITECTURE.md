# Architecture notes

Crol is a C++20 bytecode implementation of the ROL programming language. The main path in the code is:

~~~text
ROL source
  -> Lexer (lexer.cpp, token.h)
  -> Parser / AST (parser.cpp, nodes.h)
  -> ASTOptimizer (optimizer.cpp)
  -> Compiler (COMPILER.CPP)
  -> Chunk of stack-machine opcodes (chunk.h, operations.h)
  -> BytecodeOptimizer (optimizer.cpp)
  -> VM (vm.cpp, vm.h)
  -> Runtime values / objects / natives (value.*, object.*, natives.*)
~~~

## Front end

The lexer produces tokens consumed by the parser. The AST models expressions, variable declarations, blocks, conditional branches, functions, loops, iterators, and exception paths.

The AST optimizer performs constant evaluation and other transformations. Conditionals and for-loops are kept structurally intact in the present version so that folding does not convert a Block node into an incompatible Condition node or misinterpret NaN-boxed constants.

## Compiler and bytecode

The compiler resolves local variables and captured variables, maintains lexical scopes, and emits bytecode into a Chunk. Jump targets are represented using offsets. A bytecode optimizer runs after compilation; its transformations need to preserve branch destinations and stack discipline.

Nested loop compilation uses a stack of LoopFixupContext objects. Each loop records its own break/continue fixups and stack end position, instead of sharing one global pending-jump vector.

The break target must skip the conditional-expression cleanup POP; a naturally false loop condition still executes that POP because it left the condition value on the VM stack.

## Virtual machine

The VM interprets instructions, advances the instruction pointer, manages a value stack and call frames, and invokes the object/native runtime.

The OP_GOTO_SET_STACK instruction carries both a target and a stack-slot operand; **both operands are decoded before jumping**. This is necessary because reading the second operand after changing the instruction pointer would read bytes from the wrong instruction.

With NAN_BOXING enabled, equality/inequality results must be packed as runtime Boolean values and the opcode cases must terminate without falling through.

## Foreach stack discipline

Foreach uses an iterator holder and a loop-variable slot. Its compiler scope must be isolated from surrounding locals. OP_ITER_END handles iterator cleanup; another compiler-emitted POP for the same locals would corrupt the enclosing stack frame. The iterator-exhaustion path also restores the correct stack end.

## Testing boundaries

The 26-case regression suite tests the core conditional/loop behavior, including nested break/continue and foreach. A passing result does not imply comprehensive correctness of closures/upvalues, exceptions, generators, garbage collection, bytecode overflow, or async execution.
