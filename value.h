#pragma once

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
	OBJECT_STRING,
	OBJECT_UPVALUE,
	OBJECT_CLOSURE,
	OBJECT_FUNCTION,
	OBJECT_NATIVE_FUNCTION,
	OBJECT_BOUND_FUNCTION,
	OBJECT_TUPLE,
	OBJECT_LIST,
	OBJECT_EXCEPTION,
	OBJECT_RAISED_EXCEPTION,
	OBJECT_TYPE_ERROR,
	OBJECT_INTERNAL_ERROR
};

#define AS_BOOLEAN(v) ((v).as.boolean)
#define AS_NUMBER(v) ((v).as.num)
#define AS_OBJECT(v) ((v).as.obj)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_OBJECT(value) ((value).type == VAL_OBJ)

#define IS_NUMPAIR(v1, v2) (IS_NUMBER(v1) && (v1).type == (v2).type)
#define IS_OBJPAIR(v1, v2) (IS_OBJECT(v1) && (v1).type == (v2).type)

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

class Value {
public:
	ValueType type;
	union {
		double num;
		bool boolean;
		Object* obj;
	} as;

	Value() {
		this->as.num = 0;
		this->type = VAL_NULL;
	}

	Value(double num) {
		this->as.num = num;
		this->type = VAL_NUM;
	}

	Value(bool boolean) {
		this->as.boolean = boolean;
		this->type = VAL_BOOL;
	}

	Value(Object* obj) {
		this->as.obj = obj;
		this->type = VAL_OBJ;
	}

	void print() const;


	constexpr bool operator==(const Value& a) const {
		if (this->type != a.type) {
			return false;
		}

		switch (this->type) {
		case VAL_NUM:
			return AS_NUMBER(*this) == AS_NUMBER(a);
		case VAL_NULL:
			return true;
		case VAL_BOOL:
			return AS_BOOLEAN(*this) == AS_BOOLEAN(a);
		case VAL_OBJ:
			return AS_OBJECT(*this) == AS_OBJECT(a);
		default:
			// Unreachable
			throw exception("Corrupted Value");
		}
	}

	constexpr bool operator!=(const Value& a) const {
		if (this->type != a.type) {
			return true;
		}

		switch (this->type) {
		case VAL_NUM:
			return AS_NUMBER(*this) != AS_NUMBER(a);
		case VAL_NULL:
			return false;
		case VAL_BOOL:
			return AS_BOOLEAN(*this) != AS_BOOLEAN(a);
		case VAL_OBJ:
			return AS_OBJECT(*this) != AS_OBJECT(a);
		default:
			// Unreachable
			throw exception("Corrupted Value");
		}
	}

	operator bool() const {
		switch (this->type) {
		case VAL_NUM:
			return AS_NUMBER(*this);
		case VAL_BOOL:
			return AS_BOOLEAN(*this);
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

typedef unordered_map<Value, Value> dynamic_map;

class ValuePool;

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

const size_t hashTupleRepr(const vector<Value>& v) noexcept;

// Runtime value container
class ValuePool {
public:
	ValuePool();

	// Called when the native base setup is finished
	void finalizeSetup();
	// Fired when the VM associate itself with the container
	void finalizeRegisterations();

	~ValuePool();

	//
	// |*/ Internalizing stuffs \*|
	//

	bool hasString(const string& stri) const;

	// Make a string internal (also registers the object)
	StringObject* internString(const string& stri, bool silently = false);

	// Caution: If a same string already existed, it wouldn't be deleted
	// so only use this when you're sure it is a brand new string
	StringObject* internPermanentString(const string& stri);

	// Make a string internal. It won't be deleted until the VM has fully setup
	StringObject* internNativeString(const string& str);

	// Make a tuple internal. It won't be deleted until the VM has fully setup
	TupleObject* internTupleConst(const vector<Value>& repr);


	// Get dynamic Value for the given number, creates new one if didn't existed
	Value* fromDouble(double num) {
		if (this->number_consts.find(num) != this->number_consts.end()) {
			return this->number_consts[num];
		}
		return this->number_consts[num] = new Value(num);
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
	template <typename T>
	void registerUnknownObject(T* obj) {
		if (total_size > next_gc_run) {
			runGC();
			next_gc_run = total_size * GC_HEAP_GROW_FACTOR;
		}

		// When the object is registered, it might not have appeared on the stack
		// So we should run the GC first and then will track the object
		REG_OBJ_SHORTCUT(obj, ((Object*)obj)->obj_size());
	}

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

	UpvalueObject* addUpvalue(Value* value, int capture_pos);
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
	void registerTuple(TupleObject* tuple);
	void registerList(ListObject* list);

	// reassign prototype to a certain type of object
	void assignPrototype(ObjectType type, Object* prototype);

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


	// Identical information
	VM* vm = NULL;
	Compiler* currentCompiler = NULL;
	NativeBase* nativeBase = NULL;

	// Preprocess data
	vector<Chunk*> live_chunks;
	StringObject** character_strings = NULL;
	Value* ValueConstFalse, * ValueConstTrue, * ValueConstNull;
	unordered_map<string, StringObject*> string_consts;
	unordered_map<double, Value*> number_consts;
	unordered_multimap<size_t, TupleObject*> tuple_consts;

	// Runtime data
	map<Value*, UpvalueObject*> upvalues;
	vector<Object*> objects, permanent_objects;
	size_t total_size = 0;
	size_t next_gc_run = 1024 * 5;

	// Shortcut strings
	StringObject* prototypeString;

	// Associated extensions
	random_device rd;
	mt19937 random_generator;

protected:
	inline static Object* objectPrototype = NULL;
	inline static StringObject* lengthString = NULL;

	// Note for `native_objects` vector:: will be cleared after finalization (before runtime)
	vector<Object*> grayObjects, native_objects;
	map<TupleObject*, size_t> tuple_hashes;
};