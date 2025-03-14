#include <string>
#include <iostream>
#include <vector>
#include <format>

#include "value.h"
#include "vm.h"
#include "object.h"
#include "compiler.h"
#include "natives.h"
#include "RolIterator.h"

using namespace std;

void TValue::print(const Value& value) {
	switch (TValue::vtype(value)) {
	case VAL_NUM:
		printf("%g", AS_NUMBER(value));
		break;
	case VAL_NULL:
		printf("null");
		break;
	case VAL_BOOL:
		printf(AS_BOOLEAN(value) ? "true" : "false");
		break;
	case VAL_OBJ:
		AS_OBJECT(value)->print();
		break;
	default:
		// Unreachable
		return;
	}
}
void TValue::print_leaf(const Value& value) {
	switch (TValue::vtype(value)) {
	case VAL_NUM:
		printf("%g", AS_NUMBER(value));
		break;
	case VAL_NULL:
		printf("null");
		break;
	case VAL_BOOL:
		printf(AS_BOOLEAN(value) ? "true" : "false");
		break;
	case VAL_OBJ:
		AS_OBJECT(value)->print_leaf();
		break;
	default:
		// Unreachable
		return;
	}
}
string TValue::repr(const Value& value, VM* vm) {
	switch (TValue::vtype(value)) {
	case VAL_NULL:
		return "null";
	case VAL_BOOL:
		return AS_BOOLEAN(value) ? "true" : "false";
	case VAL_NUM:
		return vformat(string_view("{}"), make_format_args(AS_RNUMBER(value)));
	case VAL_OBJ:
		return AS_OBJECT(value)->repr(vm);
	default:
		throw exception("Invalid value type");
	}
}
string VTYPEN::repr_leaf(const Value& value, VM* vm) {
	switch (TValue::vtype(value)) {
	case VAL_NULL:
		return "null";
	case VAL_BOOL:
		return AS_BOOLEAN(value) ? "true" : "false";
	case VAL_NUM:
		return vformat(string_view("{}"), make_format_args(AS_RNUMBER(value)));
	case VAL_OBJ:
		return AS_OBJECT(value)->repr_leaf(vm);
	default:
		throw exception("Invalid value type");
	}
}


Value Value_ADD(const Value& a, const Value& b, ValuePool* pool, bool& status) {
	status = true;
	if (IS_NUMBER(a) and IS_NUMBER(b)) {
		return AS_NUMBER(a) + AS_NUMBER(b);
	}
	else if (IS_STRINGOBJ(a) and IS_STRINGOBJ(b)) {
		return OBJ_VAL(pool->internStr(*AS_STRING(a) + *AS_STRING(b)));
	}
	else {
		status = false;
		return NULL_VAL;
	}
}

#define MATHOP_SNIPPET(op) status = true; \
if (IS_NUMBER(a) and IS_NUMBER(b)) { \
	return NUM_VAL(AS_NUMBER(a) op AS_NUMBER(b)); \
} else { \
	status = false; \
	return NULL_VAL; \
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
		return NUM_VAL(pow(AS_NUMBER(a), AS_NUMBER(b)));
	}
	else {
		status = false;
		return NULL_VAL;
	}
}


#undef MATHOP_SNIPPET

#define CMP_SNIPPET(op) status = true; \
if (IS_NUMBER(a) and IS_NUMBER(b)) { \
	return BOOL_VAL(AS_NUMBER(a) op AS_NUMBER(b)); \
} else { \
	status = false; \
	return NULL_VAL; \
}
#define CMP_FUNC(code, op) Value Value_CMP_##code##(const Value& a, const Value& b, bool& status) { \
	CMP_SNIPPET(op); \
}

CMP_FUNC(LESS, < );
CMP_FUNC(GREATER, > );
CMP_FUNC(LESS_EQ, <= );
CMP_FUNC(GREATER_EQ, >= );

#undef CMP_FUNC
#undef CMP_SNIPPET


Value Value_UNARY_NEGATE(const Value& a, bool& status) {
	status = true; 
	if (IS_NUMBER(a)) {
		return -AS_NUMBER(a);
	}
	else {
		status = false;
		return NULL_VAL;
	}
}


// ValuePool implementation

ValuePool::ValuePool() : random_generator(rd()) {
	ValuePool::objectPrototype = new Object("object_prototype");

	this->itertool = new RolIterTools(this);

	finalizeSetup();
}
ValuePool::~ValuePool() {

	for (auto& it : this->number_consts) {
		delete it.second;
	}

	for (auto it : this->objects) {
		delete it;
	}

	for (auto it : this->permanent_objects) {
		delete it;
	}

	for (auto it : this->live_chunks) {
		delete it;
	}

	delete this->itertool;

	delete[] this->character_strings;

	delete ValuePool::objectPrototype;
	// no need to free `lengthString` because it was already deleted as an permanent object
	ValuePool::objectPrototype = ValuePool::lengthString = NULL;
}

void ValuePool::finalizeSetup() {
	character_strings = new StringObject*[256];
	for (int i = 0; i < 256; ++i) {
		character_strings[i] = internPermStr(string(1, (char)(i - 127)));
	}

	ValuePool::lengthString = internPermStr("length");
	ValuePool::lengthString->props.clear();
	ValuePool::lengthString->props[OBJ_VAL(ValuePool::lengthString)] = OBJ_VAL(ValuePool::lengthString);

	prototypeString = internPermStr("prototype");
	nullString = internPermStr("null");
	falseString = internPermStr("false");
	trueString = internPermStr("true");
	finishedString = internPermStr("finished");

	for (const auto& x : permanent_objects) {
		if (x->type == OBJECT_STRING) {
			x->props.erase(OBJ_VAL(NULL));
			x->props[OBJ_VAL(ValuePool::lengthString)] = (double)((StringObject*)x)->length();
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
			case OBJECT_SET:
				x->prototype = this->nativeBase->setPrototype;
				break;
			case OBJECT_FROZEN:
			case OBJECT_OBJECT:
				x->prototype = &this->nativeBase->objectPrototype;
				break;
			}
		}
	}

	native_objects.clear(); native_objects.shrink_to_fit();

#define DEF_PROTO(name) nativeBase->name##Init->property_set(OBJ_VAL(this->prototypeString), OBJ_VAL(nativeBase->name##Prototype));

	DEF_PROTO(set)
	DEF_PROTO(string)
	DEF_PROTO(list)
	DEF_PROTO(tuple)

#undef DEF_PROTO
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

StringObject* ValuePool::internStr(const string& str, bool silently) {
	STRING_INTERNER(silently ? this->silentRegisterObject(stri) : this->registerObject(stri));
}
StringObject* ValuePool::internPermStr(const string& str) {
	auto it = this->string_consts.find(str); if (it == this->string_consts.end()) {
		StringObject* stri = new StringObject(str); this->permanentObject(stri); return this->string_consts[str] = stri;
	}
	else {
		return it->second;
	};
}
StringObject* ValuePool::internNativeStr(const string& str) {
	STRING_INTERNER(this->nativeObject(stri));
}

const size_t hashTupleRepr(const vector<Value>& v) noexcept {
	if (v.empty()) {
		return 0;
	}
	else if (v.size() == 1) {
		return v.front();
	}
	else {
		size_t value = 0;
		for (const auto& x : v)
			value ^= x;
		return value;
	}
}

TupleObject* ValuePool::internTupleConst(const vector<Value>& repr) {
	const size_t hash_id = hashTupleRepr(repr);
	auto it = this->tuple_consts.equal_range(hash_id);
	
	if (it.first != it.second)
		return it.first->second;

	auto tup = repr.empty() ? new TupleObject() : new TupleObject((Value*)&repr.front(), repr.size());
	tup->prototype = this->nativeBase->tuplePrototype;

	this->nativeObject(tup);
	this->tuple_consts.insert({ hash_id, tup });
	this->tuple_hashes[tup] = hash_id;
	return tup;
}



UpvalueObject* ValuePool::addUpvalue(Value* value) {
	auto it = this->upvalues.find(value);
	if (it == this->upvalues.end()) {
		UpvalueObject* upvalue = new UpvalueObject(value);
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

void ValuePool::registerUnknownObject(Object* obj) {
	switch (obj->type) {
	case OBJECT_STRING:
		internStr(((StringObject*)obj)->content, true);
		break;
	case OBJECT_TUPLE:
		registerTuple((TupleObject*)obj);
		break;
	case OBJECT_LIST:
		registerList((ListObject*)obj);
		break;
	default:
		if (total_size > next_gc_run) {
			runGC();
			next_gc_run = total_size * GC_HEAP_GROW_FACTOR;
		}

		objects.push_back(obj);
		total_size += obj->obj_size() + obj->_talloc_size();
		if (!obj->prototype) {
			obj->prototype = ValuePool::objectPrototype;
		}
		break;
	}
}

void ValuePool::registerTuple(TupleObject* tuple, bool silently) {
	tuple->prototype = nativeBase->tuplePrototype;
	silently ? this->silentRegisterObject(tuple) : this->registerObject(tuple);
}
void ValuePool::registerList(ListObject* list, bool silently) {
	list->prototype = nativeBase->listPrototype;
	silently ? this->silentRegisterObject(list) : this->registerObject(list);
}
void ValuePool::registerSet(SetObject* set, bool silently) {
	set->prototype = nativeBase->setPrototype;
	silently ? this->silentRegisterObject(set) : this->registerObject(set);
}
void ValuePool::registerIterable(IterableObject* iterb, bool silently) {
	iterb->prototype = nativeBase->iterablePrototype;
	silently ? this->silentRegisterObject(iterb) : this->registerObject(iterb);
}

void ValuePool::assignPrototype(Object* object) {
	switch (object->type) {
	case OBJECT_STRING:
		object->prototype = nativeBase->stringPrototype;
		break;
	case OBJECT_CLOSURE:
		object->prototype = nativeBase->functionPrototype;
		break;
	case OBJECT_LIST:
		object->prototype = nativeBase->listPrototype;
		break;
	case OBJECT_TUPLE:
		object->prototype = nativeBase->tuplePrototype;
		break;
	case OBJECT_SET:
		object->prototype = nativeBase->setPrototype;
		break;
	case OBJECT_ITERABLE:
		object->prototype = nativeBase->iterablePrototype;
		break;
	case OBJECT_EXCEPTION:
		object->prototype = nativeBase->exceptionPrototype;
		break;
	default:
		object->prototype = &nativeBase->objectPrototype;
		break;
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
		if (OBJECT_ISREACHABLE(x)) {
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
		OBJECT_UNFLAG_REACHABLE(x);
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
			_GCmarkObject((Object*)frame.callee);
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
	if (OBJECT_ISREACHABLE(obj))
		return;

	for (const auto& pr : ITER_PURE_OBJ(obj)) {
		if (IS_OBJECT(pr.second)) {
			grayObjects.push_back(AS_OBJECT(pr.second));
		}
		if (IS_OBJECT(pr.first)) {
			grayObjects.push_back(AS_OBJECT(pr.first));
		}
	}

	if (obj->prototype) {
		grayObjects.push_back(obj->prototype);
	}

	OBJECT_FLAG_REACHABLE(obj);

	switch (obj->type) {
	case OBJECT_OBJECT:
	case OBJECT_FROZEN:
	case OBJECT_ITERABLE:
	case OBJECT_STRING:
	case OBJECT_NATIVE_FUNCTION:
	case OBJECT_EXCEPTION:
		break;
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
		for (auto& x : ITER_LIST(obj)) {
			if (IS_OBJECT(x)) {
				grayObjects.push_back(AS_OBJECT(x));
			}
		}
		break;
	case OBJECT_TUPLE:
		for (auto& x : ITER_TUPLE(obj)) {
			if (IS_OBJECT(x)) {
				grayObjects.push_back(AS_OBJECT(x));
			}
		}
		break;
	case OBJECT_SET:
		for (auto& x : ITER_SET(obj)) {
			if (IS_OBJECT(x)) {
				grayObjects.push_back(AS_OBJECT(x));
			}
		}
		break;
	default:
		throw exception("Unsupported object type");
	}
}