#include <string>
#include <iostream>
#include <vector>

#include "value.h"
#include "vm.h"
#include "object.h"
#include "compiler.h"
#include "natives.h"

using namespace std;

void Value::print() const {
	switch (this->type) {
	case VAL_NUM:
		printf("%g", AS_NUMBER(*this));
		break;
	case VAL_NULL:
		printf("null");
		break;
	case VAL_BOOL:
		printf(AS_BOOLEAN(*this) ? "true" : "false");
		break;
	case VAL_OBJ:
		AS_OBJECT(*this)->print();
		break;
	default:
		// Unreachable
		return;
	}
}

Value Value_ADD(const Value& a, const Value& b, ValuePool* pool, bool& status) {
	status = true;
	if (IS_NUMBER(a) and IS_NUMBER(b)) {
		return AS_NUMBER(a) + AS_NUMBER(b);
	}
	else if (IS_STRINGOBJ(a) and IS_STRINGOBJ(b)) {
		return pool->internString(*AS_STRING(a) + *AS_STRING(b));
	}
	else {
		status = false;
		return Value();
	}
}

#define MATHOP_SNIPPET(op) status = true; \
if (IS_NUMBER(a) and IS_NUMBER(b)) { \
	return AS_NUMBER(a) op AS_NUMBER(b); \
} else { \
	status = false; \
	return Value(); \
}

Value Value_SUB(const Value& a, const Value& b, bool& status) {
	MATHOP_SNIPPET(-);
}
Value Value_MUL(const Value& a, const Value& b, bool& status) {
	MATHOP_SNIPPET(*);
}
Value Value_DIV(const Value& a, const Value& b, bool& status) {
	MATHOP_SNIPPET(/);
}
Value Value_POW(const Value& a, const Value& b, bool& status) {
	status = true;
	if (IS_NUMBER(a) and IS_NUMBER(b)) {
		return pow(AS_NUMBER(a), AS_NUMBER(b));
	}
	else {
		status = false;
		return Value();
	}
}


#undef MATHOP_SNIPPET

#define CMP_SNIPPET(op) status = true; \
if (IS_NUMBER(a) and IS_NUMBER(b)) { \
	return AS_NUMBER(a) op AS_NUMBER(b); \
} else { \
	status = false; \
	return Value(); \
}

Value Value_CMP_LESS(const Value& a, const Value& b, bool& status) {
	CMP_SNIPPET(< );
}
Value Value_CMP_GREATER(const Value& a, const Value& b, bool& status){
	CMP_SNIPPET(> );
}
Value Value_CMP_LESS_EQ(const Value& a, const Value& b, bool& status){
	CMP_SNIPPET(<= );
}
Value Value_CMP_GREATER_EQ(const Value& a, const Value& b, bool& status){
	CMP_SNIPPET(>= );
}

#undef CMP_SNIPPET


Value Value_UNARY_NEGATE(const Value& a, bool& status) {
	status = true; 
	if (IS_NUMBER(a)) {
		return -AS_NUMBER(a);
	}
	else {
		status = false;
		return Value();
	}
}


// ValuePool implementation

ValuePool::ValuePool() : random_generator(rd()) {
	this->ValueConstFalse = new Value(false);
	this->ValueConstTrue = new Value(true);
	this->ValueConstNull = new Value();

	ValuePool::objectPrototype = new Object("object_prototype");
}

void ValuePool::finalizeSetup() {
	this->character_strings = new StringObject*[256];
	for (int i = 0; i < 256; ++i) {
		this->character_strings[i] = this->internPermanentString(string(1, (char)(i - 127)));
	}

	ValuePool::lengthString = this->internPermanentString("length");
	ValuePool::lengthString->props.clear();
	ValuePool::lengthString->props[ValuePool::lengthString] = ValuePool::lengthString;

	this->prototypeString = this->internString("prototype");

	for (const auto& x : permanent_objects) {
		if (x->type == OBJECT_STRING) {
			x->props.erase((Object*)NULL);
			x->props[ValuePool::lengthString] = (double)((StringObject*)x)->length();
		}
	}
}

void ValuePool::finalizeRegisterations() {
	next_gc_run = total_size * 2;

	for (const auto& object_list : { native_objects, permanent_objects, objects }) {

		for (auto& x : object_list) {
			switch (x->type) {
			case OBJECT_STRING:
				x->prototype = this->nativeBase->stringPrototype;
				break;
			case OBJECT_FROZEN:
			case OBJECT_OBJECT:
				x->prototype = &this->nativeBase->objectPrototype;
				break;
			}
		}
	}

	native_objects.clear(); native_objects.shrink_to_fit();
}

ValuePool::~ValuePool() {
	delete this->ValueConstNull;
	delete this->ValueConstTrue;
	delete this->ValueConstFalse;
	delete[] this->character_strings;

	for (auto it : this->permanent_objects) {
		delete it;
	}

	for (auto& it : this->number_consts) {
		delete it.second;
	}

	for (auto it : this->objects) {
		delete it;
	}

	for (auto it : this->live_chunks) {
		delete it;
	}

	delete ValuePool::objectPrototype;
	// no need to free `lengthString` because it was already deleted as an permanent object
	ValuePool::objectPrototype = ValuePool::lengthString = NULL;
}

#define STRING_INTERNER(registerer) auto it = this->string_consts.find(str); \
if (it == this->string_consts.end()) { \
	StringObject* stri = new StringObject(str); \
	stri->prototype = this->nativeBase->stringPrototype; \
	registerer; \
	return this->string_consts[str] = stri; \
} else { \
	return it->second; \
}

bool ValuePool::hasString(const string& stri) const {
	return this->string_consts.contains(stri);
}

StringObject* ValuePool::internString(const string& str, bool silently) {
	STRING_INTERNER(silently ? this->silentRegisterObject(stri) : this->registerObject(stri));
}
StringObject* ValuePool::internPermanentString(const string& str) {
	STRING_INTERNER(this->permanentObject(stri));
}
StringObject* ValuePool::internNativeString(const string& str) {
	STRING_INTERNER(this->nativeObject(stri));
}

const size_t hashTupleRepr(const vector<Value>& v) noexcept {
	if (v.empty()) {
		return 0;
	}
	else if (v.size() == 1) {
		return hash<Value>()(v.front());
	}
	else {
		size_t value = 0;
		hash<Value> hasher_;
		for (const auto& x : v)
			value ^= hasher_(x);
		return value;
	}
}

TupleObject* ValuePool::internTupleConst(const vector<Value>& repr) {
	const size_t hash_id = hashTupleRepr(repr);
	auto it = this->tuple_consts.equal_range(hash_id);
	
	for (auto x = it.first; x != it.second; ++x) {
		return x->second;
	}

	auto tup = new TupleObject((Value*)&repr.front(), repr.size());
	tup->prototype = this->nativeBase->tuplePrototype;

	this->nativeObject(tup);
	this->tuple_consts.insert({ hash_id, tup });
	this->tuple_hashes[tup] = hash_id;
	return tup;
}



UpvalueObject* ValuePool::addUpvalue(Value* value, int capture_pos) {
	auto it = this->upvalues.find(value);
	if (it == this->upvalues.end()) {
		UpvalueObject* upvalue = new UpvalueObject(value, capture_pos);
		this->registerObject(upvalue);
		return this->upvalues[value] = upvalue;
	}
	else {
		return it->second;
	}
}
UpvalueObject* ValuePool::getUpvalue(Value* value) {
	auto it = this->upvalues.find(value);
	return it == this->upvalues.end() ? NULL : it->second;
}

// Object registerers

void ValuePool::registerTuple(TupleObject* tuple) {
	tuple->prototype = nativeBase->tuplePrototype;
	this->registerObject(tuple);
}

void ValuePool::registerList(ListObject* list) {
	list->prototype = nativeBase->listPrototype;
	this->registerObject(list);
}

void ValuePool::assignPrototype(ObjectType type, Object* prototype) {
	for (auto& x : objects) {
		if (x->type == type) {
			x->prototype = prototype;
		}
	}

	for (auto& x : permanent_objects) {
		if (x->type == type) {
			x->prototype = prototype;
		}
	}
}

// -*-*-
// Mark & Sweep garbage collector implementation
// -*-*-

void ValuePool::runGC() {
#ifdef GC_SHOW_SUMMARY
	const size_t old_size = this->total_size;
#endif

	_GCmarkRoots();

	// Trace references
	while (!grayObjects.empty()) {
		auto x = grayObjects.back(); grayObjects.pop_back();
		_GCmarkObject(x);
	}
	grayObjects.shrink_to_fit();


	// Sweeping
	vector<Object*> living;
	for (const auto& x : this->objects) {
		if (x->mark) {
			living.push_back(x);
		}
		else {
			this->total_size -= x->obj_size() + x->_talloc_size();

			switch (x->type) {
			case OBJECT_STRING:
				this->string_consts.erase(((StringObject*)x)->content);
				break;
			case OBJECT_TUPLE: {
				auto tup = (TupleObject*)x;
				auto it = this->tuple_hashes.find(tup);

				if (it != this->tuple_hashes.end()) {
					auto eq_r = this->tuple_consts.equal_range(it->second);
					for (auto beg = eq_r.first, end = eq_r.second; beg != end; ++beg) {
						if (beg->second == tup) {
							this->tuple_consts.erase(beg);
							break;
						}
					}
					this->tuple_hashes.erase(it);
				}
				break;
			}
			case OBJECT_UPVALUE: {
				auto it = this->upvalues.find(((UpvalueObject*)x)->location);
				if (!(vm->stackPtr <= it->first && it->first <= vm->stack_end)) {
					delete it->first;
				}
				this->upvalues.erase(it);
				break;
			}
			}
			delete x;

#ifdef GC_SHOW_DEALLOCS
			printf("Deallocated %p\n", x);
#endif
		}
	}
	this->objects = living;
	for (auto x : this->objects) {
		x->mark = false;
	}

#ifdef GC_SHOW_SUMMARY
	printf("[GC] Cleaned %llu bytes: %llu -> %llu\n", old_size - this->total_size, old_size, this->total_size);
#endif
}

void ValuePool::_GCmarkChunk(Chunk* chunk) {
	for (auto& x : chunk->getConsts()) {
		if (IS_OBJECT(x)) {
			grayObjects.push_back(AS_OBJECT(x));
		}
	}
}

void ValuePool::_GCmarkCompilerRoots() {
	Compiler* compiler = this->currentCompiler;
	while (compiler != NULL) {
		_GCmarkChunk(compiler->getChunk());

		compiler = compiler->parent;
	}
}

void ValuePool::_GCmarkRoots() {
	if (vm) {
		for (auto elem = vm->stackPtr; elem <= vm->stack_end; ++elem) {
			if (IS_OBJECT(*elem)) {
				_GCmarkObject(AS_OBJECT(*elem));
			}
		}

		for (auto& frame : vm->frames) {
			_GCmarkObject((Object*)frame.closure);
		}
	}
	else {
		for (auto& native : native_objects) {
			_GCmarkObject(native);
		}
	}

	_GCmarkCompilerRoots();
}

void ValuePool::_GCmarkObject(Object* obj) {
	if (obj->mark)
		return;

	for (auto it = obj->begin(); it != obj->end(); ++it) {
		if (IS_OBJECT(it->second)) {
			grayObjects.push_back(AS_OBJECT(it->second));
		}
		if (IS_OBJECT(it->first)) {
			grayObjects.push_back(AS_OBJECT(it->first));
		}
	}

	if (obj->prototype) {
		grayObjects.push_back(obj->prototype);
	}

	obj->mark = true;

	switch (obj->type) {
	case OBJECT_OBJECT:
	case OBJECT_FROZEN:
	case OBJECT_STRING:
	case OBJECT_NATIVE_FUNCTION:
	case OBJECT_EXCEPTION:
	case OBJECT_TYPE_ERROR:
	case OBJECT_INTERNAL_ERROR:
		break;
	case OBJECT_RAISED_EXCEPTION: {
		const auto& v = ((RaisedExceptionObject*)obj)->throwValue;
		if (IS_OBJECT(v))
			grayObjects.push_back(AS_OBJECT(v));
		break;
	}
	case OBJECT_UPVALUE: {
		Value& v = *((UpvalueObject*)obj)->location;
		if (IS_OBJECT(v)) {
			grayObjects.push_back(AS_OBJECT(v));
		}
		break;
	}
	case OBJECT_FUNCTION:
		_GCmarkChunk(((FunctionObject*)obj)->chunk);
		break;
	case OBJECT_CLOSURE: {
		auto closure = (ClosureObject*)obj;
		grayObjects.push_back(closure->function);
		if (closure->upvalue_count) {
			grayObjects.insert(grayObjects.end(), closure->upvalues, closure->upvalues + closure->upvalue_count - 1);
		}
		break;
	}
	case OBJECT_BOUND_FUNCTION: {
		auto boundfn = (BoundFunctionObject*)obj;
		grayObjects.push_back(boundfn->function);
		if (IS_OBJECT(boundfn->thisValue)) {
			grayObjects.push_back(AS_OBJECT(boundfn->thisValue));
		}
		break;
	}
	case OBJECT_LIST:
		for (auto& x : *(ListObject*)obj) {
			if (IS_OBJECT(x)) {
				grayObjects.push_back(AS_OBJECT(x));
			}
		}
		break;
	case OBJECT_TUPLE:
		for (auto& x : *(TupleObject*)obj) {
			if (IS_OBJECT(x)) {
				grayObjects.push_back(AS_OBJECT(x));
			}
		}
		break;
	default:
		throw exception("Unsupported object type");
	}
}