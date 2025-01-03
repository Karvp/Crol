#include "state.h"
#include "vm.h"

using namespace std;

ExecutionState::ObjectReplcDetails::ObjectReplcDetails(Object** first, decltype(props) props, const size_t& pcount) : places({ first }), props(props), prop_count(pcount) {
	replacement = (*first)->obj_copy();
	replacement->props.clear();
}

void ExecutionState::ObjectReplcDetails::buildProperties() {
	replacement->props.insert(props, props + prop_count);
	delete[] props;

	prop_count = 0;
	props = NULL;
}

void ExecutionState::ObjectReplcDetails::replace() {
	for (auto& x : places) {
		*x = replacement;
	}
}

Object* ExecutionState::addReplacement(Object** obj, decltype(ObjectReplcDetails::props) props, const size_t& prop_count) {
	if (!object_replacements.contains(*obj))
		object_replacements.insert({ *obj, ObjectReplcDetails(obj, props, prop_count) });
	return object_replacements[*obj].replacement;
}

const ExecutionState::global_mapping_type& ExecutionState::globalDefs(Compiler* cp) {
	return cp->getBindings().front();
}


void ExecutionState::loadNames(Compiler* cp, Compiler::name_array& names) {
	size_t defineds = names.size();
	names.resize(defineds + globals.size());

	for (const auto& pair : globals) {
		names[defineds + pair.second.pos - natives - 1] = pair.first;
	}
}

void ExecutionState::cleanGlobalsMap(NativeBase* nb) {
	natives = nb->getNativeIDs().size();

	for (const auto& name : nb->getNativeIDs()) {
		globals.erase(name);
	}
}

void ExecutionState::snapshot(VM* vm) {
#ifdef VM_SHOW_SNAPSHOTTING
	printf("[ES] Snapshotting VM's current state...\n");
	printf("[ES] Duplicating object trees...\n");
#endif 
	global_values.resize(globals.size());

	for (const auto& pair : globals) {
		auto& rtvalue = vm->stack_begin[pair.second.pos];
		auto& temp_slot = global_values[pair.second.pos - natives - 1];

		temp_slot = rtvalue;

		if (IS_OBJECT(rtvalue)) {
			copyObjectTree(&AS_OBJECT(temp_slot));
		}
	}

	for (auto& pr : object_replacements) {
		pr.second.replace();
	}
	for (auto& pr : object_replacements) {
		pr.second.buildProperties();
	}
	object_replacements.clear();

#ifdef VM_SHOW_SNAPSHOTTING
	printf("[ES] Snapshot completed!\n");
#endif
}


#define REPLC(o) copyObjectTree((Object**)o);

void ExecutionState::copyObjectTree(Object** obj) {

	if (!*obj)
		return;

	if (object_replacements.contains(*obj)) {
		object_replacements[*obj].addPlace(obj);
		return;
	}

	auto object = *obj;

#ifdef VM_SHOW_SNAPSHOTTING
	printf("%s | %p ", string(scope++, ' ').c_str(), obj);
	object->print();
	printf("\n");
#endif

	
	auto& map = object->props;
	auto pairs = new pair<Value, Value>[map.size()];

	object = this->addReplacement(obj, pairs, map.size());

	if (object->prototype)
		REPLC(&object->prototype);

	size_t i = 0;
	for (auto& pair : map) {
		auto& pr = pairs[i++];
		pr = pair;

		if (IS_OBJECT(pr.first)) {
			REPLC(&AS_OBJECT(pr.first));
		}
		if (IS_OBJECT(pr.second)) {
			REPLC(&AS_OBJECT(pr.second));
		}
	}

	switch (object->type) {
	case OBJECT_FUNCTION:
		for (auto& v : ((FunctionObject*)object)->chunk->getConsts()) {
			if (IS_OBJECT(v)) {
				REPLC(AS_OBJECT(v));
			}
		}
		break;
	case OBJECT_BOUND_FUNCTION: {
		auto bfunc = (BoundFunctionObject*)object;
		REPLC(&bfunc->function);
		if (IS_OBJECT(bfunc->thisValue))
			REPLC(&AS_OBJECT(bfunc->thisValue));
		break;
	}
	case OBJECT_UPVALUE: {
		auto upvalue = (UpvalueObject*)object;
		if (IS_OBJECT(*upvalue->location))
			REPLC(&AS_OBJECT(*upvalue->location));
		break;
	}
	case OBJECT_CLOSURE: {
		auto closure = (ClosureObject*)object;
		REPLC(&closure->function);

		for (auto it = closure->upvalues; it < closure->upvalues + closure->upvalue_count; ++it) {
			REPLC(it);
		}
		break;
	}
	case OBJECT_STRING:
		((StringObject*)object)->props.clear();
		break;
	case OBJECT_LIST: {
		for (auto& v : *((ListObject*)object)) {
			if (IS_OBJECT(v)) {
				REPLC(&AS_OBJECT(v));
			}
		}
		break;
	}
	case OBJECT_TUPLE: {
		for (auto& v : *((TupleObject*)object)) {
			if (IS_OBJECT(v)) {
				REPLC(&AS_OBJECT(v));
			}
		}
		break;
	}
	case OBJECT_RAISED_EXCEPTION: {
		auto& tv = ((RaisedExceptionObject*)object)->throwValue;
		if (IS_OBJECT(tv))
			REPLC(&AS_OBJECT(tv));
		break;
	}
	}

#ifdef VM_SHOW_SNAPSHOTTING
	--scope;
#endif
}

#undef REPLC


#define REGOBJ(o, t) (o) = (t ## Object*)recursiveRegisterObject((o), pool);

void ExecutionState::loadSnapshot(VM* vm) {

#ifdef VM_SHOW_SNAPSHOTTING
	printf("[ES] Loading snapshot from previous state...\n");
	printf("[ES] Scanning object trees...\n");
#endif

	for (auto& value : global_values) {
		if (IS_OBJECT(value)) {
			AS_OBJECT(value) = recursiveRegisterObject(AS_OBJECT(value), vm->pool);
		}
		*++vm->stack_end = value;
	}

	for (const auto& obj : objects) {
		if (obj->type != OBJECT_STRING)
			vm->pool->registerUnknownObject(obj);
		else
			delete obj;
	}
	objects.clear();

#ifdef VM_SHOW_SNAPSHOTTING
	printf("[ES] Snapshot loaded successfully!\n");
#endif
}

void ExecutionState::registerPropertyMap(Object* obj, ValuePool* pool) {
	auto& map = obj->props;
	const size_t size = map.size();
	auto pairs = new pair<Value, Value>[size];

	size_t i = 0;
	for (auto& pair : map) {
		auto& pr = pairs[i++];
		pr = pair;

		if (IS_OBJECT(pr.first)) {
			AS_OBJECT(pr.first) = recursiveRegisterObject(AS_OBJECT(pr.first), pool);
		}
		if (IS_OBJECT(pr.second)) {
			AS_OBJECT(pr.second) = recursiveRegisterObject(AS_OBJECT(pr.second), pool);
		}
	}

	map.clear();
	map.insert(pairs, pairs + size);

	delete[] pairs;
}

Object* ExecutionState::recursiveRegisterObject(Object* obj, ValuePool* pool) {

	if (objects.contains(obj))
		return obj;

#ifdef VM_SHOW_SNAPSHOTTING
	printf("%s | %p ", string(scope++, ' ').c_str(), obj);
	obj->print();
	printf("\n");
#endif

	objects.insert(obj);
	if (obj->prototype)
		REGOBJ(obj->prototype);

	registerPropertyMap(obj, pool);

	switch (obj->type) {
	case OBJECT_FUNCTION:
		pool->addChunk(((FunctionObject*)obj)->chunk);
		for (auto& v : ((FunctionObject*)obj)->chunk->getConsts()) {
			if (IS_OBJECT(v)) {
				REGOBJ(AS_OBJECT(v));
			}
		}
		break;
	case OBJECT_BOUND_FUNCTION: {
		auto bfunc = (BoundFunctionObject*)obj;
		REGOBJ(bfunc->function);
		if (IS_OBJECT(bfunc->thisValue))
			REGOBJ(AS_OBJECT(bfunc->thisValue));
		break;
	}
	case OBJECT_CLOSURE: {
		auto closure = (ClosureObject*)obj;
		REGOBJ(closure->function, Function);
		for (auto it = closure->upvalues + closure->upvalue_count - 1; it >= closure->upvalues; --it) {
			auto _ptr = *it;

			*it = pool->addUpvalue(pool->vm->stack_begin + (*it)->capture_pos, (*it)->capture_pos);

			if (_ptr)
				delete _ptr;

			REGOBJ(*it, Upvalue);
		}
		break;
	}
	case OBJECT_STRING: {
		auto stri = pool->internString(((StringObject*)obj)->content);
		stri->prototype = pool->nativeBase->stringPrototype;

		obj = stri;
		break;
	}
	case OBJECT_LIST: {
		for (auto& v : *((ListObject*)obj)) {
			if (IS_OBJECT(v)) {
				REGOBJ(AS_OBJECT(v));
			}
		}
		break;
	}
	case OBJECT_TUPLE: {
		for (auto& v : *((TupleObject*)obj)) {
			if (IS_OBJECT(v)) {
				REGOBJ(AS_OBJECT(v));
			}
		}
		break;
	}
	case OBJECT_RAISED_EXCEPTION: {
		auto& tv = ((RaisedExceptionObject*)obj)->throwValue;
		if (IS_OBJECT(tv))
			REGOBJ(AS_OBJECT(tv));
		break;
	}
	}

#ifdef VM_SHOW_SNAPSHOTTING
	--scope;
#endif
	return obj;
}

#undef REGOBJ