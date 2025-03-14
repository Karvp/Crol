#pragma once
#include <stdint.h>
#include "common.h"

enum OP {
	// Stack mutating
	OP_CONST, OP_OBJECT_CONST, OP_LIST_FROM_CONST_TUPLE, OP_TRUE, OP_FALSE, OP_NULL, OP_SET, OP_COPY, OP_POP,
	// Unary operations
	OP_NOT, OP_NEGATE, OP_INCREASE, OP_DECREASE,
	// Math operations
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW,
	// Comparison
	OP_LESS, OP_GREATER, OP_LESS_EQUAL, OP_GREATER_EQUAL, OP_EQUAL, OP_NOT_EQUAL,
	// Altering execution order
	OP_GOTO, OP_GOTO_IF_FALSE, OP_GOTO_IF_TRUE, OP_GOTO_SET_STACK,
	OP_POP_GOTO_IF_FALSE,
	// Iterator
	OP_ITER, OP_ITER_NEXT, OP_ITER_END,
	// Functional operations
	OP_PRINT, OP_IS_PROTOTYPE,
	// Upvalue operations
	OP_SET_UPVALUE, OP_GET_UPVALUE, OP_INCREASE_UPVALUE, OP_DECREASE_UPVALUE, OP_CLOSE_UPVALUE,
	// Functions
	OP_CALL, OP_INVOKE, OP_CLOSURE, OP_NEW,
	// Objects
	OP_MEM_GET, OP_MEM_SET,
	// Generators
	OP_GEN_OBJ, OP_GEN_LIST, OP_GEN_TUPLE,
	OP_YIELD,
	// Try catch
	OP_FLAG_CATCH, OP_UNFLAG_CATCH, OP_RAISE,
	// Ends
	OP_RETURN
};

struct Operation {
	OP type;
	bytecode* arg;
	int arg_count;

	Operation(OP type, bytecode* arg, int arg_count) : type(type), arg(arg), arg_count(arg_count) {}

	constexpr bool operator==(const Operation& op) const noexcept {
		if (this->type != op.type || this->arg_count != op.arg_count) {
			return false;
		}

		for (size_t i = 0; i < arg_count; ++i) {
			if (*(this->arg + i) != *(op.arg + i)) {
				return false;
			}
		}

		return true;
	}

	constexpr bool operator!=(const Operation& op) const noexcept {
		return !(*this == op);
	}

	constexpr int size() const noexcept {
		return arg_count + 1;
	}

	constexpr const char* name() const noexcept {
		switch (type) {
		case OP_CONST:
			return "OP_CONST";
		case OP_OBJECT_CONST:
			return "OP_OBJECT_CONST";
		case OP_TRUE:
			return "OP_TRUE";
		case OP_FALSE:
			return "OP_FALSE";
		case OP_NULL:
			return "OP_NULL";
		case OP_CLOSURE:
			return "OP_CLOSURE";
		case OP_SET:
			return "OP_SET";
		case OP_COPY:
			return "OP_COPY";
		case OP_POP:
			return "OP_POP";
		case OP_CLOSE_UPVALUE:
			return "OP_CLOSE_UPVALUE";

		case OP_ADD:
			return "OP_ADD";
		case OP_SUB:
			return "OP_SUB";
		case OP_MUL:
			return "OP_MUL";
		case OP_DIV:
			return "OP_DIV";
		case OP_POW:
			return "OP_POW";

		case OP_NOT:
			return "OP_NOT";
		case OP_NEGATE:
			return "OP_NEGATE";
		case OP_INCREASE:
			return "OP_INCREASE";
		case OP_DECREASE:
			return "OP_DECREASE";

		case OP_EQUAL:
			return "OP_EQUAL";
		case OP_NOT_EQUAL:
			return "OP_NOT_EQUAL";
		case OP_LESS:
			return "OP_LESS";
		case OP_GREATER:
			return "OP_GREATER";
		case OP_LESS_EQUAL:
			return "OP_LESS_EQUAL";
		case OP_GREATER_EQUAL:
			return "OP_GREATER_EQUAL";

		case OP_GOTO:
			return "OP_GOTO";
		case OP_GOTO_IF_FALSE:
			return "OP_GOTO_IF_FALSE";
		case OP_POP_GOTO_IF_FALSE:
			return "OP_POP_GOTO_IF_FALSE";
		case OP_GOTO_IF_TRUE:
			return "OP_GOTO_IF_TRUE";
		case OP_GOTO_SET_STACK:
			return "OP_GOTO_SET_STACK";

		case OP_ITER:
			return "OP_ITER";
		case OP_ITER_NEXT:
			return "OP_ITER_NEXT";
		case OP_ITER_END:
			return "OP_ITER_END";

		case OP_PRINT:
			return "OP_PRINT";
		case OP_IS_PROTOTYPE:
			return "OP_IS_PROTOTYPE";

		case OP_SET_UPVALUE:
			return "OP_SET_UPVALUE";
		case OP_GET_UPVALUE:
			return "OP_GET_UPVALUE";
		case OP_INCREASE_UPVALUE:
			return "OP_INCREASE_UPVALUE";
		case OP_DECREASE_UPVALUE:
			return "OP_DECREASE_UPVALUE";

		case OP_MEM_GET:
			return "OP_MEM_GET";
		case OP_MEM_SET:
			return "OP_MEM_SET";

		case OP_GEN_OBJ:
			return "OP_GEN_OBJ";
		case OP_GEN_LIST:
			return "OP_GEN_LIST";
		case OP_GEN_TUPLE:
			return "OP_GEN_TUPLE";
		case OP_LIST_FROM_CONST_TUPLE:
			return "OP_LIST_FROM_CONST_TUPLE";

		case OP_CALL:
			return "OP_CALL";
		case OP_INVOKE:
			return "OP_INVOKE";
		case OP_NEW:
			return "OP_NEW";

		case OP_FLAG_CATCH:
			return "OP_FLAG_CATCH";
		case OP_UNFLAG_CATCH:
			return "OP_UNFLAG_CATCH";
		case OP_RAISE:
			return "OP_RAISE";

		case OP_YIELD:
			return "OP_YIELD";
		case OP_RETURN:
			return "OP_RETURN";

		default:
			return "UNKNOWN";
		}
	}
};