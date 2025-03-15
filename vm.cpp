#include "vm.h"
#include "natives.h"
#include "object.h"
#include <cstring>

using namespace std;

#define READ_USHORT(o) ((chunk_size)((chunk_size)((*(o++)) << 8) + *(o++)))
#define READ_UCHAR(o) (*(o++))

#define TO_BOOLEAN(x) ((x) ? pool->ValueConstTrue : pool->ValueConstFalse)
#define NULL_VALUE (pool->ValueConstNull)
#define OBJECT_CONST(id, type) ((type*)AS_OBJECT(consts[id]))

#define LOAD_FRAME(cf) this->frames.back().ops = (bytecode*)ops; \
this->newCallFrame(cf);

#define JUMP_OR_SKIP(check, ops, start) if (check) { ops = start + READ_USHORT(ops); } else { ops += 2; }

#define GEN_TERROR(msg) toValue(NATIVE_TYPE_ERROR(this, msg))

CallFrame::CallFrame(NativeFunctionObject* callee, bool new_invocation) :
	callee(callee),
	invoked_by_new_op(new_invocation)
{}

CallFrame::CallFrame(ClosureObject* callee, Value* stack_begin, bool new_invocation, VMControlCode finishCode) :
	callee(callee),
	stack_begin(stack_begin),
	finishCode(finishCode)
{
	ops = start = ((FunctionObject*)callee->function)->chunk->getOPs().data();
}
constexpr bool CallFrame::isNativeFrame() const noexcept {
	return callee->type != OBJECT_CLOSURE;
}
constexpr const char* CallFrame::getFuncName() const noexcept {
	return callee->getFuncName();
}
constexpr UpvalueArray CallFrame::getUpvalues() const noexcept {
	return ((ClosureObject*)callee)->upvalues;
}
constexpr Value* CallFrame::getConsts() const noexcept {
	return ((FunctionObject*)((ClosureObject*)callee)->function)->chunk->getConsts().data();
}



void VM::preinit(NativeBase * nb) {
	this->nb = nb;
	stack_begin = stackPtr = new Value[STACK_MAX_SIZE];
	stack_end = stackPtr - 1;

	*++stack_end = NULL_VAL;
	for (auto& x : nb->getNatives()) {
		*++stack_end = x;
	}
}

ExceptionObject* VM::newException(const char* msg, ExceptionType exc_type) {
	// Create callframe list
	vector<const char*> cfnames;
	for (const auto& i : frames) {
		cfnames.push_back(i.getFuncName());
	}

	auto exc = new ExceptionObject(msg, exc_type, cfnames);
	exc->prototype = pool->nativeBase->exceptionPrototype;

	this->pool->registerObject(exc);
	return exc;
}

Value VM::uncatchedException() {
	return toValue(newException(NULL, EXCEPT_RAISED_ERROR));
}

void VM::run() {
	Value* snap_stack = stack_end;
	execution_finished = false;
	
	try {
		frames.clear();
		this->newCallFrame(CallFrame((ClosureObject*)this->callee, this->stackPtr));
		this->interpret();

		execution_finished = true;
	}
	catch (Value e) {
		printf("Traceback most recent call(s):\n\n");
		TValue::print(e);

		stack_end = snap_stack;
		throwing = false;
	}
	catch (...) {
		printf("Unknown internal exception caught!\n\n");

		stack_end = snap_stack;
		throwing = false;
	}
}

bool VM::callValue(Object* callee_ptr, Value* call_ptr, const ushort arg_count, VMControlCode control_code, bool new_invocation) {

	if (frames.size() == CALL_STACK_MAX_COUNT) {
		this->throwValue(toValue(this->newException("Max call stack exceeded", EXCEPT_GENERAL_ERROR)));
		return true;
	}

	Value* invokeas = call_ptr++;

call_type_switch:
	switch (callee_ptr->type) {
	case OBJECT_CLOSURE: {
		auto callee = (ClosureObject*)callee_ptr;

		if (callee->is_generator) {
			auto generator = new IterableObject(RolIterable(
				RolGeneratorIterator(this, callee, call_ptr, arg_count),
				RolGeneratorIterator()
			), pool);
			pool->registerIterable(generator);
			*(this->stack_end -= arg_count) = toValue(generator);
			return true;
		}
		else {
			if (callee->getArity() != arg_count) {
				string msg = "Passing ";
				msg.append(to_string(arg_count));
				msg += " argument(s) while ";
				msg.append(to_string(callee->getArity()));
				msg += " are required ";

				this->throwValue(toValue(this->newException(msg.c_str(), EXCEPT_TYPE_ERROR)));
				break;
			}

			LOAD_FRAME(CallFrame(callee, invokeas, new_invocation, control_code));
		}
		break;
	}
	case OBJECT_NATIVE_FUNCTION: {
		// Push the return value on top of the stack
		LOAD_FRAME(CallFrame((NativeFunctionObject*)callee_ptr, new_invocation));
		((NativeFunctionObject*)callee_ptr)->function(this, *invokeas, call_ptr, arg_count);

		if (!throwing) {
			frames.pop_back();
			updateLocals();
		}
		return true;
	}
	case OBJECT_BOUND_FUNCTION: {
		auto callee = ((BoundFunctionObject*)callee_ptr);

		callee_ptr = callee->function;
		*invokeas = callee->thisValue;

		goto call_type_switch;
	}
	default:
		this->throwValue(GEN_TERROR("Callee is not callable"));
	}

	return false;
}

void VM::throwValue(const Value& v) {
	// When a value is thrown, there is two main things to do
	// 1. Find the nearest catch block, and
	// 2. Jump to the catch block
	//
	// While jumping back to the catch block,
	// we must also:
	// 1. Remove the frames on the way
	// 2. Exit foreach loops & remove the iteration object

	if (exc_jumps.empty()) {
		if (IS_OBJECT(v) && AS_OBJECT(v)->type == OBJECT_EXCEPTION) {
			throw v;
		}
		else {
			throw this->uncatchedException();
		}
	}

	if (throwing) {
		printf("[WARNING] An exception was already thrown.\n");
	}
	throwing = true;

	const auto& catcher = this->exc_jumps.top();

	// Search for the frame holding the try-catch block

	while (frames.back().isNativeFrame() || frames.back().stack_begin > catcher.second) {
		while (
			foreach_loops.top().stack_id >
			max(catcher.second, frames.back().stack_begin) - stackPtr
			) {
			const auto info = foreach_loops.top(); foreach_loops.pop();
			delete (IterationObject*)AS_OBJECT(frames.back().stack_begin[info.var_id - 1]);
		}
		if (!frames.back().isNativeFrame())
			closureCalls.pop_back();
		frames.pop_back();
	}

	updateLocals();


	stack_end = catcher.second;
	*++stack_end = v;

	ops = catcher.first;
}

VMExecutionResult VM::interpret() {
	if (frames.back().isNativeFrame())
		return EXEC_HALTED_BY_INTERNAL;

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
			ClosureObject* closure = new ClosureObject(func), * parent_closure = (ClosureObject*)closureCalls.back();

			closure->prototype = this->pool->nativeBase->functionPrototype;
			this->pool->registerObject(closure);

			// Capture upvalues
			for (int i = 0; i < func->upvalue_count; ++i) {
				int capture_pos = READ_USHORT(ops);
				bool isLocal = READ_UCHAR(ops);

				if (isLocal) {
					closure->upvalues[i] = this->pool->addUpvalue(stack_begin + capture_pos);
				}
				else {
					closure->upvalues[i] = parent_closure->upvalues[capture_pos];
				}
			}

			*++stack_end = OBJ_VAL(closure);
			continue;
		}

						// Unary
		case OP_NOT:
			switch (TValue::vtype(*stack_end)) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(!AS_NUMBER(*stack_end));
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(!AS_NUMBER(*stack_end));
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
			AS_RNUMBER(*stack_end) = -AS_NUMBER(*stack_end);
			continue;
		case OP_INCREASE:
		{
			Value& a = stack_begin[READ_USHORT(ops)];
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid increment operand"));
				continue;
			}
			++AS_RNUMBER(a);
			continue;
		}
		case OP_DECREASE:
		{
			Value& a = stack_begin[READ_USHORT(ops)];
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid decrement operand"));
				continue;
			}
			--AS_RNUMBER(a);
			continue;
		}

		// Special
		case OP_IS_PROTOTYPE: {
			Value& b = *(stack_end--), & a = *stack_end;

			if (!IS_OBJECT(*stack_end) or !IS_OBJECT(b)) {
				*stack_end = pool->ValueConstFalse;
				continue;
			}

			Object* obja = AS_OBJECT(a), * objb = AS_OBJECT(b)->prototype;
			if (!objb) {
				*stack_end = pool->ValueConstFalse;
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
				*stack_end = NUM_VAL(AS_NUMBER(*stack_end) + AS_NUMBER(b));
			}
			else if (IS_STRINGOBJ(*stack_end) and IS_STRINGOBJ(b)) {
				*stack_end = OBJ_VAL(pool->internStr(*AS_STRING(*stack_end) + *AS_STRING(b)));
			}
			else {
				this->throwValue(GEN_TERROR("Operation + applied on invalid value types"));
			}
			continue;
		}
		case OP_SUB:
		{
			Value& b = *stack_end--;

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = NUM_VAL(AS_NUMBER(*stack_end) - AS_NUMBER(b));
			}
			else {
				this->throwValue(GEN_TERROR("Operation - applied on invalid value types"));
			}
			continue;
		}
		case OP_MUL:
		{
			Value& b = *stack_end--;

			if (IS_NUMBER(*stack_end) and IS_NUMBER(b)) {
				*stack_end = NUM_VAL(AS_NUMBER(*stack_end) * AS_NUMBER(b));
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
				*stack_end = NUM_VAL(AS_NUMBER(*stack_end) / AS_NUMBER(b));
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
				*stack_end = NUM_VAL(pow(AS_NUMBER(*stack_end), AS_NUMBER(b)));
			}
			else {
				this->throwValue(GEN_TERROR("Operation ^ applied on invalid value types"));
			}
			continue;
		};

		// Comparison
		case OP_EQUAL:
		{
			Value& b = *(stack_end--);

#ifdef NAN_BOXING
			*stack_end = TO_BOOLEAN(Value_CMP_EQU(*stack_end, b));
			continue;
#else
			Value& a = *stack_end;

			if (TValue::vtype(b) != TValue::vtype(a)) {
				*stack_end = pool->ValueConstFalse;
				continue;
			}

			switch (TValue::vtype(b)) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(AS_NUMBER(b) == AS_NUMBER(a));
				continue;
			case VAL_NULL:
				*stack_end = pool->ValueConstTrue;
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(AS_BOOLEAN(b) == AS_BOOLEAN(a));
				continue;
			case VAL_OBJ:
				*stack_end = TO_BOOLEAN(AS_OBJECT(b) == AS_OBJECT(a));
				continue;
			default:
				// Unreachable
				throw exception("Corrupted Value");
			}
#endif
		}
		case OP_NOT_EQUAL:
		{
			Value& b = *(stack_end--);

#ifdef NAN_BOXING
			*stack_end = TO_BOOLEAN(Value_CMP_NEQ(*stack_end, b));
			continue;
#else
			Value& a = *stack_end;

			if (TValue::vtype(b) != TValue::vtype(a)) {
				*stack_end = pool->ValueConstTrue;
				continue;
			}

			switch (TValue::vtype(b)) {
			case VAL_NUM:
				*stack_end = TO_BOOLEAN(AS_NUMBER(b) != AS_NUMBER(a));
				continue;
			case VAL_NULL:
				*stack_end = pool->ValueConstFalse;
				continue;
			case VAL_BOOL:
				*stack_end = TO_BOOLEAN(AS_BOOLEAN(b) != AS_BOOLEAN(a));
				continue;
			case VAL_OBJ:
				*stack_end = TO_BOOLEAN(AS_OBJECT(b) != AS_OBJECT(a));
				continue;
			default:
				// Unreachable
				throw exception("Corrupted Value");
		}
#endif
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

			switch (TValue::vtype(*stack_end)) {
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

			switch (TValue::vtype(*stack_end)) {
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
		case OP_GOTO_SET_STACK: {
			const auto target = READ_USHORT(ops);
			const auto stack_slot = READ_USHORT(ops);

			stack_end = stack_begin + stack_slot;
			ops = start + target;
			continue;
		}

		case OP_ITER: {
			if (!IS_OBJECT(*stack_end)) {
				this->throwValue(GEN_TERROR("Trying to iterate non-object value"));
				continue;
			}
			*stack_end = OBJ_VAL(new IterationObject(pool->itertool->iterateObject(AS_OBJECT(*stack_end))));

			foreach_loops.push(ForLoopData{
				READ_USHORT(ops),
				READ_USHORT(ops),
				(chunk_size)(stack_end - stackPtr)
			});
			++stack_end;
			continue;
		}
		case OP_ITER_NEXT: {
			// Assuming the stack top always being IterationObject
			const auto& loop_data = foreach_loops.top();
			auto& it_obj = *(IterationObject*)AS_OBJECT(*(stack_end - 1));

			if (!it_obj.b->notsameas(*(RolIteratorBase*)it_obj.e)) {
				ops = start + loop_data.op_id;
				stack_end = stackPtr + loop_data.stack_id + 1;
				continue;
			}

			*stack_end = it_obj.b->deref();
			if (throwing)
				continue;

			it_obj.b->next_noret();

			continue;
		}
		case OP_ITER_END: {
			foreach_loops.pop();
			delete (IterationObject*)AS_OBJECT(*--stack_end);
			--stack_end;
			continue;
		}

			// Functional
		case OP_PRINT:
			TValue::print(*(stack_end--));
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
			*++stack_end = OBJ_VAL(obj);
			continue;
		}
		case OP_GEN_LIST: {
			ushort size = READ_USHORT(ops);
			ListObject* list = new ListObject(++(stack_end -= size), size);

			this->pool->registerList(list);
			*stack_end = OBJ_VAL(list);
			continue;
		}
		case OP_GEN_TUPLE: {
			ushort size = READ_USHORT(ops);
			TupleObject* tuple = new TupleObject(++(stack_end -= size), size);

			this->pool->registerTuple(tuple);
			*stack_end = OBJ_VAL(tuple);
			continue;
		}
		case OP_LIST_FROM_CONST_TUPLE: {
			const TupleObject* tup = AS_TUPLE(consts[READ_USHORT(ops)]);
			ListObject* list = new ListObject(tup->list, tup->size);

			this->pool->registerList(list);
			*++stack_end = OBJ_VAL(list);
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
			++AS_RNUMBER(a);
			continue;
		}
		case OP_DECREASE_UPVALUE: {
			Value& a = *upvalues[READ_USHORT(ops)]->location;
			if (!IS_NUMBER(a)) {
				this->throwValue(GEN_TERROR("Invalid decrement operand"));
				continue;
			}
			--AS_RNUMBER(a);
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
			this->exc_jumps.emplace(start + READ_USHORT(ops), stack_end);
			continue;
		case OP_UNFLAG_CATCH:
			this->exc_jumps.pop();
			throwing = false;
			continue;
		case OP_RAISE:
			if (frame.finishCode == VMCODE_BREAK) {
				this->throwValue(*stack_end);
				return EXEC_HALTED_BY_EXCEPTION;
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
			Object* new_inst = Object::specie_construct(callee_ptr, this);

			*vcallee_ptr = OBJ_VAL(new_inst);
			pool->registerObject(new_inst);

			this->callValue(callee_ptr, vcallee_ptr, arg_count, VMCODE_NONE, true);

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

		case OP_YIELD:
			return EXEC_HALTED_BY_YIELD;

		case OP_RETURN: {
			if (this->frames.size() == 1) {
				return EXEC_SUCCEED;
			}

			Value* ret_ptr = stack_end;
			*(stack_end = stack_begin) = *ret_ptr;

			VMControlCode finishCode = frame.finishCode;

			this->frames.pop_back();
			this->closureCalls.pop_back();

			updateLocals();

			if (finishCode) {
				return EXEC_HALTED_BY_INTERNAL;
			}
			continue;
		}
		}
	}
}

#undef LOAD_FRAME
#undef OBJECT_CONST
#undef NULL_VALUE
#undef TO_BOOLEAN

#undef READ_UCHAR
#undef READ_USHORT