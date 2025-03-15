# Asynchronous

Commonly known with the keyword `async` placing before a `function` declarer.
This type of dynamic execution allowed us to run a function for an amount of time, then
halts and passes the turn for other functions.

## Requirements

Because we cannot preserve the native call stack, we need to ensure that `VM::interpret`
aren't triggering itself inside (disable recursive)

* Check list:
1. list_accumulate
2. list_each
3. list_map
4. list_replaceif
5. list_filter
6. list_sort
7. invoke
8. apply
1. tuple_each
1. tuple_map
1. tuple_accumulate
1. Set
1. List
1. Tuple

## Conversion Procedure

- Reimplement every native function that ignites `VM::interpret` using ROL's opcodes
- Reimplement every function that accepts *generator functions* using ROL's opcodes

=> This are the necessary piece needed to enable asynchronous mode implementation in ROL

## Implementing

- A `VM::halt_request` will be added to declare a halt request to the current runtime
- A new separate thread will be created to manage asynchronous functions