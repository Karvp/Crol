#pragma once

#include <bit>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <random>
#include "common.h"
#include "nodes.h"

using namespace std;

enum ValueType {
	VAL_NUM,
	VAL_BOOL,
	VAL_NULL,
	VAL_OBJ
};

enum ObjectType {
	OBJECT_OBJECT,
	OBJECT_FROZEN,
	OBJECT_ITERABLE,
	OBJECT_STRING,
	OBJECT_UPVALUE,
	OBJECT_CLOSURE,
	OBJECT_FUNCTION,
	OBJECT_NATIVE_FUNCTION,
	OBJECT_BOUND_FUNCTION,
	OBJECT_TUPLE,
	OBJECT_LIST,
	OBJECT_SET,
	OBJECT_EXCEPTION,
	OBJECT_ITERATION,
};

enum ExceptionType {
	EXCEPT_TYPE_ERROR,
	EXCEPT_INTERNAL_ERROR,
	EXCEPT_RAISED_ERROR,
	EXCEPT_GENERAL_ERROR
};

#ifdef NAN_BOXING

#define VTYPEN TValue

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN (0x7ffc000000000000ui64)

#define TAG_NULL 1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE 3 // 11.

#define FALSE_VAL (Value(QNAN | TAG_FALSE))
#define TRUE_VAL (Value(QNAN | TAG_TRUE))
#define NULL_VAL (Value(QNAN | TAG_NULL))
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define OBJ_VAL(obj) (Value)(SIGN_BIT | QNAN | (uintptr_t)(obj))
#define NUM_VAL(v) (bit_cast<Value>(v))

#define IS_NUMBER(value) ((((uint64_t)value) & QNAN) != QNAN)
#define IS_NULL(value) (((uint64_t)value) == NULL_VAL)
#define IS_BOOL(value) ((((uint64_t)value) | 1) == TRUE_VAL)
#define IS_OBJECT(value) ((((uint64_t)value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOLEAN(value) (((uint64_t)value) == TRUE_VAL)
#define AS_NUMBER(value) (bit_cast<double>(value))
#define AS_OBJECT(value) ((Object*)(((uint64_t)value) & ~(SIGN_BIT | QNAN)))

#define AS_RNUMBER(value) (*(double*)&(value))




#else

#define VTYPEN Value

#define FALSE_VAL Value(false)
#define TRUE_VAL Value(true)
#define NUM_VAL(v) Value((double)(v))
#define BOOL_VAL(v) Value((bool)(v))
#define NULL_VAL Value()
#define OBJ_VAL(v) Value((Object*)(v))

#define AS_BOOLEAN(v) ((v).as.boolean)
#define AS_NUMBER(v) ((v).as.num)
#define AS_OBJECT(v) ((v).as.obj)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_OBJECT(value) ((value).type == VAL_OBJ)

#define IS_NUMPAIR(v1, v2) (IS_NUMBER(v1) && (v1).type == (v2).type)
#define IS_OBJPAIR(v1, v2) (IS_OBJECT(v1) && (v1).type == (v2).type)

#define AS_RNUMBER(value) AS_NUMBER(value)

#endif

#define AS_TOBJECT(o, type) ((type*)AS_OBJECT(o))

class ValuePool;
class VM;
class Compiler;
class NativeBase;
class Chunk;

class Object;
class UpvalueObject;
class ClosureObject;
class StringObject;
class TupleObject;
class ListObject;
class SetObject;
class IterableObject;

class RolIterTools;

class _StructValue {
public:
	ValueType type;
	union {
		double num;
		bool boolean;
		Object* obj;
	} as;

	_StructValue() {
		this->as.num = 0;
		this->type = VAL_NULL;
	}

	_StructValue(double num) {
		this->as.num = num;
		this->type = VAL_NUM;
	}

	_StructValue(bool boolean) {
		this->as.boolean = boolean;
		this->type = VAL_BOOL;
	}

	_StructValue(Object* obj) {
		this->as.obj = obj;
		this->type = VAL_OBJ;
	}

	constexpr bool operator==(const _StructValue& a) const {
		if (this->type != a.type) {
			return false;
		}

		return !memcmp(&as, &a.as, sizeof(_StructValue::as));
	}
	constexpr bool operator!=(const _StructValue& a) const {
		if (this->type != a.type) {
			return true;
		}

		return memcmp(&as, &a.as, sizeof(_StructValue::as));
	}
	operator bool() const {
		switch (this->type) {
		case VAL_NUM:
			return this->as.num;
		case VAL_BOOL:
			return this->as.boolean;
		case VAL_NULL:
			return false;
		case VAL_OBJ:
			return true;
		default:
			// Unreachable
			throw exception("Unknown value type");
		}
	}
};

#ifdef NAN_BOXING

typedef unsigned long long Value;

struct ValueHashLessComparator {
	inline bool operator()(const Value& a, const Value& b) const noexcept {
		return a < b;
	}
};

inline constexpr Value toValue(Value v) noexcept {
	return v;
}

inline constexpr Value toValue() noexcept {
	return NULL_VAL;
}

inline constexpr Value toValue(bool val) noexcept {
	return BOOL_VAL(val);
}

inline constexpr Value toValue(Object* obj) noexcept {
	return OBJ_VAL(obj);
}

inline constexpr Value toValue(double num) noexcept {
	return NUM_VAL(num);
}

#else

typedef _StructValue Value;

struct ValueHashLessComparator {
	inline bool operator()(const Value& a, const Value& b) const noexcept {
		if (a.type < b.type)
			return true;
		else if (a.type == b.type)
			return *(size_t*)&a.as < *(size_t*)&b.as;
		else
			return false;
	}
};

template <typename... Args>
inline constexpr Value toValue(Args... args) noexcept {
	return Value(args...);
}

#endif

struct TValue {
	static void print(const Value&);
	static void print_leaf(const Value&);
	static string repr(const Value&, VM*);
	static string repr_leaf(const Value&, VM*);

	inline static ValueType vtype(const Value& v) {
#ifdef NAN_BOXING
		if (IS_NULL(v)) {
			return VAL_NULL;
		}
		else if (IS_BOOL(v)) {
			return VAL_BOOL;
		}
		else if (IS_NUMBER(v)) {
			return VAL_NUM;
		}
		else if (IS_OBJECT(v)) {
			return VAL_OBJ;
		}
		else {
			return (ValueType)255ui8;
		}
#else
		return v.type;
#endif
	}

	inline static const char* vtype_name(const Value& v) {
		switch (TValue::vtype(v)) {
		case VAL_NULL:
			return "VAL_NULL";
			break;
		case VAL_NUM:
			return "VAL_NUM";
			break;
		case VAL_BOOL:
			return "VAL_BOOL";
			break;
		case VAL_OBJ:
			return "VAL_OBJ";
			break;
		}
		return "VAL_UNDEF";
	}

	inline static bool tobool(const Value& v) {
		switch (TValue::vtype(v)) {
		case VAL_NUM:
			return AS_NUMBER(v);
		case VAL_BOOL:
			return AS_BOOLEAN(v);
		case VAL_NULL:
			return false;
		case VAL_OBJ:
			return true;
		default:
			// Unreachable
			throw exception("Unknown value type");
		}
	}
};

inline _StructValue reprValue(const Value& v) noexcept {
	switch (TValue::vtype(v)) {
	case VAL_NUM:
		return _StructValue(AS_NUMBER(v));
	case VAL_NULL:
		return _StructValue();
	case VAL_BOOL:
		return _StructValue(AS_BOOLEAN(v));
	case VAL_OBJ:
		return _StructValue(AS_OBJECT(v));
	default:
		return _StructValue();
	}
}

typedef vector<Value> rol_list_container;
typedef string::const_iterator rol_string_iterator;
typedef std::set<Value, ValueHashLessComparator> rol_set_container;
typedef rol_set_container::iterator rol_set_iterator;

typedef vector<pair<const char*, Value>> mapping_pairs;
typedef unordered_map<Value, Value> dynamic_map;
typedef map<Value, Value> dynamic_tree_map;


Value Value_ADD(const Value& a, const Value& b, ValuePool* pool, bool& status);
Value Value_SUB(const Value& a, const Value& b, bool& status);
Value Value_MUL(const Value& a, const Value& b, bool& status);
Value Value_DIV(const Value& a, const Value& b, bool& status);
Value Value_POW(const Value& a, const Value& b, bool& status);

Value Value_CMP_LESS(const Value& a, const Value& b, bool& status);
Value Value_CMP_GREATER(const Value& a, const Value& b, bool& status);
Value Value_CMP_LESS_EQ(const Value& a, const Value& b, bool& status);
Value Value_CMP_GREATER_EQ(const Value& a, const Value& b, bool& status);

Value Value_UNARY_NEGATE(const Value& a, bool& status);

constexpr bool Value_CMP_EQU(const Value& a, const Value& b) noexcept {
	if (IS_NUMBER(a) && IS_NUMBER(b)) {
		return AS_NUMBER(a) == AS_NUMBER(b);
	}
	return a == b;
}

constexpr bool Value_CMP_NEQ(const Value& a, const Value& b) noexcept {
	if (IS_NUMBER(a) && IS_NUMBER(b)) {
		return AS_NUMBER(a) != AS_NUMBER(b);
	}
	return a != b;
}

// Unchecked operations
// 
// **
// Automatically assump the values are numbers and perform the operation
// **

constexpr bool Value_CMP_LESS_Unchecked(const Value& a, const Value& b) noexcept {
	return AS_NUMBER(a) < AS_NUMBER(b);
}

constexpr bool Value_CMP_GREATER_Unchecked(const Value& a, const Value& b) noexcept {
	return AS_NUMBER(a) > AS_NUMBER(b);
}

#ifdef NAN_BOXING

template <>
struct hash<Value> {
	inline constexpr size_t operator()(const Value& v) const noexcept {
		return v;
	}
};

#else

template <>
struct hash<Value> {
	size_t operator()(const Value& v) const noexcept {
		switch (v.type) {
		case VAL_NULL:
			return 0;
		case VAL_BOOL:
			return AS_BOOLEAN(v) ? 1 : 2;
		case VAL_NUM:
			return *(size_t*)&v.as.num;
			//return hash<double>()(v.as.num);
		case VAL_OBJ:
			return hash<Object*>()(v.as.obj);
		default:
			throw exception("Un-hashable value type");
		}
	}
};

#endif

const size_t hashTupleRepr(const vector<Value>& v) noexcept;

// Runtime value container
class ValuePool {
private:
	// Fired at the end of pool construction
	void finalizeSetup();

public:
	ValuePool();

	// Fired when the VM associate itself with the container
	void finalizeRegisterations();

	~ValuePool();

	//
	// |*/ Internalizing stuffs \*|
	//

	// Make a string internal (also registers the object)
	StringObject* internStr(const string&, bool silently = false);

	// Caution: If a same string already existed, it wouldn't be deleted
	// so only use this when you're sure it is a brand new string
	StringObject* internPermStr(const string&);

	// Make a string internal. It won't be deleted until the VM has fully setup
	StringObject* internNativeStr(const string&);

	// Make a tuple internal. It won't be deleted until the VM has fully setup
	TupleObject* internTupleConst(const vector<Value>& repr);


	// Get dynamic Value for the given number, creates new one if didn't existed
	Value* fromDouble(double num) {
		if (this->number_consts.find(num) != this->number_consts.end()) {
			return this->number_consts[num];
		}
		return this->number_consts[num] = new Value(NUM_VAL(num));
	}


#define REG_OBJ_SHORTCUT(obj, struct_size) objects.push_back((Object*)obj); \
total_size += struct_size + ((Object*)obj)->_talloc_size(); \
if (!obj->prototype) { \
	obj->prototype = ValuePool::objectPrototype; \
}

	// Also register the object but doesn't trigger Garbage Collector
	template <typename T>
	void silentRegisterObject(T* obj) {
		REG_OBJ_SHORTCUT(obj, sizeof(T));
	}

	// Register object on the internal VM live object list
	template <typename T>
	void registerObject(T* obj) {
		if (total_size > next_gc_run) {
			runGC();
			next_gc_run = total_size * GC_HEAP_GROW_FACTOR;
		}

		// When the object is registered, it might not have appeared on the stack
		// So we should run the GC first and then will track the object
		REG_OBJ_SHORTCUT(obj, sizeof(T));
	}

	// Register object on the internal VM live object list without knowing its type at compile time
	void registerUnknownObject(Object* obj);

	// Register an array of objects on the internal VM live object list
	template <typename T>
	void registerObjects(T** obj, size_t count) {
		if (total_size > next_gc_run) {
			runGC();
			next_gc_run = total_size * GC_HEAP_GROW_FACTOR;
		}

		while (count--) {
			REG_OBJ_SHORTCUT(*obj, sizeof(T));
			++obj;
		}
	}

#undef REG_OBJ_SHORTCUT

	// Register permanent object
	template <typename T>
	void permanentObject(T* obj) {
		permanent_objects.push_back((Object*)obj);
		total_size += sizeof(T) + ((Object*)obj)->_talloc_size();
	}

	// Silently register native object
	template <typename T>
	void nativeObject(T* obj) {
		native_objects.push_back((Object*)obj);
		this->silentRegisterObject(obj);
	}

	void addChunk(Chunk* chunk) {
		live_chunks.push_back(chunk);
	}

	UpvalueObject* addUpvalue(Value* value);
	UpvalueObject* getUpvalue(Value* value);


	// ** Registerers **
	// 
	// These methods may add the object id onto the track and/or
	// attach prototype onto the natively generated object
	// 
	// Note:: For built-in types registerers, always set the object prototype
	// before registering it through the registerObject, or permanentObject, or nativeObject,
	// or silentRegisterObject, and registerObjects functions
	//
	void registerTuple(TupleObject* tuple, bool silently = false);
	void registerList(ListObject* list, bool silently = false);
	void registerSet(SetObject* set, bool silently = false);
	void registerIterable(IterableObject* iterb, bool silently = false);

	// reassign prototype to a certain type of object
	void assignPrototype(Object* object);

	// ---
	// Getters for static attributes
	// ---

	// getter for object's prototype
	static constexpr Object* getObjProto() noexcept {
		return ValuePool::objectPrototype;
	}
	// getter for "length" string
	static constexpr StringObject* getLengthStr() noexcept {
		return ValuePool::lengthString;
	}

	// ---
	// Garbage collector
	// --- 

	void runGC();
	void _GCmarkChunk(Chunk* chunk);
	void _GCmarkCompilerRoots();
	void _GCmarkRoots();
	void _GCmarkObject(Object* obj);


	// Associated parts
	VM* vm = NULL;
	Compiler* currentCompiler = NULL;
	NativeBase* nativeBase = NULL;

	// Preprocess data

	// List of living chunks
	vector<Chunk*> live_chunks;
	// Array of ascii characters
	StringObject** character_strings = NULL;
	// Common use constants
	Value ValueConstFalse = FALSE_VAL, ValueConstTrue = TRUE_VAL, ValueConstNull = NULL_VAL;
	// Pool of internalized strings
	unordered_map<string, StringObject*> string_consts;
	// Pool of internalized number constants
	unordered_map<double, Value*> number_consts;
	// Pool of internalized tuple constants
	unordered_multimap<size_t, TupleObject*> tuple_consts;

	// Runtime data
	size_t total_size = 0;
	size_t next_gc_run = 1024 * 5;
	map<Value*, UpvalueObject*> upvalues;
	vector<Object*> objects, permanent_objects;

	// Shortcut strings
	StringObject* prototypeString, * nullString, * falseString, * trueString, * finishedString;

	// Associated extensions
	random_device rd;
	mt19937 random_generator;
	RolIterTools* itertool = NULL;

protected:
	inline static Object* objectPrototype = NULL;
	inline static StringObject* lengthString = NULL;

	// Note for `native_objects` vector:: will be cleared after finalization (before runtime)
	vector<Object*> grayObjects, native_objects;
	map<TupleObject*, size_t> tuple_hashes;
};