#pragma once

#include "chunk.h"
#include "object.h"
#include "value.h"
#include "common.h"
#include "natives.h"
#include <stack>
#include <ctime>
#include <chrono>

using namespace std;


typedef UpvalueObject** UpvalueArray;

enum VMControlCode {
	VMCODE_NONE,
	VMCODE_BREAK
};

enum VMExecutionResult {
	EXECUTION_SUCCEED,
	EXECUTION_FAILED,
	EXECUTION_HALTED_BY_EXCEPTION,
	EXECUTION_HALTED_BY_NATIVE
};

class CallFrame {
public:
	const ClosureObject* closure;
	const Value* consts;
	UpvalueArray upvalues;
	uint8_t* ops, * start;
	Value* stack_begin;
	int shift_back = 0;
	VMControlCode finishCode = VMCODE_NONE;

	CallFrame() {}

	CallFrame(ClosureObject* closure, Value* stack_begin, VMControlCode finishCode = VMCODE_NONE) :
		closure(closure),
		consts(closure->function->chunk->getConsts().data()),
		stack_begin(stack_begin),
		upvalues(closure->upvalues),
		finishCode(finishCode)
	{
		ops = start = closure->function->chunk->getOPs().data();
	}

	CallFrame& operator=(const CallFrame& cf) {
		closure = cf.closure;
		consts = cf.consts;
		upvalues = cf.upvalues;
		ops = cf.ops;
		start = cf.start;
		stack_begin = cf.stack_begin;
		shift_back = cf.shift_back;
		finishCode = cf.finishCode;

		return *this;
	}
};

class VM {
public:
	VM(ClosureObject* closure, NativeBase* nb) : closure(closure), nb(nb) {
		stack_begin = stackPtr = new Value[STACK_MAX_SIZE];
		stack_end = stackPtr - 1;

		*++stack_end = Value();
		for (auto& x : nb->getNatives()) {
			*++stack_end = x;
		}
	}

	void throwValue(const Value& v);

	void run() {
		try {
			this->newCallFrame(false, this->closure, this->stackPtr);
			this->interpret();
		}
		catch (const Value& e) {
			printf("Traceback most recent call(s):\n");
			e.print();
		}
	}

	VMExecutionResult interpret();
	void associate(ValuePool* pool) {
		pool->vm = this;
		this->pool = pool;

		pool->finalizeRegisterations();
	}

	template <typename T, typename... Args>
	T* newException(Args... args) {
		// Create callframe list
		vector<const char*> cfnames;
		for (auto i = frames.rbegin(); i != frames.rend(); ++i) {
			cfnames.push_back(i->closure->function->fnname.c_str());
		}

		auto exc = new T(args..., cfnames);
		this->pool->registerObject(exc);
		return exc;
	}

	Value uncatchedException(const Value& value) {
		return Value(newException<RaisedExceptionObject>(value));
	}

	void callValue(Object* callee_ptr, Value* call_ptr, const ushort arg_count, bool native_invocation = false, VMControlCode control_code = VMCODE_NONE);

	exception error(const char* msg) {
		printf("RuntimeError: %s", msg);
		return exception(msg);
	}

	~VM() {
		delete[] this->stackPtr;
	}

	ClosureObject* closure;
	ValuePool* pool = NULL;
	NativeBase* nb;

	// Local Variables that saves the current state of the VM
	bool throwing = false;
	UpvalueArray upvalues;
	const Value* consts;
	Value* stackPtr, * stack_begin, * stack_end;
	uint8_t* ops = NULL, * start = NULL;

	CallFrame frame;
	vector<CallFrame> frames;

	vector<pair<uint8_t*, Value*>> stack_pos;

	template <typename... Args>
	void newCallFrame(bool natively_invoked, Args... args) {
		frame = CallFrame(args...);
		if (natively_invoked) {
			frame.shift_back = 1;
		}
		frames.push_back(frame);

		ops = frame.ops;
		start = frame.start;
		upvalues = frame.upvalues;
		consts = frame.consts;
		stack_begin = frame.stack_begin;
	}
};