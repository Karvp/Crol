#include "natives.h"
#include "vm.h"
#include <ctime>
#include <cmath>
#include <chrono>
//#include <algorithm>

using namespace std;

#define CREATE_NATIVE_FN(fnsymbol) (nb->makeFunction(NATIVE_FN(fnsymbol)))
#define CREATE_NATIVE_METHOD(classname, fnsymbol) (nb->makeFunction(NATIVE_METHOD(classname, fnsymbol)))

#define SIMPLE_SINGLE_NUMARG_FN(name, fn) static NATIVE_FN_SIG(name) { \
if (arg_count != 1) { \
	NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected")); \
} else if (!IS_NUMBER(*arg)) { \
	NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a number")); \
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected at least 2 argument"));
	}else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Arguments of 'max' function must be numbers"));
	}

	double max = AS_NUMBER(*arg), temp;

	size_t checks = arg_count;
	while (--checks) {
		if (!IS_NUMBER(*++arg)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Arguments of 'max' function must be numbers"));
		}
		temp = AS_NUMBER(*arg);
		max = max > temp ? max : temp;
	}

	NATIVE_RETURN(vm, max);
}
static NATIVE_FN_SIG(min) {
	if (arg_count < 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected at least 2 argument"));
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Arguments of 'min' function must be numbers"));
	}

	double min = AS_NUMBER(*arg), temp;

	size_t checks = arg_count;
	while (--checks) {
		if (!IS_NUMBER(*++arg)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Arguments of 'min' function must be numbers"));
		}
		temp = AS_NUMBER(*arg);
		min = min > temp ? temp : min;
	}

	NATIVE_RETURN(vm, min);
}

static NATIVE_FN_SIG(ord) {
	if (arg_count != 1 || !IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected only 1 argument to be a string"));
	}

	const auto& str = AS_STRING(*arg)->content;
	if (str.empty()) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "The string must not be empty"));
	}

	NATIVE_RETURN(vm, (double)str.front() + 127);
}
static NATIVE_FN_SIG(chr) {
	if (arg_count != 1 || !IS_NUMBER(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected only 1 argument to be a number"));
	}

	const auto charcode = AS_NUMBER(*arg);
	if (charcode < 0 || charcode > 255) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Invalid charcode"));
	}

	NATIVE_RETURN(vm, vm->pool->character_strings[(uint8_t)charcode]);
}


namespace NativeMathLib {
	SIMPLE_SINGLE_NUMARG_FN(ceil, ceil);
	SIMPLE_SINGLE_NUMARG_FN(floor, floor);

	// Logarithm
	SIMPLE_SINGLE_NUMARG_FN(log2, log2);
	SIMPLE_SINGLE_NUMARG_FN(log10, log10);
	static NATIVE_FN_SIG(log) {
		if (arg_count != 2) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "2 argument is expected"));
		}
		else if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Both argument must be of type number"));
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

	// Recipe: (object) -> prototype<object>
	static NATIVE_FN_SIG(getPrototypeOf) {
		if (arg_count != 1) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected"));
		}
		else if (!IS_OBJECT(*arg)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be an object"));
		}

		const auto& proto = AS_OBJECT(*arg)->prototype;
		if (proto) {
			NATIVE_RETURN(vm, proto);
		}
		else {
			NATIVE_RETURN(vm);
		}
	}

	// Recipe: (object, newprototype<object>) -> null
	static NATIVE_FN_SIG(setPrototypeOf) {
		if (arg_count != 2) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "2 arguments are expected"));
		}
		else if (!IS_OBJECT(*arg) || !IS_OBJECT(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "The first and second arguments must be of type object"));
		}

		AS_OBJECT(*arg)->prototype = AS_OBJECT(*(arg + 1));
		NATIVE_RETURN(vm);
	}


	// Recipe: (object) -> number
	static NATIVE_FN_SIG(getSizeOf) {
		if (arg_count != 1) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument only"));
		}

		double vsize = sizeof(Value);
		if (IS_OBJECT(*arg)) {
			const auto& obj = AS_OBJECT(*arg);
			vsize += obj->obj_size() + obj->_talloc_size();
		}

		NATIVE_RETURN(vm, vsize);
	}
}

// Utility functions

typedef vector<pair<const char*, Value>> mapping_pairs;

Object::property_map NativeBase::createMap(const mapping_pairs& mapping) {
	vector<pair<Value, Value>> ptrs;

	for (auto& x : mapping) {
		ptrs.push_back({ this->makeStr(x.first), x.second });
	}

	return Object::property_map(ptrs.begin(), ptrs.end());
}


void NativeBase::loadInspectLib(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(NativeInspectLib, symbol)}

	FrozenObject* math_lib = nb->createTemporaryObject<FrozenObject>("inspect_lib", nb->createMap({
		FN_PAIR("getPrototypeOf", getPrototypeOf),
		FN_PAIR("setPrototypeOf", setPrototypeOf),
		FN_PAIR("getSizeOf", getSizeOf),
	}));
	nb->registerObject("inspect", math_lib);

#undef FN_PAIR
}

void NativeBase::loadMathLib(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(NativeMathLib, symbol)}

	FrozenObject* math_lib = nb->createTemporaryObject<FrozenObject>("math_lib", nb->createMap({
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

#undef REG_FUNC
}

#define PROTO_DEF(name, obj_name, map) nb->name = nb->createPermanentObject<FrozenObject>(obj_name, nb->createMap(map));

void NativeBase::loadObjectPrototype(NativeBase* nb) {
	nb->objectPrototype.props = nb->createMap({
		{"copy", CREATE_NATIVE_METHOD(Object, copy)},
		{"hasName", CREATE_NATIVE_METHOD(Object, has)}
	});
}

void NativeBase::loadStringPrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(StringObject, symbol)},

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
	mapping_pairs map{
		{"bind", CREATE_NATIVE_FN(function_bind)},
		{"arity", CREATE_NATIVE_FN(function_arity)},
		{"apply", CREATE_NATIVE_FN(function_apply)},
		{"invoke", CREATE_NATIVE_FN(function_invoke)}
	};
	PROTO_DEF(functionPrototype, "function_prototype", map);

	// Enabling self refs
	for (auto& x : *nb->functionPrototype) {
		AS_NATIVE_FUNCTION(x.second)->prototype = nb->functionPrototype;
	}
}

void NativeBase::loadTuplePrototype(NativeBase* nb) {
#define FN_PAIR(name, symbol) {name, CREATE_NATIVE_METHOD(TupleObject, symbol)},

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

#define FN_PAIR(name, symbol) { name, CREATE_NATIVE_METHOD(ListObject, symbol) },

	mapping_pairs map{
		FN_PAIR("size", list_size) 

		FN_PAIR("push", list_push) 
		FN_PAIR("pop", list_pop) 
		FN_PAIR("swap", list_swap)
		FN_PAIR("clear", list_clear)

		FN_PAIR("find", list_find) 
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
	};
	PROTO_DEF(listPrototype, "list_prototype", map);

#undef FN_PAIR
}

#undef PROTO_DEF

void NativeBase::loadPrototypes(NativeBase* nb) {
	loadObjectPrototype(nb);
	loadStringPrototype(nb);
	loadFunctionPrototype(nb);
	loadTuplePrototype(nb);
	loadListPrototype(nb);
}

NativeBase* NativeBase::generate(ValuePool* pool) {
	NativeBase* nb = new NativeBase(pool);

	// Internalize some permanent strings
	for (const auto& x : { "prototype" }) {
		nb->pool->internPermanentString(x);
	}

	// Load object prototypes
	NativeBase::loadPrototypes(nb);
	// Load global entities
	NativeBase::loadGlobalEntities(nb);
	// Load built-in libraries
	NativeBase::loadLibraries(nb);

	// Finalization
	pool->finalizeSetup();
	return nb;
}