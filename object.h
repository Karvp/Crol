#pragma once
#include <unordered_map>
#include <cstring>
#include "value.h"

using namespace std;

class Chunk;

#define AS_TOBJECT(o, type) ((type*)AS_OBJECT(o))
#define AS_STRING(o) AS_TOBJECT(o, StringObject)
#define AS_UPVALUE(o) AS_TOBJECT(o, UpvalueObject)
#define AS_CLOSURE(o) AS_TOBJECT(o, ClosureObject)
#define AS_FUNCTION(o) AS_TOBJECT(o, FunctionObject)
#define AS_NATIVE_FUNCTION(o) AS_TOBJECT(o, NativeFunctionObject)
#define AS_BOUND_FUNCTION(o) AS_TOBJECT(o, BoundFunctionObject)
#define AS_LIST(o) AS_TOBJECT(o, ListObject)
#define AS_TUPLE(o) AS_TOBJECT(o, TupleObject)

#define IS_STRING(o) (AS_OBJECT(o)->type == OBJECT_STRING)
#define IS_UPVALUE(o) (AS_OBJECT(o)->type == OBJECT_UPVALUE)
#define IS_CLOSURE(o) (AS_OBJECT(o)->type == OBJECT_CLOSURE)
#define IS_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_FUNCTION)
#define IS_NATIVE_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_NATIVE_FUNCTION)
#define IS_BOUND_FUNCTION(o) (AS_OBJECT(o)->type == OBJECT_BOUND_FUNCTION)
#define IS_LIST(o) (AS_OBJECT(o)->type == OBJECT_LIST)
#define IS_TUPLE(o) (AS_OBJECT(o)->type == OBJECT_TUPLE)

#define IS_STRINGOBJ(v) (IS_OBJECT(v) && IS_STRING(v))
#define IS_FUNCTIONOBJ(v) (IS_OBJECT(v) && IS_FUNCTION(v))
#define IS_LISTOBJ(v) (IS_OBJECT(v) && IS_LIST(v))
#define IS_TUPLEOBJ(v) (IS_OBJECT(v) && IS_TUPLE(v))

#define IS_CALLABLE(v) (IS_OBJECT(v) && (IS_CLOSURE(v) || IS_NATIVE_FUNCTION(v) || IS_BOUND_FUNCTION(v)))
#define IS_DEFINED_FUNCTION(v) (IS_OBJECT(v) && (IS_CLOSURE(v) || (IS_BOUND_FUNCTION(v) && AS_TOBJECT(v, BoundFunctionObject)->function->type == OBJECT_CLOSURE)))

template <typename T>
inline static void constructMemory(T* ptr) {
	auto memlayout = new T();
	memcpy(ptr, memlayout, sizeof(T));
	free(memlayout);
}

class VM;

class Object {
public:
	typedef dynamic_map property_map;

	ObjectType type = OBJECT_OBJECT;
	bool mark = false;
	const char* name = "Object";
	property_map props;

	Object* prototype = NULL;

	Object() {}
	Object(const char* name) : name(name) {}
	Object(const char* name, const property_map& properties) : name(name), props(properties) {}

	virtual ~Object(){}

	property_map::iterator begin() {
		return this->props.begin();
	}

	property_map::iterator end() {
		return this->props.end();
	}

	virtual Value get(const Value& key, VM* vm) {
		property_map::iterator it;

		if ((it = this->props.find(key)) != this->props.cend()) {
			return it->second;
		}

		return this->prototype ? this->prototype->get(key, vm) : Value();
	}

	virtual void set(const Value& key, const Value& value) {
		this->props[key] = value;
	}

	bool has(const Value& key) {
		Object* ptr = this;
		do {
			if (ptr->props.count(key))
				return true;
		} while (ptr = ptr->prototype);

		return false;
	}

	constexpr size_t obj_size() const noexcept;

	virtual Object* obj_copy() const {
		return new Object(*this);
	}

	template <typename T, typename... Args>
	T* create_copy(Args... args) const {
		auto obj = new T(args...);
		//this->copy_properties(this->props, obj->props);
		return obj;
	}

	virtual void print() {
		printf("<%s object>", this->name);
	}

	virtual constexpr size_t _talloc_size() const noexcept {
		return 0;
	}

	static NATIVE_FN_SIG(copy);
	static NATIVE_FN_SIG(has);
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
	FrozenObject(const char* name, const property_map& properties) {
		type = OBJECT_FROZEN;
		name = name;
		props = properties;
	}

	virtual void set(const Value& key, const Value& value) {}

	virtual void print() {
		printf("<%s (frozen)object>", this->name);
	}

	Object* obj_copy() const override {
		return new FrozenObject(*this);
	}
};

class StringObject : public Object {
public:
	string content;

	StringObject(const char* data) : content(data) {
		this->type = OBJECT_STRING;
		this->name = "String";

		content.shrink_to_fit();
		Object::set(ValuePool::getLengthStr(), Value((double)content.size()));
	}
	StringObject(const string str) : content(str) {
		this->type = OBJECT_STRING;
		this->name = "String";

		content.shrink_to_fit();
		Object::set(ValuePool::getLengthStr(), Value((double)content.size()));
	}

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
	int capture_pos;
	Value* location;
	Value closed;

	UpvalueObject(Value* loc, int capture_pos) : location(loc), capture_pos(capture_pos) {
		this->name = "Upvalue";
		this->type = OBJECT_UPVALUE;
	}

	Object* obj_copy() const override {
		return new UpvalueObject(*this);
	}
};

class FunctionObject : public Object {
public:
	string fnname;
	Chunk* chunk;
	int arity, upvalue_count;

	FunctionObject(const string& name, Chunk* chunk, int arity) : fnname(name), chunk(chunk), arity(arity), upvalue_count(0) {
		this->name = "Function";
		this->type = OBJECT_FUNCTION;

		if (name.empty()) {
			this->fnname = "(anonymous)";
		}
	}

	void print() override {
		printf("<%s function>", this->fnname.c_str());
	}

	Object* obj_copy() const override;
};

class BoundFunctionObject : public Object {
public:
	Object* function;
	Value thisValue;

	BoundFunctionObject(Object* function, const Value& thisValue) : function(function), thisValue(thisValue) {
		this->type = OBJECT_BOUND_FUNCTION;
		this->name = "BoundFunction";
	}

	Object* obj_copy() const override {
		return new BoundFunctionObject(*this);
	}
};

class NativeFunctionObject : public Object {
public:
	typedef void (*fntype)NATIVE_FN_ARG_SIG;
	fntype function;

	NativeFunctionObject(const fntype& function) : function(function) {
		this->type = OBJECT_NATIVE_FUNCTION;
		this->name = "NativeFunction";
	}

	~NativeFunctionObject() override {}

	Object* obj_copy() const override {
		return new NativeFunctionObject(*this);
	}
};

// bind function
NATIVE_FN_SIG(function_bind);
NATIVE_FN_SIG(function_arity);
NATIVE_FN_SIG(function_invoke);
NATIVE_FN_SIG(function_apply);

class ClosureObject : public Object {
public:
	FunctionObject* function;
	UpvalueObject** upvalues;
	int upvalue_count;

	ClosureObject(FunctionObject* function) : function(function) {
		this->name = "Closure";
		this->type = OBJECT_CLOSURE;

		UpvalueObject** upvalues = new UpvalueObject*[function->upvalue_count];
		memset(upvalues, NULL, base_type<decltype(upvalues)>::size * function->upvalue_count);

		this->upvalues = upvalues;
		this->upvalue_count = function->upvalue_count;
	}

	void print() override {
		this->function->print();
	}

	constexpr size_t _talloc_size() const noexcept override {
		return base_type<decltype(upvalues)>::size * this->upvalue_count;
	}

	Object* obj_copy() const override {
		auto obj = new ClosureObject(*this);
		obj->upvalues = new UpvalueObject * [this->upvalue_count];
		memcpy(obj->upvalues, this->upvalues, base_type<decltype(upvalues)>::size * this->upvalue_count);
		return obj;
	}

	~ClosureObject() override {
		delete[] this->upvalues;
	}
};

//class IterableSpecieObject : public Object {
//	IterableSpecieObject() {
//		this->type = OBJECT_ITERABLE_SPECIE;
//	}
//
//	virtual constexpr Value* begin() const noexcept = 0;
//	virtual constexpr Value* end() const noexcept = 0;
//};

class ListObject : public Object {
public:
	vector<Value> list;
	
	constexpr vector<Value>::iterator begin() noexcept {
		return list.begin();
	}

	constexpr vector<Value>::iterator end() noexcept {
		return list.end();
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
			return Value();
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
			list[0].print();
			printf("]");
			return;
		default: {
			auto ptr = list.begin();
			printf("[");
			ptr->print();
			while (++ptr != list.end()) {
				printf(", ");
				ptr->print();
			}
			printf("]");
			return;
		}
		}
	}

	Object* obj_copy() const override {
		return new ListObject(*this);
	}

	static NATIVE_FN_SIG(list_size);

	static NATIVE_FN_SIG(list_push);
	static NATIVE_FN_SIG(list_pop);
	static NATIVE_FN_SIG(list_swap);
	static NATIVE_FN_SIG(list_clear);

	static NATIVE_FN_SIG(list_find);
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
};


class TupleObject : public Object {
public:
	Value* list = NULL;
	size_t size = 0;

	constexpr Value* begin() const noexcept {
		return list;
	}

	constexpr Value* end() const noexcept {
		return list + size;
	}

	TupleObject() {
		this->type = OBJECT_TUPLE;
		this->name = "Tuple";
	}

	TupleObject(Value* start_ptr, size_t length) : size(length) {
		this->type = OBJECT_TUPLE;
		this->name = "Tuple";

		list = new Value[length];
		memcpy(list, start_ptr, length * base_type<decltype(list)>::size);
	}

	~TupleObject() override {
		if (list) {
			delete[] this->list;
		}
	}

	constexpr size_t _talloc_size() const noexcept override {
		return size * base_type<decltype(list)>::size;
	}

	Value get(const Value& key, VM* vm) override {
		if (IS_NUMBER(key)) {
			double num = AS_NUMBER(key);
			if (num >= 0 && num < size) {
				return *(list + (size_t)num);
			}
			return Value();
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
			list[0].print();
			printf(",)");
			return;
		default: {
			auto ptr = list;
			printf("(");
			ptr->print();
			while (++ptr != list + size) {
				printf(", ");
				ptr->print();
			}
			printf(")");
			return;
		}
		}
	}

	Object* obj_copy() const override {
		auto obj = new TupleObject(*this);
		obj->list = new Value[this->size];
		memcpy(obj->list, this->list, sizeof(Value) * this->size);
		return obj;
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


class ExceptionObject : public Object {
public:
	const char* msg;
	vector<const char*> callframe_names;

	ExceptionObject() {}

	ExceptionObject(const char* msg, const vector<const char*>& callframe_names) : msg(msg), callframe_names(callframe_names) {
		this->name = "Exception";
		this->type = OBJECT_EXCEPTION;
	}

	~ExceptionObject() override {}

	void print_CallFrames() {
		for (int i = 0, len = callframe_names.size(); i < len; ++i) {
			printf(i != len - 1 ? "In [%s],\n" : "In [%s]\n\n", callframe_names[i]);
		}
	}

	virtual void print() override {
		printf("%s: %s\n\n", this->name, msg);
		print_CallFrames();
	}

	constexpr size_t _talloc_size() const noexcept override {
		return callframe_names.capacity() * sizeof(decltype(callframe_names)::value_type);
	}

	Object* obj_copy() const override {
		return new ExceptionObject(*this);
	}
};

class RaisedExceptionObject : public ExceptionObject {
public:
	Value throwValue;

	RaisedExceptionObject(const Value& v, const vector<const char*>& callframe_names) : throwValue(v) {
		this->callframe_names = callframe_names;
		this->name = "RaiseException";
		this->type = OBJECT_RAISED_EXCEPTION;
	}

	virtual void print() override {
		printf("%s: Value ", this->name);
		this->throwValue.print();
		printf(" was thrown but uncatched\n\n");

		ExceptionObject::print_CallFrames();
	}

	Object* obj_copy() const override {
		return new RaisedExceptionObject(*this);
	}
};

class TypeErrorObject : public ExceptionObject {
public:
	TypeErrorObject(const char* msg, const vector<const char*>& callframe_names) {
		this->callframe_names = callframe_names;
		this->msg = msg;

		this->name = "TypeError";
		this->type = OBJECT_TYPE_ERROR;
	}

	Object* obj_copy() const override {
		return new TypeErrorObject(*this);
	}
};

class InternalErrorObject : public ExceptionObject {
public:
	InternalErrorObject(const char* msg, const vector<const char*>& callframe_names) {
		this->callframe_names = callframe_names;
		this->msg = msg;

		this->name = "TypeError";
		this->type = OBJECT_INTERNAL_ERROR;
	}

	Object* obj_copy() const override {
		return new InternalErrorObject(*this);
	}
};



constexpr size_t Object::obj_size() const noexcept {
	switch (this->type) {
	case OBJECT_OBJECT:
		return sizeof(Object);
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
	case OBJECT_BOUND_FUNCTION:
		return sizeof(BoundFunctionObject);
	case OBJECT_FROZEN:
		return sizeof(FrozenObject);
	case OBJECT_EXCEPTION:
		return sizeof(ExceptionObject);
	case OBJECT_INTERNAL_ERROR:
		return sizeof(InternalErrorObject);
	case OBJECT_TYPE_ERROR:
		return sizeof(TypeErrorObject);
	case OBJECT_RAISED_EXCEPTION:
		return sizeof(RaisedExceptionObject);
	default:
		throw exception("Unsupported object type");
	}
}