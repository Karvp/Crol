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


	// Inner definition

	NativeBase(ValuePool* pool) : pool(pool), stringPrototype(NULL), functionPrototype(NULL) {
		pool->nativeBase = this;
	}

	~NativeBase() {}

	void registerFunction(const string& name, NativeFunctionObject::fntype func) {
		this->registerObject(name, AS_NATIVE_FUNCTION(this->makeFunction(func)));
	}

	void registerObject(const string& name, Object* obj) {
		this->registerValue(name, Value(obj));
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
	T* createTemporaryObject(Args... args) {
		T* obj = new T(args...);

		if (dynamic_cast<Object*>(obj) == nullptr) {
			throw exception("Object is not an object");
		}
		else if (obj->type == OBJECT_STRING) {
			throw exception("Object should not be a string");
		}

		this->pool->nativeObject(obj);
		return obj;
	}

	template <typename T, typename... Args>
	T* createPermanentObject(Args... args) {
		T* obj = new T(args...);

		if (dynamic_cast<Object*>(obj) == nullptr) {
			throw exception("Object is not an object");
		}
		else if (obj->type == OBJECT_STRING) {
			throw exception("Object should not be a string");
		}

		this->pool->permanentObject(obj);
		return obj;
	}

	Object::property_map createMap(const vector<pair<const char*, Value>>& mapping);

	Value makeStr(const char* str) {
		return this->pool->internPermanentString(str);
	}

	Value makeFunction(NativeFunctionObject::fntype func) {
		Object* funcobj = this->createPermanentObject<NativeFunctionObject>(func);
		funcobj->prototype = this->functionPrototype;
		return funcobj;
	}

	Object& objectPrototype = *ValuePool::getObjProto();
	Object* stringPrototype, * functionPrototype, * listPrototype, * tuplePrototype;
protected:
	ValuePool* pool;

	vector<string> native_ids;
	vector<Value> native_vals;
};