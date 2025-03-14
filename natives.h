#pragma once

#include <string>
#include <vector>
#include "object.h"

using namespace std;


class NativeBase {
public:
	// Generators

	static NativeBase* generate(ValuePool* pool);
	static void loadLibraries(NativeBase* nb);
	static void loadPrototypes(NativeBase* nb);
	static void loadGlobalEntities(NativeBase* nb);

	// Library loaders
	static void loadMathLib(NativeBase* nb);
	static void loadInspectLib(NativeBase* nb);

	// Prototype loaders
	static void loadObjectPrototype(NativeBase* nb);
	static void loadStringPrototype(NativeBase* nb);
	static void loadFunctionPrototype(NativeBase* nb);
	static void loadTuplePrototype(NativeBase* nb);
	static void loadListPrototype(NativeBase* nb);
	static void loadSetPrototype(NativeBase* nb);
	static void loadIterablePrototype(NativeBase* nb);
	static void loadExceptionPrototype(NativeBase* nb);


	// Inner definition

	NativeBase(ValuePool* pool) : pool(pool), stringPrototype(NULL), functionPrototype(NULL) {
		pool->nativeBase = this;
	}

	~NativeBase() {}

	auto registerFunction(const string& name, NativeFunctionObject::fntype func) {
		auto nt_func = AS_NATIVE_FUNCTION(this->makeFunction(func, name.c_str()));
		this->registerObject(name, nt_func);
		return nt_func;
	}

	void registerObject(const string& name, Object* obj) {
		this->registerValue(name, toValue(obj));
	}

	void registerValue(const string& name, const Value& val) {
		native_ids.push_back(name);
		native_vals.push_back(val);
	}

	const vector<string>& getNativeIDs() {
		return this->native_ids;
	}

	const vector<Value>& getNatives() {
		return this->native_vals;
	}

	template <typename T, typename... Args>
	T* createTempObj(Args... args) {
		Object* obj = new T(args...);
		
		if (obj->type == OBJECT_STRING) {
			throw exception("Object should not be a string");
		}

		this->pool->nativeObject(obj);
		return (T*)obj;
	}

	template <typename T, typename... Args>
	T* createPermObj(Args... args) {
		Object* obj = new T(args...);

		if (obj->type == OBJECT_STRING) {
			throw exception("Object should not be a string");
		}

		this->pool->permanentObject(obj);
		return (T*)obj;
	}

	dynamic_map createMap(const mapping_pairs& mapping);

	Value makeStr(const char* str) {
		return toValue(this->pool->internPermStr(str));
	}

	Value makeFunction(NativeFunctionObject::fntype func, const char* fnname) {
		Object* funcobj = this->createPermObj<NativeFunctionObject>(func, fnname);
		funcobj->prototype = this->functionPrototype;
		return toValue(funcobj);
	}

	Object& objectPrototype = *ValuePool::getObjProto();

	// Prototypes
	Object* stringPrototype, * functionPrototype, * listPrototype, * tuplePrototype, * setPrototype,
		* iterablePrototype, * exceptionPrototype;

	// Generator Functions a.k.a. constructors
	Object* stringInit, * functionInit, * listInit, * tupleInit, * setInit;
protected:
	ValuePool* pool;

	vector<string> native_ids;
	vector<Value> native_vals;
};