#include "natives.h"
#include "vm.h"
#include "RolIterator.h"
#include <ctime>
#include <cmath>
#include <chrono>

using namespace std;

#define CREATE_NATIVE_FN(fnsymbol, fnname) (nb->makeFunction(NATIVE_FN(fnsymbol), fnname))
#define CREATE_NATIVE_METHOD(classname, fnsymbol, fnname) (nb->makeFunction(NATIVE_METHOD(classname, fnsymbol), fnname))

#define SIMPLE_SINGLE_NUMARG_FN(name, fn) static NATIVE_FN_SIG(name) { \
if (arg_count != 1) { \
	NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected"); \
} else if (!IS_NUMBER(*arg)) { \
	NATIVE_RAISE_TERROR(vm, "First argument must be a number"); \
} \
NATIVE_RETURN(vm, fn(AS_NUMBER(*arg))); \
}


static NATIVE_FN_SIG(clock) {
	NATIVE_RETURN(vm, (double)clock() / CLOCKS_PER_SEC);
}
static NATIVE_FN_SIG(time) {
	NATIVE_RETURN(vm, (double)chrono::system_clock::to_time_t(chrono::system_clock::now()));
}
SIMPLE_SINGLE_NUMARG_FN(sqrt, sqrt);
SIMPLE_SINGLE_NUMARG_FN(round, round);
static NATIVE_FN_SIG(max) {
	if (arg_count < 2) {
		NATIVE_RAISE_TERROR(vm, "Expected at least 2 argument");
	}else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Arguments of 'max' function must be numbers");
	}

	double max = AS_NUMBER(*arg), temp;

	size_t checks = arg_count;
	while (--checks) {
		if (!IS_NUMBER(*++arg)) {
			NATIVE_RAISE_TERROR(vm, "Arguments of 'max' function must be numbers");
		}
		temp = AS_NUMBER(*arg);
		max = max > temp ? max : temp;
	}

	NATIVE_RETURN(vm, max);
}
static NATIVE_FN_SIG(min) {
	if (arg_count < 2) {
		NATIVE_RAISE_TERROR(vm, "Expected at least 2 argument");
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Arguments of 'min' function must be numbers");
	}

	double min = AS_NUMBER(*arg), temp;

	size_t checks = arg_count;
	while (--checks) {
		if (!IS_NUMBER(*++arg)) {
			NATIVE_RAISE_TERROR(vm, "Arguments of 'min' function must be numbers");
		}
		temp = AS_NUMBER(*arg);
		min = min > temp ? temp : min;
	}

	NATIVE_RETURN(vm, min);
}
static NATIVE_FN_SIG(ord) {
	if (arg_count != 1 || !IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument to be a string");
	}

	const auto& str = AS_STRING(*arg)->content;
	if (str.empty()) {
		NATIVE_RAISE_TERROR(vm, "The string must not be empty");
	}

	NATIVE_RETURN(vm, (double)str.front() + 127);
}
static NATIVE_FN_SIG(chr) {
	if (arg_count != 1 || !IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected only 1 argument to be a number");
	}

	const auto charcode = AS_NUMBER(*arg);
	if (charcode < 0 || charcode > 255) {
		NATIVE_RAISE_TERROR(vm, "Invalid charcode");
	}

	NATIVE_RETURN(vm, vm->pool->character_strings[(bytecode)charcode]);
}
static NATIVE_FN_SIG(repr) {
	if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument");
	}

	NATIVE_RETURN(vm, vm->pool->internStr(TValue::repr(*arg, vm)));
}

static NATIVE_FN_SIG(Set) {
	if (vm->isInvokedByNewOperator()) {
		if (arg_count) {
			if (arg_count == 1 && IS_ITERABLE(*arg))
				vm->pool->itertool->addElements2Container(AS_OBJECT(*arg), AS_SET(thisarg));
			else
				AS_SET(thisarg)->values.insert(arg, arg + arg_count);
		}

		if (vm->throwing)
			return;

		NATIVE_RETURN_NOWRAP(vm, thisarg);
	}
	else {
		SetObject* set;

		if (arg_count) {
			if (arg_count == 1 && IS_ITERABLE(*arg)) {
				set = new SetObject();
				vm->pool->itertool->addElements2Container(AS_OBJECT(*arg), set);
			}
			else
				set = new SetObject(arg, arg_count);
		}
		else
			set = new SetObject();

		// Register the set
		vm->pool->registerSet(set);

		if (vm->throwing)
			return;

		NATIVE_RETURN(vm, set);
	}
}
static NATIVE_FN_SIG(List) {
	if (vm->isInvokedByNewOperator()) {
		if (arg_count) {
			if (arg_count == 1 && IS_ITERABLE(*arg))
				vm->pool->itertool->addElements2Container(AS_OBJECT(*arg), AS_LIST(thisarg));
			else {
				auto& list = AS_LIST(thisarg)->list;
				list.insert(list.end(), arg, arg + arg_count);
			}
		}

		if (vm->throwing)
			return;

		NATIVE_RETURN_NOWRAP(vm, thisarg);
	}
	else {
		ListObject* list;

		if (arg_count) {
			if (arg_count == 1 && IS_ITERABLE(*arg)) {
				list = new ListObject();
				vm->pool->itertool->addElements2Container(AS_OBJECT(*arg), list);
			}
			else
				list = new ListObject(arg, arg_count);
		}
		else
			list = new ListObject();

		vm->pool->registerList(list);

		if (vm->throwing)
			return;

		NATIVE_RETURN(vm, list);
	}
}
static NATIVE_FN_SIG(Tuple) {
	if (vm->isInvokedByNewOperator()) {
		if (arg_count) {
			auto tuple = AS_TUPLE(thisarg);
			if (arg_count == 1 && IS_ITERABLE(*arg)) {
				vector<Value> temp;
				vm->pool->itertool->addElements2Vec(AS_OBJECT(*arg), temp);

				if (vm->throwing)
					return;

				tuple->resize(temp.size());
				memcpy(tuple->list, &temp.front(), temp.size());
			}
			else {
				tuple->resize(arg_count);
				memcpy(tuple->list, arg, arg_count);
			}
		}

		NATIVE_RETURN_NOWRAP(vm, thisarg);
	}
	else {
		TupleObject* tuple;
		if (arg_count) {
			if (arg_count == 1 && IS_ITERABLE(*arg)) {
				vector<Value> temp;
				vm->pool->itertool->addElements2Vec(AS_OBJECT(*arg), temp);

				tuple = temp.empty() ? new TupleObject() : new TupleObject(&temp.front(), temp.size());
			}
			else
				tuple = new TupleObject(arg, arg_count);
		}
		else
			tuple = new TupleObject();

		vm->pool->registerTuple(tuple);

		if (vm->throwing)
			return;

		NATIVE_RETURN(vm, tuple);
	}
}
static NATIVE_FN_SIG(String) {
	if (arg_count > 1) {
		NATIVE_RAISE_TERROR(vm, "Expected at most 1 argument");
	}

	if (vm->isInvokedByNewOperator()) {
		// commented a line of code that frees the memory that the object claimed
		// for `new invocation`, that could be handled by garbage collector
		// could be uncommented after comfirmation that it won't affect functionality
		//
		//delete AS_OBJECT(thisarg);

		if (arg_count) {
			if (IS_STRINGOBJ(*arg)) {
				NATIVE_RETURN_NOWRAP(vm, *arg);
			}
			else {
				NATIVE_RETURN(vm, vm->pool->internStr(TValue::repr(*arg, vm)));
			}
		}
		else {
			NATIVE_RETURN(vm, vm->pool->internStr(""));
		}
	}
	else {
		if (arg_count) {
			NATIVE_RETURN(vm, vm->pool->internStr(TValue::repr(*arg, vm)));
		}
		else {
			NATIVE_RETURN(vm, vm->pool->internStr(""));
		}
	}
}


namespace NativeMathLib {
	SIMPLE_SINGLE_NUMARG_FN(ceil, ceil);
	SIMPLE_SINGLE_NUMARG_FN(floor, floor);

	// Logarithm
	SIMPLE_SINGLE_NUMARG_FN(log2, log2);
	SIMPLE_SINGLE_NUMARG_FN(log10, log10);
	static NATIVE_FN_SIG(log) {
		if (arg_count != 2) {
			NATIVE_RAISE_TERROR(vm, "2 argument is expected");
		}
		else if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "Both argument must be of type number");
		}
		NATIVE_RETURN(vm, log2(AS_NUMBER(*arg)) / log2(AS_NUMBER(*(arg + 1))));
	}

	// Triametric
	SIMPLE_SINGLE_NUMARG_FN(sin, sin);
	SIMPLE_SINGLE_NUMARG_FN(cos, cos);
	SIMPLE_SINGLE_NUMARG_FN(tan, tan);
	SIMPLE_SINGLE_NUMARG_FN(asin, asin);
	SIMPLE_SINGLE_NUMARG_FN(acos, acos);
	SIMPLE_SINGLE_NUMARG_FN(atan, atan);
};

namespace NativeInspectLib {
	// Helper Function
	static inline Chunk* NT_HELPER_FN(unwrap_callable)(Object* f) {
		switch (f->type) {
		case OBJECT_BOUND_FUNCTION:
			return ((BoundFunctionObject*)f)->getChunk();
		case OBJECT_CLOSURE:
			return ((ClosureObject*)f)->getChunk();
		case OBJECT_FUNCTION:
			return ((FunctionObject*)f)->getChunk();
		}
		return NULL;
	}
	// END
	
	// Recipe: (object) -> prototype<object>
	static NATIVE_FN_SIG(getPrototypeOf) {
		if (arg_count != 1) {
			NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
		}
		else if (!IS_OBJECT(*arg)) {
			NATIVE_RAISE_TERROR(vm, "First argument must be an object");
		}

		const auto& proto = AS_OBJECT(*arg)->prototype;
		if (proto) {
			NATIVE_RETURN(vm, proto);
		}
		else {
			NATIVE_RETURN_NULL(vm);
		}
	}

	// Recipe: (object, newprototype<object>) -> null
	static NATIVE_FN_SIG(setPrototypeOf) {
		if (arg_count != 2) {
			NATIVE_RAISE_TERROR(vm, "2 arguments are expected");
		}
		else if (!IS_OBJECT(*arg) || !IS_OBJECT(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "The first and second arguments must be of type object");
		}

		AS_OBJECT(*arg)->prototype = AS_OBJECT(*(arg + 1));
		NATIVE_RETURN_NULL(vm);
	}


	// Recipe: (object) -> number
	static NATIVE_FN_SIG(getSizeOf) {
		if (arg_count != 1) {
			NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
		}

		double vsize = sizeof(Value);
		if (IS_OBJECT(*arg)) {
			const auto& obj = AS_OBJECT(*arg);
			vsize += obj->obj_size() + obj->_talloc_size();
		}

		NATIVE_RETURN(vm, vsize);
	}

	// Recipe: (absolute_index<number>) -> value
	static NATIVE_FN_SIG(stackElem) {
		if (arg_count != 1) {
			NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
		}
		else if (!IS_NUMBER(*arg)) {
			NATIVE_RAISE_TERROR(vm, "Expected a number as argument");
		}

		auto ptr = vm->stackPtr + (size_t)AS_NUMBER(*arg);
		if (ptr < vm->stackPtr || ptr > vm->stack_end) {
			NATIVE_RAISE_TERROR(vm, "Invalid stack element index");
		}

		NATIVE_RETURN_NOWRAP(vm, *ptr);
	}

	// Recipe: () -> number
	static NATIVE_FN_SIG(stackSize) {
		NATIVE_RETURN(vm, (double)(vm->stack_end - vm->stackPtr + 1));
	}

	// Recipe: () -> null
	static NATIVE_FN_SIG(printStack) {

		for (auto it = vm->stackPtr; it <= vm->stack_end; ++it) {
			printf("%td | ", it - vm->stackPtr);
			TValue::print(*it);
			printf("\n");
		}

		NATIVE_RETURN_NULL(vm);
	}

	// Recipe: () -> null
	static NATIVE_FN_SIG(viewStack) {

		for (auto it = vm->stackPtr; it <= vm->stack_end; ++it) {
			printf("%td | %s\n", it - vm->stackPtr, TValue::vtype_name(*it));
		}

		NATIVE_RETURN_NULL(vm);
	}

	// Recipe: () -> null
	static NATIVE_FN_SIG(printCallFrames) {
		int i = 0;
		for (const auto& f : vm->frames) {
			printf("%d | <%s> (%s)\n", ++i, f.callee->getFuncName(), f.isNativeFrame() ? "nativefn" : "function");
		}

		NATIVE_RETURN_NULL(vm);
	}

	// Recipe: () -> pool_info<object>
	static NATIVE_FN_SIG(getPoolStat) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(NativeInspectLib, symbol, name)}

		const auto& pool = *vm->pool;
		
		FrozenObject* info = vm->nb->createTempObj<FrozenObject>("PoolInfo", vm->nb->createMap({
			{"total_size", NUM_VAL(pool.total_size)},
			{"object_count", NUM_VAL(pool.objects.size() + pool.permanent_objects.size())},
			{"string_count", NUM_VAL(pool.string_consts.size())},
			{"chunk_count", NUM_VAL(pool.live_chunks.size())},
			{"upvalue_count", NUM_VAL(pool.upvalues.size())},
		}));

		vm->pool->registerObject(info);

		NATIVE_RETURN(vm, (Object*)info);

#undef FN_PAIR
	}

	// Recipe: (function<object>) -> null
	static NATIVE_FN_SIG(disassemble) {
		if (arg_count != 1) {
			NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
		}
		else if (!IS_OBJECT(*arg)) {
			NATIVE_RAISE_TERROR(vm, "Expected a user-defined function as argument");
		}

		Chunk* funcobj = NativeInspectLib::NT_HELPER_FN(unwrap_callable)(AS_OBJECT(*arg));
		if (!funcobj) {
			NATIVE_RAISE_TERROR(vm, "Expected a user-defined function as argument");
		}

		TValue::print(*arg);
		funcobj->print();

		NATIVE_RETURN_NULL(vm);
	}

	// Recipe: (function<object>) -> null
	static NATIVE_FN_SIG(getConsts) {
		if (arg_count != 1) {
			NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
		}
		else if (!IS_OBJECT(*arg)) {
			NATIVE_RAISE_TERROR(vm, "Expected a user-defined function as argument");
		}

		Chunk* funcobj = NativeInspectLib::NT_HELPER_FN(unwrap_callable)(AS_OBJECT(*arg));
		if (!funcobj) {
			NATIVE_RAISE_TERROR(vm, "Expected a user-defined function as argument");
		}

		const auto& co_array = funcobj->getConsts();
		auto consts = new TupleObject((Value*)co_array.data(), co_array.size());
		vm->pool->registerTuple(consts);

		NATIVE_RETURN(vm, consts);
	}

	// Recipe: () -> function<object>
	static NATIVE_FN_SIG(getCallee) {
		NATIVE_RETURN(vm, (Object*)(++vm->frames.rbegin())->callee);
	}
}

// Utility functions

dynamic_map NativeBase::createMap(const mapping_pairs& mapping) {
	vector<pair<Value, Value>> ptrs;

	for (auto& x : mapping) {
		ptrs.push_back({ this->makeStr(x.first), x.second });
	}

	return dynamic_map(ptrs.begin(), ptrs.end());
}

// Libraries implementations

void NativeBase::loadInspectLib(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(NativeInspectLib, symbol, name)}

	FrozenObject* math_lib = nb->createTempObj<FrozenObject>("inspect_lib", nb->createMap({
		FN_PAIR("getPrototypeOf", getPrototypeOf),
		FN_PAIR("setPrototypeOf", setPrototypeOf),
		FN_PAIR("getSizeOf", getSizeOf),
		FN_PAIR("stackElem", stackElem),
		FN_PAIR("viewStack", viewStack),
		FN_PAIR("printStack", printStack),
		FN_PAIR("printCallFrames", printCallFrames),
		FN_PAIR("stackSize", stackSize),
		FN_PAIR("disassemble", disassemble),
		FN_PAIR("getConsts", getConsts),
		FN_PAIR("getCallee", getCallee),
		FN_PAIR("getPoolStat", getPoolStat),
	}));
	nb->registerObject("inspect", math_lib);

#undef FN_PAIR
}
void NativeBase::loadMathLib(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(NativeMathLib, symbol, name)}

	FrozenObject* math_lib = nb->createTempObj<FrozenObject>("math_lib", nb->createMap({
		FN_PAIR("ceil", ceil),
		FN_PAIR("floor", floor),

		FN_PAIR("log10", log10),
		FN_PAIR("log2", log2),
		FN_PAIR("log", log),

		FN_PAIR("sin", sin),
		FN_PAIR("cos", cos),
		FN_PAIR("tan", tan),
		FN_PAIR("asin", asin),
		FN_PAIR("acos", acos),
		FN_PAIR("atan", atan),
	}));
	nb->registerObject("math", math_lib);

#undef FN_PAIR
}

void NativeBase::loadLibraries(NativeBase* nb) {
	loadMathLib(nb);
	loadInspectLib(nb);
}

// Global entities

void NativeBase::loadGlobalEntities(NativeBase* nb) {
#define REG_FUNC(name, symbol) nb->registerFunction(name, NATIVE_FN(symbol));

	// Common-use functions
	REG_FUNC("clock", clock);
	REG_FUNC("time", time);
	REG_FUNC("sqrt", sqrt);
	REG_FUNC("round", round);
	REG_FUNC("max", max);
	REG_FUNC("min", min);
	REG_FUNC("ord", ord);
	REG_FUNC("chr", chr);
	REG_FUNC("repr", repr);

#undef REG_FUNC

	// Generator functions
#define REG_GEN_FUNC(name, symbol, prefix, specie_type) auto prefix##_gen = nb->registerFunction(name, NATIVE_FN(symbol)); \
nb->prefix##Init = prefix##_gen; \
OBJECT_SET_SPECIE_TYPE(prefix##_gen, (uint8_t)specie_type);


	REG_GEN_FUNC("Set", Set, set, OBJECT_SET);
	REG_GEN_FUNC("String", String, string, OBJECT_STRING);
	REG_GEN_FUNC("List", List, list, OBJECT_LIST);
	REG_GEN_FUNC("Tuple", Tuple, tuple, OBJECT_TUPLE);

#undef REG_GEN_FUNC
}

#define PROTO_DEF(name, obj_name, map) nb->name = nb->createPermObj<FrozenObject>(obj_name, nb->createMap(map));

void NativeBase::loadObjectPrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(Object, symbol, name)},

	nb->objectPrototype.props = nb->createMap({
		FN_PAIR("copy", copy)
		FN_PAIR("has", has)
		FN_PAIR("size", size)
		FN_PAIR("keys", keys)
		FN_PAIR("values", values)
		FN_PAIR("items", items)
		FN_PAIR("extendby", extendby)
	});

#undef FN_PAIR
}

void NativeBase::loadStringPrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(StringObject, symbol, name)},

	mapping_pairs map{
		FN_PAIR("repeat", repeat)
		FN_PAIR("substr", substr)
		FN_PAIR("find", find)
		FN_PAIR("rfind", rfind)
		FN_PAIR("reverse", reverse)
		FN_PAIR("split", split)
		FN_PAIR("join", join)
		FN_PAIR("count", count)
	};
	PROTO_DEF(stringPrototype, "string_prototype", map);

#undef FN_PAIR
}

void NativeBase::loadFunctionPrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(FunctionObject, symbol, name)},

	mapping_pairs map{
		FN_PAIR("bind", bind)
		FN_PAIR("arity", arity)
		FN_PAIR("apply", apply)
		FN_PAIR("invoke", invoke)
	};
	PROTO_DEF(functionPrototype, "function_prototype", map);

	// Enabling self refs
	for (auto& x : ITER_PURE_OBJ(nb->functionPrototype)) {
		AS_NATIVE_FUNCTION(x.second)->prototype = nb->functionPrototype;
	}

#undef FN_PAIR
}

void NativeBase::loadTuplePrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(TupleObject, symbol, name)},

	mapping_pairs map{
		FN_PAIR("size", tuple_size)
		FN_PAIR("find", tuple_find)
		FN_PAIR("count", tuple_count)

		FN_PAIR("any", tuple_any)
		FN_PAIR("all", tuple_all)

		FN_PAIR("each", tuple_each)
		FN_PAIR("map", tuple_map)
		FN_PAIR("accumulate", tuple_accumulate)
	};
	PROTO_DEF(tuplePrototype, "tuple_prototype", map);

#undef FN_PAIR
}

void NativeBase::loadListPrototype(NativeBase* nb) {

#define FN_PAIR(name, symbol) { name, CREATE_NATIVE_METHOD(ListObject, symbol, name) },

	mapping_pairs map{
		FN_PAIR("size", list_size) 

		FN_PAIR("push", list_push) 
		FN_PAIR("insert", list_insert)
		FN_PAIR("extend", list_extend)
		FN_PAIR("extend_at", list_extend_at)

		FN_PAIR("pop", list_pop)
		FN_PAIR("remove", list_remove)

		FN_PAIR("swap", list_swap)
		FN_PAIR("clear", list_clear)

		FN_PAIR("find", list_find)
		FN_PAIR("findsub", list_findsub)
		FN_PAIR("count", list_count)
		FN_PAIR("any", list_any) 
		FN_PAIR("all", list_all) 

		FN_PAIR("replace", list_replace)
		FN_PAIR("replaceif", list_replaceif)
		FN_PAIR("accumulate", list_accumulate)

		FN_PAIR("each", list_each) 
		FN_PAIR("map", list_map) 
		FN_PAIR("filter", list_filter)

		FN_PAIR("sort", list_sort)
		FN_PAIR("reverse", list_reverse)
		FN_PAIR("shuffle", list_shuffle)

		FN_PAIR("sub", list_sub)
		FN_PAIR("cycle", list_cycle)
	};
	PROTO_DEF(listPrototype, "list_prototype", map);

#undef FN_PAIR
}

void NativeBase::loadSetPrototype(NativeBase* nb) {

#define FN_PAIR(name, symbol) { name, CREATE_NATIVE_METHOD(SetObject, symbol, name) },

	mapping_pairs map{
		FN_PAIR("has", set_has)
		FN_PAIR("size", set_size)

		FN_PAIR("add", set_add)
		FN_PAIR("extend", set_extend)

		FN_PAIR("pop", set_pop)
		FN_PAIR("remove", set_remove)
		FN_PAIR("remove", set_discard_intersection)

		FN_PAIR("union", set_union)
		FN_PAIR("intersection", set_intersection)
		FN_PAIR("difference", set_difference)

		FN_PAIR("issubset", set_issubset)
		FN_PAIR("issuperset", set_issuperset)
		FN_PAIR("isdisjoint", set_isdisjoint)
	};
	PROTO_DEF(setPrototype, "set_prototype", map);

#undef FN_PAIR
}

void NativeBase::loadIterablePrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(IterableObject, symbol, name)},

	mapping_pairs map{
		FN_PAIR("next", next)
		FN_PAIR("feed", feed)
	};
	PROTO_DEF(iterablePrototype, "iterable_prototype", map);

#undef FN_PAIR
}

void NativeBase::loadExceptionPrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(ExceptionObject, symbol, name)},

	mapping_pairs map{
	};
	PROTO_DEF(exceptionPrototype, "exception_prototype", map);

#undef FN_PAIR
}

#undef PROTO_DEF

void NativeBase::loadPrototypes(NativeBase* nb) {
	loadObjectPrototype(nb);
	loadStringPrototype(nb);
	loadFunctionPrototype(nb);
	loadTuplePrototype(nb);
	loadListPrototype(nb);
	loadSetPrototype(nb);
	loadIterablePrototype(nb);
	loadExceptionPrototype(nb);
}

NativeBase* NativeBase::generate(ValuePool* pool) {
	NativeBase* nb = new NativeBase(pool);

	// Load object prototypes
	NativeBase::loadPrototypes(nb);
	// Load global entities
	NativeBase::loadGlobalEntities(nb);
	// Load built-in libraries
	NativeBase::loadLibraries(nb);
	return nb;
}