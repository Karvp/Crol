#include "vm.h"
#include "common.h"
#include <cstring>

using namespace std;

#define READ_USHORT(o) ((((unsigned short)*(o++)) << 8) + *(o++))
#define READ_UCHAR(o) (*(o++))

#define TO_BOOLEAN(x) ((x) ? *this->pool->ValueConstTrue : *this->pool->ValueConstFalse)
#define NULL_VALUE (*this->pool->ValueConstNull)
#define OBJECT_CONST(id, type) ((type*)AS_OBJECT(consts[id]))

#define UPDATE_LOCALS ops = frame.ops; \
start = frame.start; \
upvalues = frame.upvalues; \
consts = frame.consts; \
stack_begin = frame.stack_begin;

#define LOAD_FRAME(natively_invoked, callee, next_stack_begin, vmcode) this->frames.back().ops = (uint8_t*)ops; \
this->newCallFrame(natively_invoked, callee, next_stack_begin, vmcode);

#define JUMP_OR_SKIP(check, ops, start) if (check) { ops = start + READ_USHORT(ops); } else { ops += 2; }

#define GEN_TERROR(msg) Value(NATIVE_TYPE_ERROR(this, msg))


void VM::callValue(Object* callee_ptr, Value* call_ptr, const ushort arg_count, bool native_invocation, VMControlCode control_code) {

	Value* invokeas = call_ptr++;

call_type_switch:
	switch (callee_ptr->type) {
	case OBJECT_CLOSURE: {
		auto callee = (ClosureObject*)callee_ptr;

		if (callee->function->arity != arg_count) {
			string msg = "Passing ";
			msg.append(to_string(arg_count));
			msg += " argument(s) while ";
			msg.append(to_string(callee->function->arity));
			msg += " are required ";

			this->throwValue(this->newException<TypeErrorObject>(msg.c_str()));
			break;
		}

		LOAD_FRAME(native_invocation, callee, invokeas, control_code);
		break;
	}
	case OBJECT_NATIVE_FUNCTION: {
		// Push the return value on top of the stack
		((NativeFunctionObject*)callee_ptr)->function(this, *invokeas, call_ptr, arg_count);
		break;
	}
	case OBJECT_BOUND_FUNCTION: {
		auto callee = ((BoundFunctionObject*)callee_ptr);

		callee_ptr = callee->function;
		*invokeas = callee->thisValue;

		goto call_type_switch;
	}
	default:
		this->throwValue(this->newException<TypeErrorObject>("Callee is not callable"));
	}
}

void VM::throwValue(const Value& v) {
	if (stack_pos.empty()) {
		if (IS_OBJECT(v) && dynamic_cast<ExceptionObject*>(AS_OBJECT(v)) != nullptr) {
			throw v;
		}
		else {
			throw this->uncatchedException(v);
		}
	}

	if (throwing) {
		printf("[WARNING] An exception was already thrown.\n");
	}
	throwing = true;

	auto catcher = this->stack_pos.back();

	// If there is a catcher, one of the frames must be holding the
	// try catch block
	if (this->frames.back().stack_begin > catcher.second) {
		this->frames.pop_back();

		while (this->frames.back().stack_begin > catcher.second) {
			this->frames.pop_back();
		}

		frame = this->frames.back();
		UPDATE_LOCALS;
	}

	stack_end = catcher.second;
	*++stack_end = v;

	ops = catcher.first;
}

VMExecutionResult VM::interpret() {
	for (;;) {
		switch (*ops++) {

			 // Stack mutation
		case OP_CONST:
			*++stack_end = consts[READ_USHORT(ops)];
			continue;
		case OP_TRUE:
			*++stack_end = TO_BOOLEAN(true);
			continue;
		case OP_FALSE:
			*++stack_end = TO_BOOLEAN(false);
			continue;
		case OP_NULL:
			*++stack_end = NULL_VALUE;
			continue;
		case OP_SET:
			stack_begin[READ_USHORT(ops)] = *stack_end;
			continue;
		case OP_COPY:
			*++stack_end = stack_begin[READ_USHORT(ops)];
			continue;
		case OP_POP:
			--stack_end;
			continue;
		case OP_CLOSURE: {
			FunctionObject* func = OBJECT_CONST(READ_USHORT(ops), FunctionObject);
			ClosureObject* closure = new ClosureObject(func);
			
			closure->prototype = this->pool->nativeBase->functionPrototype;
			this->pool->registerObject(closure);
			
			// Capture upvalues
			for (int i = 0; i < func->upvalue_count; ++i) {
				int capture_pos = READ_USHORT(ops);
				bool isLocal = READ_UCHAR(ops);

				if (isLocal) {
					closure->upvalues[i] = this->pool->addUpvalue(stack_begin + capture_pos, capture_pos);
				}
				else {
					closure->upvalues[i] = frame.closure->upvalues[capture_pos];
				}
			}

			*++stack_end = Value(closure);
			continue;
		}

			 // Unary
		case OP_NOT:
			switch (stack_end->type) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(!stack_end->as.num);
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(!stack_end->as.boolean);
				continue;
			case VAL_NULL:
				*stack_end = TO_BOOLEAN(true);
				continue;
			case VAL_OBJ:
				*stack_end = TO_BOOLEAN(false);
				continue;
			default:
				// Unreachable
				this->throwValue(GEN_TERROR("Unknown value type")); \
				continue;
			}
		case OP_NEGATE:
			if (!IS_NUMBER(*stack_end)) {
				this->throwValue(GEN_TERROR("Invalid negation operand"));
				continue;
			}
			AS_NUMBER(*stack_end) = -AS_NUMBER(*stack_end);
			continue;
		case OP_INCREASE:
		{
			Value& a = stack_begin[READ_USHORT(ops)];
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid increment operand"));
				continue;
			}
			++AS_NUMBER(a);
			continue;
		}
		case OP_DECREASE:
		{
			Value& a = stack_begin[READ_USHORT(ops)];
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid decrement operand"));
				continue;
			}
			--AS_NUMBER(a);
			continue;
		}

		// Special
		case OP_IS_PROTOTYPE: {
			Value& b = *(stack_end--), &a = *stack_end;

			if (!IS_OBJECT(*stack_end) or !IS_OBJECT(b)) {
				*stack_end = *pool->ValueConstFalse;
				continue;
			}

			Object* obja = AS_OBJECT(a), * objb = AS_OBJECT(b)->prototype;
			if (!objb) {
				*stack_end = *pool->ValueConstFalse;
				continue;
			}

			bool check = false;
			do {
				if (obja == objb) {
					check = true;
					break;
				}
			} while (objb = objb->prototype);

			*stack_end = TO_BOOLEAN(check);
			continue;
		}

		// Mathmetic
		case OP_ADD:
		{
			Value& b = *(stack_end--);

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = Value(AS_NUMBER(*stack_end) + AS_NUMBER(b));
			}
			else if (IS_STRINGOBJ(*stack_end) and IS_STRINGOBJ(b)) {
				*stack_end = Value(pool->internString(*AS_STRING(*stack_end) + *AS_STRING(b)));
			}
			else {
				this->throwValue(GEN_TERROR("Operation + applied on invalid value types"));
			}
			continue;
		}
		case OP_SUB:
		{
			Value& b = *(stack_end--);

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = Value(AS_NUMBER(*stack_end) - AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation - applied on invalid value types"));
			}
			continue;
		}
		case OP_MUL:
		{
			Value& b = *(stack_end--);

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = Value(AS_NUMBER(*stack_end) * AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation * applied on invalid value types"));
			}
			continue;
		};
		case OP_DIV:
		{
			Value& b = *(stack_end--);

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = Value(AS_NUMBER(*stack_end) / AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation / applied on invalid value types"));
			}
			continue;
		};
		case OP_POW:
		{
			Value& b = *(stack_end--);

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = Value(pow(AS_NUMBER(*stack_end), AS_NUMBER(b)));
			}
			else {
				this->throwValue(GEN_TERROR("Operation ^ applied on invalid value types"));
			}
			continue;
		};

		// Comparison
		case OP_EQUAL:
		{
			Value& b = *(stack_end--), & a = *stack_end;

			if (b.type != a.type) {
				*stack_end = *pool->ValueConstFalse;
				continue;
			}

			switch (b.type) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(b.as.num == a.as.num);
				continue;
			case VAL_NULL:
				*stack_end = *pool->ValueConstTrue;
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(b.as.boolean == a.as.boolean);
				continue;
			case VAL_OBJ:
				*stack_end = TO_BOOLEAN(b.as.obj == a.as.obj);
				continue;
			default:
				// Unreachable
				throw exception("Corrupted Value");
			}
		}
		case OP_NOT_EQUAL: 
		{
			Value& b = *(stack_end--), & a = *stack_end;

			if (b.type != a.type) {
				*stack_end = *pool->ValueConstTrue;
				continue;
			}

			switch (b.type) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(b.as.num != a.as.num);
				continue;
			case VAL_NULL:
				*stack_end = *pool->ValueConstFalse;
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(b.as.boolean != a.as.boolean);
				continue;
			case VAL_OBJ:
				*stack_end = TO_BOOLEAN(b.as.obj != a.as.obj);
				continue;
			default:
				// Unreachable
				throw exception("Corrupted Value");
			}
		}
		case OP_LESS:
		{
			Value& b = *(stack_end--);
			if (IS_NUMBER(b) and IS_NUMBER(*stack_end)) {
				*stack_end = TO_BOOLEAN(AS_NUMBER(*stack_end) < AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation < applied on invalid value types"));
			}
			continue;
		}
		case OP_GREATER:
		{
			Value& b = *(stack_end--);
			if (IS_NUMBER(b) and IS_NUMBER(*stack_end)) {
				*stack_end = TO_BOOLEAN(AS_NUMBER(*stack_end) > AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation > applied on invalid value types"));
			}
			continue;
		}
		case OP_LESS_EQUAL:
		{
			Value& b = *(stack_end--);
			if (IS_NUMBER(b) and IS_NUMBER(*stack_end)) {
				*stack_end = TO_BOOLEAN(AS_NUMBER(*stack_end) <= AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation <= applied on invalid value types"));
			}
			continue;
		}
		case OP_GREATER_EQUAL:
		{
			Value& b = *(stack_end--);
			if (IS_NUMBER(b) and IS_NUMBER(*stack_end)) {
				*stack_end = TO_BOOLEAN(AS_NUMBER(*stack_end) >= AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation >= applied on invalid value types"));
			}
			continue;
		}

			 // Altering execution order
		case OP_GOTO:
			ops = start + READ_USHORT(ops);
			continue;
		case OP_GOTO_IF_FALSE:

			switch (stack_end->type) {
			case VAL_NUM:
				JUMP_OR_SKIP(!AS_NUMBER(*stack_end), ops, start);
				continue;
			case VAL_BOOL:
				JUMP_OR_SKIP(!AS_BOOLEAN(*stack_end), ops, start);
				continue;
			case VAL_NULL:
				JUMP_OR_SKIP(true, ops, start);
				continue;
			case VAL_OBJ:
				JUMP_OR_SKIP(false, ops, start);
				continue;
			default:
				// Unreachable
				throw exception("Unknown value type");
			}

		case OP_GOTO_IF_TRUE:

			switch (stack_end->type) {
			case VAL_NUM:
				JUMP_OR_SKIP(AS_NUMBER(*stack_end), ops, start);
				continue;
			case VAL_BOOL:
				JUMP_OR_SKIP(AS_BOOLEAN(*stack_end), ops, start);
				continue;
			case VAL_NULL:
				JUMP_OR_SKIP(false, ops, start);
				continue;
			case VAL_OBJ:
				JUMP_OR_SKIP(true, ops, start);
				continue;
			default:
				// Unreachable
				throw exception("Unknown value type");
			}

			 // Functional
		case OP_PRINT:
			(stack_end--)->print();
			printf("\n");
			continue;

		case OP_MEM_GET: {
			// |object|key|
			Value& a = *(stack_end - 1);
			if (!IS_OBJECT(a)) {
				this->throwValue(GEN_TERROR("Can't get member of a non-object"));
				continue;
			}
			*stack_end = AS_OBJECT(a)->get(*stack_end--, this);
			continue;
		}
		case OP_MEM_SET: {
			// |value|object|key|
			Value& obj = *(stack_end - 1);
			if (!IS_OBJECT(obj)) {
				this->throwValue(GEN_TERROR("Can't get member of a non-object"));
				continue;
			}
			AS_OBJECT(obj)->set(*stack_end, *(stack_end - 2));
			stack_end -= 2;
			continue;
		}

		case OP_GEN_OBJ: {
			// Stack order: |key|value|

			ushort size = READ_USHORT(ops);
			const Value& prototype = *stack_end--;
			Object* obj = new Object();
			for (ushort i = size; i; --i) {
				obj->props[*stack_end--] = *stack_end--;
			}
			this->pool->registerObject(obj);

			if (IS_OBJECT(prototype)) {
				obj->prototype = AS_OBJECT(prototype);
			}
			*++stack_end = Value(obj);
			continue;
		}
		case OP_GEN_LIST: {
			ushort size = READ_USHORT(ops);
			ListObject* list = new ListObject(stack_end - size + 1, size);

			this->pool->registerList(list);
			*++(stack_end -= size) = Value(list);
			continue;
		}
		case OP_GEN_TUPLE: {
			ushort size = READ_USHORT(ops);
			TupleObject* tuple = new TupleObject(stack_end - size + 1, size);

			this->pool->registerTuple(tuple);
			*++(stack_end -= size) = Value(tuple);
			continue;
		}
		case OP_LIST_FROM_CONST_TUPLE: {
			const TupleObject* tup = AS_TUPLE(consts[READ_USHORT(ops)]);
			ListObject* list = new ListObject(tup->list, tup->size);

			this->pool->registerList(list);
			*++stack_end = Value(list);
			continue;
		}

			 // Names
		case OP_GET_UPVALUE:
			*++stack_end = *upvalues[READ_USHORT(ops)]->location;
			continue;
		case OP_SET_UPVALUE:
			*upvalues[READ_USHORT(ops)]->location = *stack_end;
			continue;
		case OP_INCREASE_UPVALUE: {
			Value& a = *upvalues[READ_USHORT(ops)]->location;
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid increment operand"));
				continue;
			}
			++AS_NUMBER(a);
			continue;
		}
		case OP_DECREASE_UPVALUE: {
			Value& a = *upvalues[READ_USHORT(ops)]->location;
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid decrement operand"));
				continue;
			}
			--AS_NUMBER(a);
			continue;
		}

		case OP_CLOSE_UPVALUE: {
			UpvalueObject* upvalue = this->pool->getUpvalue(stack_end);
			if (upvalue) {
				upvalue->closed = *stack_end;
				upvalue->location = &upvalue->closed;
			}
			--stack_end;
			continue;
		}

		case OP_FLAG_CATCH:
			this->stack_pos.emplace_back(start + READ_USHORT(ops), stack_end);
			continue;
		case OP_UNFLAG_CATCH:
			this->stack_pos.pop_back();
			throwing = false;
			continue;
		case OP_RAISE:
			if (frame.finishCode == VMCODE_BREAK) {
				this->throwValue(*stack_end);
				return EXECUTION_HALTED_BY_EXCEPTION;
			}
			else
				this->throwValue(*stack_end);
			continue;

		case OP_CALL: {
			const ushort arg_count = READ_USHORT(ops);
			Value* vcallee_ptr = stack_end - arg_count;

			if (!IS_OBJECT(*vcallee_ptr)) {
				this->throwValue(GEN_TERROR("Calling on a non-object value"));
			}

			Object* callee_ptr = AS_OBJECT(*vcallee_ptr);
			*vcallee_ptr = *stack_begin;

			this->callValue(callee_ptr, vcallee_ptr, arg_count);

			continue;
		}
		case OP_NEW: {
			const ushort arg_count = READ_USHORT(ops);
			Value* vcallee_ptr = stack_end - arg_count;

			if (!IS_OBJECT(*vcallee_ptr)) {
				this->throwValue(GEN_TERROR("Applying 'new' operator on a non-object value"));
			}

			Object* callee_ptr = AS_OBJECT(*vcallee_ptr);
			auto new_proto = callee_ptr->get(Value(pool->prototypeString), this);

			Object* new_inst = new Object();
			if (IS_OBJECT(new_proto)) {
				new_inst->prototype = AS_OBJECT(new_proto);
			}

			*vcallee_ptr = Value(new_inst);
			pool->registerObject(new_inst);

			this->callValue(callee_ptr, vcallee_ptr, arg_count);

			continue;
		}
		case OP_INVOKE: {
			// Stack order: |obj|args...|prop|

			const ushort arg_count = READ_USHORT(ops);
			Value& vprop = *stack_end--, * obj_ptr = stack_end - arg_count;

			if (!IS_OBJECT(*obj_ptr)) {
				this->throwValue(GEN_TERROR("Can't get member of a non-object"));
			}

			Value vcallee_ptr = AS_OBJECT(*obj_ptr)->get(vprop, this);

			if (!IS_OBJECT(vcallee_ptr)) {
				this->throwValue(GEN_TERROR("Calling on a non-function value"));
			}

			this->callValue(AS_OBJECT(vcallee_ptr), obj_ptr, arg_count);
			
			continue;
		}

		case OP_RETURN: {
			if (this->frames.size() == 1) {
				return EXECUTION_SUCCEED;
			}
			Value* ret_ptr = stack_end;
			stack_end = stack_begin - frame.shift_back;
			*stack_end = *ret_ptr;

			VMControlCode finishCode = frame.finishCode;

			this->frames.pop_back();

			frame = this->frames.back();
			// Update local variables
			UPDATE_LOCALS;

			if (finishCode) {
				return EXECUTION_HALTED_BY_NATIVE;
			}
			continue;
		}
		}
	}
}

#undef LOAD_FRAME
#undef UPDATE_LOCALS
#undef OBJECT_CONST
#undef NULL_VALUE
#undef TO_BOOLEAN

#undef READ_UCHAR
#undef READ_USHORT