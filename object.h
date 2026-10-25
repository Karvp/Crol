#pragma once

#include <unordered_map>
#include <cstring>
#include "value.h"
#include "common.h"
#include "RolIterator.h"

using namespace std;

class Chunk;
class VM;

//
// Object flag modifiers
//
#define OBJECT_ISREACHABLE(o) ((o)->flag & 0b1)
#define OBJECT_FLAG_REACHABLE(o) ((o)->flag |= 0b1)
#define OBJECT_UNFLAG_REACHABLE(o) ((o)->flag &= ~0b1)
#define OBJECT_ISITERABLE(o) ((o)->flag & 0b10)
#define OBJECT_FLAG_ITERABLE(o) ((o)->flag |= 0b10)
#define OBJECT_UNFLAG_ITERABLE(o) ((o)->flag &= ~0b10)

#define OBJECT_SPECIE_TYPE(o) ((o)->specie_type)
#define OBJECT_SET_SPECIE_TYPE(o, t) static_assert(sizeof(decltype(t)) == 1); (o)->specie_type = (t);

//
// Object converter shortcut
//
#define AS_STRING(o) AS_TOBJECT(o, StringObject)
#define AS_UPVALUE(o) AS_TOBJECT(o, UpvalueObject)
#define AS_CLOSURE(o) AS_TOBJECT(o, ClosureObject)
#define AS_FUNCTION(o) AS_TOBJECT(o, FunctionObject)
#define AS_NATIVE_FUNCTION(o) AS_TOBJECT(o, NativeFunctionObject)
#define AS_BOUND_FUNCTION(o) AS_TOBJECT(o, BoundFunctionObject)
#define AS_LIST(o) AS_TOBJECT(o, ListObject)
#define AS_TUPLE(o) AS_TOBJECT(o, TupleObject)
#define AS_SET(o) AS_TOBJECT(o, SetObject)

//
// Object type checker
//
#define IS_STRING(o) (AS_OBJECT(o)->type == OBJECT_STRING)
#define IS_UPVALUE(o) (AS_OBJECT(o)->type == OBJECT_UPVALUE)
#define IS_CLOSURE(o) (AS_OBJECT(o)->type == OBJECT_CLOSURE)
#define IS_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_FUNCTION)
#define IS_NATIVE_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_NATIVE_FUNCTION)
#define IS_BOUND_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_BOUND_FUNCTION)
#define IS_LIST(o) (AS_OBJECT(o)->type == OBJECT_LIST)
#define IS_TUPLE(o) (AS_OBJECT(o)->type == OBJECT_TUPLE)
#define IS_SET(o) (AS_OBJECT(o)->type == OBJECT_SET)


#define IS_STRINGOBJ(v) (IS_OBJECT(v) && IS_STRING(v))
#define IS_FUNCTIONOBJ(v) (IS_OBJECT(v) && IS_FUNCTION(v))
#define IS_LISTOBJ(v) (IS_OBJECT(v) && IS_LIST(v))
#define IS_TUPLEOBJ(v) (IS_OBJECT(v) && IS_TUPLE(v))
#define IS_SETOBJ(v) (IS_OBJECT(v) && IS_SET(v))
#define IS_ITERABLEOBJ(v) (IS_OBJECT(v) && AS_OBJECT(v)->type == OBJECT_ITERABLE)

#define IS_CALLABLE(v) (IS_OBJECT(v) && (IS_CLOSURE(v) || IS_NATIVE_FUNCTION(v) || IS_BOUND_FUNCTION(v)))
#define IS_DEFINED_FUNCTION(v) (IS_OBJECT(v) && (IS_CLOSURE(v) || (IS_BOUND_FUNCTION(v) && AS_TOBJECT(v, BoundFunctionObject)->function->type == OBJECT_CLOSURE)))
#define IS_ITERABLE(v) (IS_OBJECT(v) && OBJECT_ISITERABLE(AS_OBJECT(v)))

//
// Iterating
//

#define GET_ITER_SPECIE(o, iter_t) ((IterableSpecie<iter_t>*)o)
#define ITER_OBJECT(o, iter_t) (GET_ITER_SPECIE(o, iter_t)->get_iterator())
#define ITER_TUPLE(o) ITER_OBJECT(o, Value*)
#define ITER_LIST(o) ITER_TUPLE(o)
#define ITER_SET(o) ITER_OBJECT(o, rol_set_iterator)
#define ITER_STRING(o) ITER_OBJECT(o, rol_string_iterator)
#define ITER_PURE_OBJ(o) PureObjectIterable(o)
#define ITER_ITERABLE(o) ((IterableObject*)o)->rol_it
#define ITER_ANY_OBJ(o) vm->pool->itertool->iterateObject(o)
#define ITER_ANY_VOBJ(o) ITER_ANY_OBJ(AS_OBJECT(o))


// ***
//   HOW TO IMPLEMENT NEW OBJECT
// ***
// 
// - This guide will tell you what steps are required to add a new object type
//
// > Basically, there are 2 important things to do:
//	1. Defintion phase
//		+ Inherit from `Object` base class (very important)
//			-> Make your object usable by other part of the language
//		+ Declare your new type in `ObjectType` enum
//			-> Remember to set the `type` field on construction
//		(Optionally) + Set your own object name through the `name` field
//  2. Garbage Collector
//		+ Add your own object's tree iterator into `ValuePool::_GCmarkObject()`
//			-> Helps the GC know which objects are accessible from your object
//		(Optionally) + Add your object's pre-deconstructor into `ValuePool::runGC()`
//			-> Incase your object is stored somewhere else
//	3. Meta-info
//	 > This helps provide information for specific object type
//		+ Add your object size into `Object::obj_size()`
//			-> Used by the GC to track memory usage
//		(Optionally) + Add your own custom object constructor into `Object::specie_construct()`
//			-> Will be invoked by the `new` operator
//
// &&&
// That's all, enjoy
// &&&


class Object {
public:
	static Object* specie_construct(Object* target, VM* vm) noexcept;


	ObjectType type = OBJECT_OBJECT;
	uint8_t specie_type = OBJECT_OBJECT;
	FLAG_TYPE flag = 0;
	const char* name = "Object";
	dynamic_map props;

	Object* prototype = NULL;

	Object() {}
	Object(const char* name) : name(name) {}
	Object(const char* name, const dynamic_map& properties) : name(name), props(properties) {}

	virtual ~Object(){}

	// Get property of object
	// Return NULL if key not found
	inline Value* property_get(const Value& key) {
		dynamic_map::iterator it;

		if ((it = this->props.find(key)) != this->props.cend()) {
			return &it->second;
		}
		else if (this->prototype) {
			return this->prototype->property_get(key);
		}
		else {
			return NULL;
		}
	}
	// Set property of object
	inline void property_set(const Value& key, const Value& value) {
		this->props[key] = value;
	}
	// Check if object has property
	inline bool property_has(const Value& key) {
		Object* ptr = this;
		do {
			if (ptr->props.count(key))
				return true;
		} while (ptr = ptr->prototype);

		return false;
	}

	virtual Value get(const Value& key, VM* vm) {
		auto propVPtr = property_get(key);
		return propVPtr ? *propVPtr : vm->pool->ValueConstNull;
	}
	virtual void set(const Value& key, const Value& value) {
		return property_set(key, value);
	}
	virtual bool has(const Value& key) {
		return property_has(key);
	}

	constexpr size_t obj_size() const noexcept;

	virtual Object* obj_copy() const {
		return new Object(*this);
	}

	virtual void print() {
		printf("<%s object>", this->name);
	}
	virtual void print_leaf() {
		Object::print();
	}
	virtual string repr(VM* vm) {
		return string("<") + this->name + " object>";
	}
	virtual string repr_leaf(VM* vm) {
		return Object::repr(vm);
	}

	virtual constexpr size_t _talloc_size() const noexcept {
		return 0;
	}

	static NATIVE_FN_SIG(copy);
	static NATIVE_FN_SIG(has);
	static NATIVE_FN_SIG(size);
	static NATIVE_FN_SIG(keys);
	static NATIVE_FN_SIG(values);
	static NATIVE_FN_SIG(items);
	static NATIVE_FN_SIG(extendby);
};
class FrozenObject : public Object {
public:
	FrozenObject() {
		type = OBJECT_FROZEN;
	}
	FrozenObject(const char* name) {
		type = OBJECT_FROZEN;
		name = name;
	}
	FrozenObject(const char* name, const dynamic_map& properties) {
		type = OBJECT_FROZEN;
		name = name;
		props = properties;
	}

	virtual void set(const Value& key, const Value& value) {}

	virtual void print() {
		printf("<%s (frozen)object>", this->name);
	}
	string repr(VM* vm) override {
		string r("{");

		switch (props.size()) {
		case 0:
			break;
		case 1: {
			const auto& pr = *props.begin();
			r.append(TValue::repr_leaf(pr.first, vm));
			r.append(": ");
			r.append(TValue::repr_leaf(pr.second, vm));
			break;
		}
		default: {
			auto ptr = props.begin(), eptr = props.end();
			r.append(TValue::repr_leaf(ptr->first, vm));
			r.append(": ");
			r.append(TValue::repr_leaf(ptr->second, vm));

			while (++ptr != eptr) {
				r.append(", ");
				r.append(TValue::repr_leaf(ptr->first, vm));
				r.append(": ");
				r.append(TValue::repr_leaf(ptr->second, vm));
			}
			break;
		}
		}

		r.push_back('}');

		return r;
	}

	Object* obj_copy() const override {
		return new FrozenObject(*this);
	}
};

class PureObjectIterable {
public:
	Object* obj;

	PureObjectIterable(Object* obj) : obj(obj) {}
	PureObjectIterable(const Object* obj) : obj((Object*)obj) {}

	auto begin() const noexcept {
		return obj->props.begin();
	}

	auto end() const noexcept {
		return obj->props.end();
	}
};

// Special abstract object type that supports iterating through itself
template <typename T>
class IterableSpecie : public Object {
public:
	IterableSpecie() {
		OBJECT_FLAG_ITERABLE(this);
	}

	virtual T iter_begin() const noexcept = 0;
	virtual T iter_end() const noexcept = 0;
	virtual inline size_t iter_size() const noexcept {
		return UNSIZED_ITERABLE;
	}

	struct iterator {
		IterableSpecie* spec;

		iterator(IterableSpecie* spec) : spec(spec) {}
		iterator(const IterableSpecie* spec) : spec((IterableSpecie*)spec) {}

		T begin() const noexcept {
			return spec->iter_begin();
		}

		T end() const noexcept {
			return spec->iter_end();
		}
	};

	iterator get_iterator() const noexcept {
		return IterableSpecie::iterator(this);
	}
};

class IterationObject {
public:
	RolIteratorBase* b = NULL;

	ObjectType type = OBJECT_ITERATION; // addr of type flag is +0x8 relative to the object addr

	RolIteratorBase* e = NULL;

	IterationObject(const RolIterable& rolit) {
		const size_t it_size = rolit.begin().it_size;

		b = (RolIteratorBase*)malloc(it_size);
		if (!b) throw exception();
		memcpy(b, rolit.begin().it, it_size);

		e = (RolIteratorBase*)malloc(it_size);
		if (!e) throw exception();
		memcpy(e, rolit.end().it, it_size);
	}
};

class IterableObject : public IterableSpecie<RolIterator> {
public:
	RolIterable rol_it;
	
	RolIterator iter_begin() const noexcept override {
		return rol_it.begin();
	}
	RolIterator iter_end() const noexcept override {
		return rol_it.end();
	}

	IterableObject(const RolIterable& rolit, ValuePool* pool) : rol_it(rolit) {
		this->type = OBJECT_ITERABLE;
		this->name = "Iterable";

		Object::property_set(toValue(pool->finishedString), false);
	}
	~IterableObject() override {}

	void set(const Value& key, const Value& value) override {}

	static NATIVE_FN_SIG(next);
	static NATIVE_FN_SIG(feed);
};

class StringObject : public IterableSpecie<rol_string_iterator> {
public:
	string content;

	rol_string_iterator iter_begin() const noexcept override {
		return content.begin();
	}

	rol_string_iterator iter_end() const noexcept override {
		return content.end();
	}

	inline size_t iter_size() const noexcept override {
		return content.size();
	}

	StringObject(const char* data) : content(data) {
		this->type = OBJECT_STRING;
		this->name = "String";

		if (auto lengthStr = ValuePool::getLengthStr()) {
			Object::property_set(toValue(lengthStr), NUM_VAL(static_cast<double>(content.size())));
		}
	}
	StringObject(const string str) : StringObject(str.c_str()) {}

	~StringObject() override {}

	constexpr size_t length() const noexcept {
		return content.size();
	}

	string operator+(const StringObject& obj) {
		return this->content + obj.content;
	}

	Value get(const Value& key, VM* vm) override;
	void set(const Value& key, const Value& value) override {}

	void print() override {
		printf("%s", content.c_str());
	}
	void print_leaf() override {
		printf("%s", repr_leaf(NULL).c_str());
	}
	string repr(VM* vm) override {
		string r("'");
		Token::stringToRepr(content, r);
		r.push_back('\'');
		return r;
	}
	virtual string repr_leaf(VM* vm) {
		return repr(vm);
	}

	constexpr size_t _talloc_size() const noexcept override {
		return content.capacity() * sizeof(string::value_type);
	}

	Object* obj_copy() const override {
		return new StringObject(*this);
	}

	static NATIVE_FN_SIG(repeat);
	static NATIVE_FN_SIG(substr);
	static NATIVE_FN_SIG(reverse);

	static NATIVE_FN_SIG(find);
	static NATIVE_FN_SIG(rfind);
	
	static NATIVE_FN_SIG(split);
	static NATIVE_FN_SIG(join);
	static NATIVE_FN_SIG(replace);

	static NATIVE_FN_SIG(count);
};

class UpvalueObject : public Object {
public:
	Value* location;
	Value closed;

	UpvalueObject(Value* loc) : location(loc) {
		this->name = "Upvalue";
		this->type = OBJECT_UPVALUE;
	}

	Object* obj_copy() const override {
		return new UpvalueObject(*this);
	}
};

// A specie of callable objects
class CallableSpecie : public Object {
public:
	virtual constexpr const char* getFuncName() const noexcept = 0;
	virtual constexpr Chunk* getChunk() const noexcept = 0;

	constexpr vector<Value>& getConsts() noexcept;
	constexpr vector<uint8_t>& getOPs() noexcept;
	constexpr ValuePool* getPool() const noexcept;
	virtual constexpr int countUpvalues() const noexcept {
		return 0;
	}
	virtual constexpr int getArity() const noexcept {
		return INT32_MAX;
	}
};

class FunctionObject : public CallableSpecie {
public:
	const char* fnname;
	Chunk* chunk;
	chunk_size arity, upvalue_count = 0;
	bool is_generator = false;

	FunctionObject(const char* name, Chunk* chunk, chunk_size arity) : chunk(chunk), arity(arity) {

		this->name = "Function";
		this->type = OBJECT_FUNCTION;

		if (!*name) {
			fnname = "(anonymous)";
		}
		else {
			size_t _s = strlen(name);
			fnname = new char[_s + 1];
			memcpy((void*)fnname, name, _s);
			((char*)fnname)[_s] = '\0';
		}
	}

	~FunctionObject() override {
		delete[] fnname;
	}

	void print() override {
		printf("<%s function>", this->fnname);
	}
	void print_leaf() override {
		FunctionObject::print();
	}
	string repr(VM* vm) override {
		return string("<") + this->fnname + " function>";
	}
	string repr_leaf(VM* vm) override {
		return FunctionObject::repr(vm);
	}

	Object* obj_copy() const override {
		return new FunctionObject(*this);
	}

	size_t _talloc_size() const noexcept override;


	// Callable specie characteristics

	constexpr const char* getFuncName() const noexcept {
		return this->fnname;
	}
	constexpr Chunk* getChunk() const noexcept {
		return this->chunk;
	}
	constexpr int countUpvalues() const noexcept override {
		return this->upvalue_count;
	}
	constexpr int getArity() const noexcept override {
		return this->arity;
	}

	static NATIVE_FN_SIG(name);
	static NATIVE_FN_SIG(arity);
	static NATIVE_FN_SIG(bind);
	static NATIVE_FN_SIG(invoke);
	static NATIVE_FN_SIG(apply);
};
class BoundFunctionObject : public CallableSpecie {
public:
	CallableSpecie* function;
	Value thisValue;

	BoundFunctionObject(CallableSpecie* function, const Value& thisValue) : function(function), thisValue(thisValue) {
		this->type = OBJECT_BOUND_FUNCTION;
		this->name = "BoundFunction";
	}

	constexpr const char* getFuncName() const noexcept {
		return function->getFuncName();
	}
	constexpr Chunk* getChunk() const noexcept {
		return function->getChunk();
	}
	constexpr int countUpvalues() const noexcept override {
		return function->countUpvalues();
	}
	constexpr int getArity() const noexcept override {
		return function->getArity();
	}

	Object* obj_copy() const override {
		return new BoundFunctionObject(*this);
	}
};
class NativeFunctionObject : public CallableSpecie {
public:
	typedef void (*fntype)NATIVE_FN_ARG_SIG;
	fntype function;
	const char* fnname;

	NativeFunctionObject(const fntype& function, const char* fnname) : function(function), fnname(fnname) {
		this->type = OBJECT_NATIVE_FUNCTION;
		this->name = "NativeFunction";
	}

	~NativeFunctionObject() override {}

	constexpr const char* getFuncName() const noexcept {
		return fnname;
	}
	constexpr Chunk* getChunk() const noexcept {
		return NULL;
	}

	Object* obj_copy() const override {
		return new NativeFunctionObject(*this);
	}
};
class ClosureObject : public CallableSpecie {
public:
	CallableSpecie* function;
	UpvalueObject** upvalues;
	int upvalue_count;
	bool is_generator = false;

	ClosureObject(int upvalue_count) : upvalue_count(upvalue_count) {
		this->name = "Closure";
		this->type = OBJECT_CLOSURE;

		UpvalueObject** upvalues = new UpvalueObject*[upvalue_count];
		memset(upvalues, NULL, VAR_SIZET(upvalues) * upvalue_count);

		this->upvalues = upvalues;
		this->upvalue_count = upvalue_count;
	}

	ClosureObject(FunctionObject* function) : ClosureObject(function->countUpvalues()) {
		this->function = function;
		this->is_generator = function->is_generator;
	}

	void print() override {
		function->print();
	}
	void print_leaf() override {
		function->print_leaf();
	}
	string repr(VM* vm) override {
		return function->repr(vm);
	}
	string repr_leaf(VM* vm) override {
		return function->repr_leaf(vm);
	}

	constexpr size_t _talloc_size() const noexcept override {
		return sizet_of<decltype(upvalues)> * this->upvalue_count;
	}

	constexpr const char* getFuncName() const noexcept {
		return function->getFuncName();
	}
	constexpr Chunk* getChunk() const noexcept {
		return function->getChunk();
	}
	constexpr int countUpvalues() const noexcept override {
		return function->countUpvalues();
	}
	constexpr int getArity() const noexcept override {
		return function->getArity();
	}

	Object* obj_copy() const override {
		auto obj = new ClosureObject(*this);
		obj->upvalues = new UpvalueObject * [this->upvalue_count];
		memcpy(obj->upvalues, this->upvalues, sizet_of<decltype(upvalues)> *this->upvalue_count);
		return obj;
	}

	~ClosureObject() override {
		delete[] this->upvalues;
	}
};


class ListObject : public IterableSpecie<Value*> {
public:
	vector<Value> list;

	Value* iter_begin() const noexcept override {
		return (Value*)&list.front();
	}

	Value* iter_end() const noexcept override {
		return (Value*)&list.back() + 1;
	}

	inline size_t iter_size() const noexcept override {
		return list.size();
	}

	ListObject() {
		this->type = OBJECT_LIST;
	}

	ListObject(size_t length) : list(length) {
		this->type = OBJECT_LIST;
	}

	ListObject(Value* start_ptr, size_t length) : list(start_ptr, start_ptr + length) {
		this->type = OBJECT_LIST;
	}

	~ListObject() override {}

	constexpr size_t _talloc_size() const noexcept override {
		return list.capacity() * sizeof(decltype(list)::value_type);
	}

	constexpr size_t size() const noexcept {
		return list.size();
	}

	constexpr void push(const Value& v) {
		list.push_back(v);
	}

	Value get(const Value& key, VM* vm) override {
		if (IS_NUMBER(key)) {
			double num = AS_NUMBER(key);
			if (num >= 0 && num < list.size()) {
				return list[(size_t)num];
			}
			return NULL_VAL;
		}

		return Object::get(key, vm);
	}

	void set(const Value& key, const Value& value) override {
		if (IS_NUMBER(key)) {
			double num = AS_NUMBER(key);
			if (num >= 0 && num < list.size()) {
				list[(size_t)num] = value;
			}
			return;
		}

		Object::set(key, value);
	}

	void print() override {
		switch (list.size()) {
		case 0:
			printf("[]");
			return;
		case 1:
			printf("[");
			TValue::print_leaf(list[0]);
			printf("]");
			return;
		default: {
			auto ptr = list.begin(), eptr = list.end();
			printf("[");
			TValue::print_leaf(*ptr);
			while (++ptr != eptr) {
				printf(", ");
				TValue::print_leaf(*ptr);
			}
			printf("]");
			return;
		}
		}
	}
	string repr(VM* vm) override {
		string r("[");

		switch (list.size()) {
		case 0:
			r.push_back(']');
			break;
		case 1:
			r.append(TValue::repr_leaf(list[0], vm));
			r.push_back(']');
			break;
		default: {
			auto ptr = list.begin(), eptr = list.end();
			r.append(TValue::repr_leaf(*ptr, vm));
			while (++ptr != eptr) {
				r.append(", ");
				r.append(TValue::repr_leaf(*ptr, vm));
			}
			r.push_back(']');
			break;
		}
		}

		return r;
	}

	Object* obj_copy() const override {
		return new ListObject(*this);
	}

	static NATIVE_FN_SIG(list_size);

	static NATIVE_FN_SIG(list_push);
	static NATIVE_FN_SIG(list_insert);
	static NATIVE_FN_SIG(list_extend);
	static NATIVE_FN_SIG(list_extend_at);

	static NATIVE_FN_SIG(list_pop);
	static NATIVE_FN_SIG(list_remove);

	static NATIVE_FN_SIG(list_swap);
	static NATIVE_FN_SIG(list_clear);

	static NATIVE_FN_SIG(list_find);
	static NATIVE_FN_SIG(list_findsub);
	static NATIVE_FN_SIG(list_count);
	static NATIVE_FN_SIG(list_any);
	static NATIVE_FN_SIG(list_all);

	static NATIVE_FN_SIG(list_replace);
	static NATIVE_FN_SIG(list_replaceif);
	static NATIVE_FN_SIG(list_accumulate);

	static NATIVE_FN_SIG(list_each);
	static NATIVE_FN_SIG(list_map);
	static NATIVE_FN_SIG(list_filter);

	static NATIVE_FN_SIG(list_sort);
	static NATIVE_FN_SIG(list_reverse);
	static NATIVE_FN_SIG(list_shuffle);

	static NATIVE_FN_SIG(list_sub);
	static NATIVE_FN_SIG(list_cycle);
};
class TupleObject : public IterableSpecie<Value*> {
public:
	Value* list = NULL;
	size_t size = 0;

	Value* iter_begin() const noexcept override {
		return list;
	}

	Value* iter_end() const noexcept override {
		return list + size;
	}

	inline size_t iter_size() const noexcept override {
		return size;
	}

	TupleObject() {
		this->type = OBJECT_TUPLE;
		this->name = "Tuple";
	}

	TupleObject(size_t length) : TupleObject() {
		list = new Value[size = length];
	}

	TupleObject(Value* start_ptr, size_t length) : TupleObject(length) {
		memcpy(list, start_ptr, size * sizet_of<decltype(list)>);
	}

	~TupleObject() override {
		if (list) {
			delete[] list;
		}
	}

	constexpr size_t _talloc_size() const noexcept override {
		return size * sizet_of<decltype(list)>;
	}

	Value get(const Value& key, VM* vm) override {
		if (IS_NUMBER(key)) {
			double num = AS_NUMBER(key);
			if (num >= 0 && num < size) {
				return *(list + (size_t)num);
			}
			return NULL_VAL;
		}

		return Object::get(key, vm);
	}

	void set(const Value& key, const Value& value) override {}

	void print() override {
		switch (size) {
		case 0:
			printf("()");
			return;
		case 1:
			printf("(");
			TValue::print_leaf(*list);
			printf(",)");
			return;
		default: {
			auto ptr = list, eptr = list + size;
			printf("(");
			TValue::print_leaf(*ptr);
			while (++ptr != eptr) {
				printf(", ");
				TValue::print_leaf(*ptr);
			}
			printf(")");
			return;
		}
		}
	}
	string repr(VM* vm) override {
		string r("(");

		switch (size) {
		case 0:
			r.push_back(')');
			break;
		case 1:
			r.append(TValue::repr_leaf(*list, vm));
			r.append(",)");
			break;
		default: {
			auto ptr = list, eptr = list + size;
			r.append(TValue::repr_leaf(*ptr, vm));
			while (++ptr != eptr) {
				r.append(", ");
				r.append(TValue::repr_leaf(*ptr, vm));
			}
			r.push_back(')');
			break;
		}
		}

		return r;
	}

	Object* obj_copy() const override {
		auto obj = new TupleObject(*this);
		obj->list = new Value[size];
		memcpy(obj->list, list, sizet_of<decltype(list)> * size);
		return obj;
	}

	void resize(const size_t& size) {
		this->size = size;
		list = REALLOCATE(list, size);
	}

	static NATIVE_FN_SIG(tuple_size);
	static NATIVE_FN_SIG(tuple_find);
	static NATIVE_FN_SIG(tuple_count);

	static NATIVE_FN_SIG(tuple_any);
	static NATIVE_FN_SIG(tuple_all);

	static NATIVE_FN_SIG(tuple_each);
	static NATIVE_FN_SIG(tuple_map);
	static NATIVE_FN_SIG(tuple_accumulate);
};
class SetObject : public IterableSpecie<rol_set_iterator> {
public:
	rol_set_container values;

	rol_set_iterator iter_begin() const noexcept override {
		return values.begin();
	}

	rol_set_iterator iter_end() const noexcept override {
		return values.end();
	}

	inline size_t iter_size() const noexcept override {
		return values.size();
	}

	SetObject() {
		this->type = OBJECT_SET;
		this->name = "Set";
	}
	SetObject(const rol_set_container& set) : values(set) {
		SetObject::SetObject();
	}
	SetObject(Value* list, size_t length) : SetObject() {
		values.insert(list, list + length);
	}

	size_t size() const noexcept {
		return values.size();
	}

	void set(const Value& key, const Value& value) override {}

	size_t _talloc_size() const noexcept override {
		return values.size() * sizet_of<decltype(values)::node_type>;
	}

	Object* obj_copy() const noexcept {
		return new SetObject(*this);
	}

	void print() override {
		switch (size()) {
		case 0:
			printf("{}");
			return;
		case 1:
			printf("{");
			TValue::print_leaf(*values.begin());
			printf("}");
			return;
		default: {
			auto ptr = values.begin(), eptr = values.end();
			printf("{");
			TValue::print_leaf(*ptr);
			while (++ptr != eptr) {
				printf(", ");
				TValue::print_leaf(*ptr);
			}
			printf("}");
			return;
		}
		}
	}
	string repr(VM* vm) override {
		string r("{");

		switch (size()) {
		case 0:
			r.push_back('}');
			break;
		case 1:
			r.append(TValue::repr_leaf(*values.begin(), vm));
			r.push_back('}');
			break;
		default: {
			auto ptr = values.begin(), eptr = values.end();
			r.append(TValue::repr_leaf(*ptr, vm));
			while (++ptr != eptr) {
				r.append(", ");
				r.append(TValue::repr_leaf(*ptr, vm));
			}
			r.push_back('}');
			break;
		}
		}

		return r;
	}

	static NATIVE_FN_SIG(set_has);
	static NATIVE_FN_SIG(set_size);

	static NATIVE_FN_SIG(set_add);
	static NATIVE_FN_SIG(set_extend);

	static NATIVE_FN_SIG(set_pop);
	static NATIVE_FN_SIG(set_remove);
	static NATIVE_FN_SIG(set_discard_intersection);

	static NATIVE_FN_SIG(set_union);
	static NATIVE_FN_SIG(set_difference);
	static NATIVE_FN_SIG(set_intersection);

	static NATIVE_FN_SIG(set_issuperset);
	static NATIVE_FN_SIG(set_isdisjoint);
	static NATIVE_FN_SIG(set_issubset);
};


class ExceptionObject : public Object {
public:
	static constexpr const char* getExceptionName(const ExceptionType& exc_type) noexcept {
		switch (exc_type) {
		case EXCEPT_GENERAL_ERROR:
			return "Exception";
		case EXCEPT_RAISED_ERROR:
			return "UncatchedException";
		case EXCEPT_TYPE_ERROR:
			return "TypeErrorException";
		case EXCEPT_INTERNAL_ERROR:
			return "InternalException";
		default:
			return "UnknownException";
		}
	}

	static constexpr const char* getExceptionMessage(const ExceptionType& exc_type) noexcept {
		switch (exc_type) {
		case EXCEPT_GENERAL_ERROR:
			return "An error occurred";
		case EXCEPT_RAISED_ERROR:
			return "A value was raised but remain uncatched";
		case EXCEPT_INTERNAL_ERROR:
			return "An internal error occurred";
		default:
			return "<no_description>";
		}
	}

	const char* msg;
	const char** callframe_names;
	size_t callframe_count;
	ExceptionType exc_type;

	ExceptionObject() {}

	ExceptionObject(
		const char* msg,
		ExceptionType exc_type,
		const vector<const char*>& callframe_names
	) : exc_type(exc_type) {

		this->name = ExceptionObject::getExceptionName(exc_type);
		this->type = OBJECT_EXCEPTION;

		this->msg = msg ? msg : ExceptionObject::getExceptionMessage(exc_type);
		this->callframe_names = new const char* [callframe_count = callframe_names.size()];
		memcpy(this->callframe_names, callframe_names.data(), callframe_count * 8);
	}

	~ExceptionObject() override {
		delete[] this->callframe_names;
	}

	const void inline print_CallFrames() const noexcept {
		for (int i = 0; i < callframe_count; ++i) {
			printf(i != callframe_count - 1 ? "In [%s],\n" : "In [%s]\n\n", callframe_names[i]);
		}
	}

	virtual void print() override {
		print_CallFrames();
		printf("%s: %s\n\n", this->name, msg);
	}

	constexpr size_t _talloc_size() const noexcept override {
		return callframe_count * 8;
	}

	Object* obj_copy() const override {
		return new ExceptionObject(*this);
	}
};


constexpr size_t Object::obj_size() const noexcept {
	switch (this->type) {
	case OBJECT_OBJECT:
		return sizeof(Object);
	case OBJECT_ITERABLE:
		return sizeof(IterableObject);
	case OBJECT_FUNCTION:
		return sizeof(FunctionObject);
	case OBJECT_CLOSURE:
		return sizeof(ClosureObject);
	case OBJECT_STRING:
		return sizeof(StringObject);
	case OBJECT_UPVALUE:
		return sizeof(UpvalueObject);
	case OBJECT_NATIVE_FUNCTION:
		return sizeof(NativeFunctionObject);
	case OBJECT_LIST:
		return sizeof(ListObject);
	case OBJECT_TUPLE:
		return sizeof(TupleObject);
	case OBJECT_SET:
		return sizeof(SetObject);
	case OBJECT_BOUND_FUNCTION:
		return sizeof(BoundFunctionObject);
	case OBJECT_FROZEN:
		return sizeof(FrozenObject);
	case OBJECT_EXCEPTION:
		return sizeof(ExceptionObject);
	default:
		return UINT64_MAX;
	}
}