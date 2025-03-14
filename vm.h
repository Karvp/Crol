#pragma once

#include "chunk.h"
#include "value.h"
#include "common.h"
#include <stack>
#include <ctime>
#include <chrono>

using namespace std;

class NativeBase;
class NativeFunctionObject;
class ClosureObject;
class ExceptionObject;
class CallableSpecie;

typedef UpvalueObject** UpvalueArray;

struct ForLoopData {
	chunk_size op_id, var_id, stack_id;
};

enum _VMControlCode {
	VMCODE_NONE,
	VMCODE_BREAK
};
typedef uint8_t VMControlCode;

enum VMExecutionResult {
	EXEC_SUCCEED,
	EXEC_FAILED,
	EXEC_HALTED_BY_EXCEPTION,
	EXEC_HALTED_BY_INTERNAL,
	EXEC_HALTED_BY_YIELD
};

class CallFrame {
public:
	const CallableSpecie* callee;
	bytecode* ops, * start;
	Value* stack_begin;
	VMControlCode finishCode = VMCODE_NONE;
	bool invoked_by_new_op = false;

	CallFrame() {}
	CallFrame(NativeFunctionObject* callee, bool new_invocation = false);
	CallFrame(ClosureObject* callee, Value* stack_begin, bool new_invocation = false, VMControlCode finishCode = VMCODE_NONE);

	constexpr bool isNativeFrame() const noexcept;
	constexpr const char* getFuncName() const noexcept;
	constexpr UpvalueArray getUpvalues() const noexcept;
	constexpr Value* getConsts() const noexcept;
};

class VM {
public:
	VM(NativeBase* nb) {
		preinit(nb);
	}

	VM(ClosureObject* callee, NativeBase* nb) {
		preinit(nb);
		initialize(callee);
	}

	void preinit(NativeBase* nb);
	void initialize(ClosureObject* callee) {
		this->callee = (CallableSpecie*)callee;
	}
	void associate(ValuePool* pool) {
		pool->vm = this;
		this->pool = pool;

		pool->finalizeRegisterations();
	}

	void run();
	VMExecutionResult interpret();

	constexpr bool isInvokedByNewOperator() const noexcept {
		return this->frames.back().invoked_by_new_op;
	}

	void throwValue(const Value& v);

	ExceptionObject* newException(const char* msg, ExceptionType exc_type = EXCEPT_GENERAL_ERROR);

	Value uncatchedException();

	// Call a function
	// If it is a native function, the execution will be done inside this function
	// Otherwise it is a bytecode function, this function will set up the environment,
	// preparing for it's interpretation
	// 
	// {return}: a boolean indicating if the interpretation has already done
	bool callValue(Object* callee_ptr, Value* call_ptr, const ushort arg_count, VMControlCode control_code = VMCODE_NONE, bool new_invocation = false);

	exception error(const char* msg) {
		printf("RuntimeError: %s", msg);
		return exception(msg);
	}

	~VM() {
		delete[] this->stackPtr;
	}


	// Utilities
	ValuePool* pool = NULL;
	NativeBase* nb = NULL;

	// VM flags
	bool throwing = false, halt_request = false, execution_finished = false;

	// Variables that shares current state of the VM
	UpvalueArray upvalues;
	const Value* consts;
	Value* stackPtr, * stack_begin, * stack_end;
	bytecode* ops = NULL, * start = NULL;

	// VM calling infos
	CallableSpecie* callee;
	vector<ClosureObject*> closureCalls;
	CallFrame frame;
	vector<CallFrame> frames;

	// Exception handler
	stack<pair<bytecode*, Value*>> exc_jumps;
	stack<ForLoopData> foreach_loops;


	void newCallFrame(const CallFrame& cf) {
		frames.push_back(frame = cf);

		if (!cf.isNativeFrame()) {
			closureCalls.push_back((ClosureObject*)cf.callee);

			ops = frame.ops;
			start = frame.start;
			upvalues = frame.getUpvalues();
			consts = frame.getConsts();
			stack_begin = frame.stack_begin;
		}
	}

	inline void updateLocals() {
		frame = frames.back();

		if (!frame.isNativeFrame()) {
			ops = frame.ops;
			start = frame.start;
			upvalues = frame.getUpvalues();
			consts = frame.getConsts();
			stack_begin = frame.stack_begin;
		}
	}

private:
	bool ready = false;
};