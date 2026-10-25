#include "object.h"
#include "value.h"
#include "vm.h"
#include "RolIterator.h"
#include <algorithm>
#include <set>

using namespace std;

// Internal implementations

Object* Object::specie_construct(Object* target, VM* vm) noexcept {
	Object* obj;

	switch (OBJECT_SPECIE_TYPE(target)) {
	case OBJECT_LIST:
		obj = new ListObject();
		break;
	case OBJECT_TUPLE:
		obj = new TupleObject();
		break;
	case OBJECT_SET:
		obj = new SetObject();
		break;
	default:
		obj = new Object();
		break;
	}

	auto new_proto = target->get(toValue(vm->pool->prototypeString), vm);
	if (IS_OBJECT(new_proto)) {
		obj->prototype = AS_OBJECT(new_proto);
	}
	else {
		vm->pool->assignPrototype(obj);
	}
	return obj;
}

Value StringObject::get(const Value& key, VM* vm) {
	if (IS_NUMBER(key)) {
		double num = AS_NUMBER(key);
		if (num >= 0 && num < content.size()) {
			return toValue(vm->pool->character_strings[127 + content[(size_t)num]]);
		}
		return NULL_VAL;
	}

	return Object::get(key, vm);
}

constexpr vector<Value>& CallableSpecie::getConsts() noexcept {
	return getChunk()->getConsts();
}
constexpr vector<uint8_t>& CallableSpecie::getOPs() noexcept {
	return getChunk()->getOPs();
}
constexpr ValuePool* CallableSpecie::getPool() const noexcept {
	return getChunk()->getPool();
}

// Object prototype implementation

NATIVE_METHOD_SIG(Object, copy) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}
	else if (IS_STRING(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument should not be a string");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "No argument is expected");
	}

	auto obj = AS_OBJECT(thisarg)->obj_copy();
	vm->pool->registerUnknownObject(obj);

	NATIVE_RETURN(vm, obj);
}

NATIVE_METHOD_SIG(Object, has) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
	}

	NATIVE_RETURN(vm, AS_OBJECT(thisarg)->property_has(*arg));
}

NATIVE_METHOD_SIG(Object, size) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}

	NATIVE_RETURN(vm, (double)AS_OBJECT(thisarg)->props.size());
}

NATIVE_METHOD_SIG(Object, keys) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}

	const size_t size = AS_OBJECT(thisarg)->props.size();
	auto lst = new ListObject(size);

	auto it = lst->list.begin();
	for (const auto& pr : AS_OBJECT(thisarg)->props) {
		*(it++) = pr.first;
	}

	NATIVE_RETURN(vm, lst);
}

NATIVE_METHOD_SIG(Object, values) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}

	const size_t size = AS_OBJECT(thisarg)->props.size();
	auto lst = new ListObject(size);

	auto it = lst->list.begin();
	for (const auto& pr : AS_OBJECT(thisarg)->props) {
		*(it++) = pr.second;
	}

	vm->pool->registerList(lst);
	NATIVE_RETURN(vm, lst);
}

NATIVE_METHOD_SIG(Object, items) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}

	const size_t size = AS_OBJECT(thisarg)->props.size();
	auto lst = new ListObject(size);

	auto it = lst->list.begin();
	for (const auto& pr : AS_OBJECT(thisarg)->props) {
		*it = toValue(new TupleObject((Value*)&pr.first, 2));
		vm->pool->registerTuple(AS_TOBJECT(*it, TupleObject), true);
		++it;
	}

	vm->pool->registerList(lst, true);
	NATIVE_RETURN(vm, lst);
}

NATIVE_METHOD_SIG(Object, extendby) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be an object");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
	}
	else if (!IS_OBJECT(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected to pass in 1 object");
	}

	auto obj_props = ITER_PURE_OBJ(AS_OBJECT(*arg));
	AS_OBJECT(thisarg)->props.insert(obj_props.begin(), obj_props.end());

	NATIVE_RETURN(vm, AS_OBJECT(thisarg));
}

//
// Iterable prototype
//

NATIVE_METHOD_SIG(IterableObject, next) {
	if (!IS_ITERABLEOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected an Iterable object as `this` argument");
	}

	auto obj = AS_TOBJECT(thisarg, IterableObject);
	auto finished = obj->property_get(toValue(vm->pool->finishedString));

	if (*finished) {
		NATIVE_RETURN_NULL(vm);
	}

	Value ret;
	*finished = !obj->rol_it.next(ret);
	if (vm->throwing)
		return;

	NATIVE_RETURN(vm, ret);
}
NATIVE_METHOD_SIG(IterableObject, feed) {
	if (!IS_ITERABLEOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected an Iterable object as `this` argument");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument as the generator's food");
	}

	auto obj = AS_TOBJECT(thisarg, IterableObject);
	auto finished = obj->property_get(toValue(vm->pool->finishedString));

	if (*finished) {
		NATIVE_RETURN_NULL(vm);
	}

	obj->rol_it.begin().it->feed(*arg);
	Value ret;
	*finished = !obj->rol_it.next(ret);
	if (vm->throwing)
		return;

	NATIVE_RETURN(vm, ret);
}

// String prototype

NATIVE_METHOD_SIG(StringObject, repeat) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "First argument must be a number");
	}
	
	const char* str = AS_STRING(thisarg)->content.c_str();
	string newstr;
	
	for (size_t i = max(AS_NUMBER(*arg), 0.); i; --i) {
		newstr.append(str);
	}

	NATIVE_RETURN(vm, vm->pool->internStr(newstr));
}

NATIVE_METHOD_SIG(StringObject, substr) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}

	if (arg_count == 1) {
		// (start_pos,)
		if (!IS_NUMBER(*arg)) {
			NATIVE_RAISE_TERROR(vm, "First argument must be a number");
		}
		NATIVE_RETURN(vm, vm->pool->internStr(string(AS_STRING(thisarg)->content, max(AS_NUMBER(*arg), 0.))));
	}
	else if (arg_count == 2) {
		// (start_pos, length)
		if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "First and second arguments must be a number");
		}
		NATIVE_RETURN(vm, vm->pool->internStr(string(AS_STRING(thisarg)->content, max(AS_NUMBER(*arg), 0.), max(AS_NUMBER(*(arg + 1)), 0.))));
	}
	else {
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}
}

NATIVE_METHOD_SIG(StringObject, find) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count != 1 && arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE_TERROR(vm, "First argument must be a string");
	}

	size_t start = string::npos;

	if (arg_count == 2) {
		if (!IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "Startindex must be a number");
		}
		start = max(AS_NUMBER(*(arg + 1)), 0.);
	}

	size_t pos = AS_STRING(thisarg)->content.find(AS_STRING(*arg)->content, start);
	if (pos == string::npos) {
		NATIVE_RETURN(vm, (double)-1);
	}
	NATIVE_RETURN(vm, (double)pos);
}
NATIVE_METHOD_SIG(StringObject, rfind) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	} else if (arg_count != 1 && arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE_TERROR(vm, "First argument must be a string");
	}

	size_t start = string::npos;

	if (arg_count == 2) {
		if (!IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "Second argument must be a number");
		}
		start = max(AS_NUMBER(*(arg + 1)), 0.);
	}

	size_t pos = AS_STRING(thisarg)->content.rfind(AS_STRING(*arg)->content, start);
	if (pos == string::npos) {
		NATIVE_RETURN(vm, (double)-1);
	}
	NATIVE_RETURN(vm, (double)pos);
}

NATIVE_METHOD_SIG(StringObject, reverse) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "0 argument is expected");
	}

	const auto& original = AS_STRING(thisarg)->content;

	NATIVE_RETURN(vm, vm->pool->internStr(string(original.rbegin(), original.rend())));
}

NATIVE_METHOD_SIG(StringObject, split) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count && arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 0 to 1 argument is expected");
	}

	const auto& original = AS_STRING(thisarg)->content;
	ListObject* tokens = NULL;

	if (arg_count) {
		if (!IS_STRINGOBJ(*arg)) {
			NATIVE_RAISE_TERROR(vm, "First argument must be a string");
		}

		const auto& delim = AS_STRING(*arg)->content;
		tokens = new ListObject();

		size_t last = 0, next = 0;
		while ((next = original.find(delim, last)) != string::npos) {
			tokens->push(toValue(vm->pool->internStr(original.substr(last, next - last), true)));
			last = next + 1;
		}
		tokens->push(toValue(vm->pool->internStr(original.substr(last), true)));
	}
	else {
		tokens = new ListObject();

		for (const auto& c : original) {
			tokens->push(toValue(vm->pool->character_strings[127 + c]));
		}
	}

	vm->pool->registerList(tokens);
	NATIVE_RETURN(vm, tokens);
}

NATIVE_METHOD_SIG(StringObject, join) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_ITERABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be an iterable");
	}

	const auto& delim = AS_STRING(thisarg)->content;
	string result;
	bool has_before = false;

	for (const auto& x : ITER_ANY_VOBJ(*arg)) {
		if (vm->throwing)
			return;

		if (!IS_STRINGOBJ(x)) {
			NATIVE_RAISE_TERROR(vm, "Elements must be strings");
		}

		if (has_before)
			result.append(delim);

		result.append(AS_STRING(x)->content);
		has_before = true;
	}

	NATIVE_RETURN(vm, vm->pool->internStr(result));
}

NATIVE_METHOD_SIG(StringObject, replace) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "2 arguments is expected");
	} else if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE_TERROR(vm, "First and second arguments must be strings");
	}

	const auto& original = AS_STRING(thisarg)->content,
		find = AS_STRING(*arg)->content,
		replacement = AS_STRING(*arg)->content;

	string result;

	const char* start_Ptr = &original.front();
	size_t last = 0, next = 0;
	while ((next = original.find(find, last)) != string::npos) {
		result.append(start_Ptr + last, next - last);
		result.append(replacement);
		last = next + 1;
	}
	result.append(start_Ptr + last, next - last);

	NATIVE_RETURN(vm, vm->pool->internStr(result));
}

NATIVE_METHOD_SIG(StringObject, count) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a string");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
	} else if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE_TERROR(vm, "First argument must be a string");
	}

	const auto& delim = AS_STRING(*arg)->content;
	const auto& original = AS_STRING(thisarg)->content;

	int counter = 0;
	size_t last = 0, next = 0;
	while ((next = original.find(delim, last)) != string::npos) {
		++counter;
		last = next + 1;
	}

	NATIVE_RETURN(vm, (double)counter);
}

// End of string prototype
// --
// Function prototype

size_t FunctionObject::_talloc_size() const noexcept {
	return chunk->_talloc_size() + sizeof(Chunk);
}

NATIVE_METHOD_SIG(FunctionObject, name) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a callable");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "No argument is expected");
	}
	
	NATIVE_RETURN(vm, vm->pool->internStr(AS_CLOSURE(thisarg)->getFuncName()));
}
NATIVE_METHOD_SIG(FunctionObject, arity) {
	if (!IS_OBJECT(thisarg) || !IS_CLOSURE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a defined function");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "No argument is expected");
	}

	NATIVE_RETURN(vm, (double)AS_CLOSURE(thisarg)->getArity());
}
NATIVE_METHOD_SIG(FunctionObject, bind) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a callable");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Only 1 argument is expected");
	}

	auto bound_func = new BoundFunctionObject(IS_BOUND_FUNCTION(thisarg) ? AS_TOBJECT(thisarg, BoundFunctionObject)->function : (CallableSpecie*)AS_OBJECT(thisarg), *arg);
	vm->pool->registerObject(bound_func);
	NATIVE_RETURN(vm, (Object*)bound_func);
}
NATIVE_METHOD_SIG(FunctionObject, apply) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a function");
	}

	if (!vm->callValue(AS_OBJECT(thisarg), arg, arg_count - 1, VMCODE_BREAK)) {
		// Not native function
		vm->interpret();

		--vm->stack_end;
		*vm->stack_end = *(vm->stack_end + 1);
	}
}
NATIVE_METHOD_SIG(FunctionObject, invoke) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a function");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 argument for 'invoke' function");
	}

	const Value invokeas = *arg, callee = thisarg, iterable = *(arg + 1);

	vm->stack_end -= arg_count;
	*++vm->stack_end = callee;
	*++vm->stack_end = invokeas;
	Value* arg_ptr = vm->stack_end++;

	vector<Value> arguments;

	for (const auto& x : ITER_ANY_VOBJ(iterable)) {
		if (vm->throwing)
			return;

		arguments.push_back(x);
	}

	vm->stack_end = arg_ptr;
	if (!arguments.empty())
		memcpy(arg_ptr + 1, &arguments.front(), arguments.size() * sizeof(Value));
	
	if (!vm->callValue(AS_OBJECT(callee), vm->stack_end, arguments.size(), VMCODE_BREAK)) {
		// Not native function
		vm->interpret();

		--vm->stack_end;
		*vm->stack_end = *(vm->stack_end + 1);
	}
}


// List prototype

NATIVE_METHOD_SIG(ListObject, list_size) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	NATIVE_RETURN(vm, (double)AS_LIST(thisarg)->list.size());
}

NATIVE_METHOD_SIG(ListObject, list_push) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	con.insert(con.end(), arg, arg + arg_count);

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_insert) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 arguments");
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a number");
	}
	else if (!IS_ITERABLE(*(arg + 1))) {
		NATIVE_RAISE_TERROR(vm, "Expected second argument to be an iterable");
	}

	vector<Value> results;
	for (const auto& x : ITER_ANY_VOBJ(*(arg + 1))) {
		if (vm->throwing)
			return;

		results.push_back(x);
	}

	size_t insert_pos = AS_NUMBER(*arg);
	auto& clist = AS_LIST(thisarg)->list;
	clist.insert(clist.begin() + insert_pos, results.begin(), results.end());

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_extend) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1 || !IS_ITERABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single input argument to be an iterable");
	}

	auto& clist = AS_LIST(thisarg)->list;
	for (const auto& x : ITER_ANY_VOBJ(*arg)) {
		if (vm->throwing)
			return;

		clist.push_back(x);
	}

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_extend_at) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected `this` argument to be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 arguments: an index and an iterable");
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a number");
	}
	else if (!IS_ITERABLE(*(arg + 1))) {
		NATIVE_RAISE_TERROR(vm, "Expected second argument to be an iterable");
	}

	vector<Value> vec;
	for (const auto& x : ITER_ANY_VOBJ(*(arg+1))) {
		if (vm->throwing)
			return;

		vec.push_back(x);
	}

	auto& clist = AS_LIST(thisarg)->list;
	clist.insert(clist.begin() + (size_t)AS_NUMBER(*arg), vec.begin(), vec.end());

	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(ListObject, list_pop) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;

	switch (arg_count) {
	case 0:
		con.pop_back();
		break;
	case 1: {
		if (!IS_NUMBER(*arg) || AS_NUMBER(*arg) < 0) {
			NATIVE_RAISE_TERROR(vm, "Second argument must be a number greater or equal to 0");
		}
		size_t x = AS_NUMBER(*arg);

		if (!x) {} else
		if (x > con.size()) {
			con.clear();
		}
		else {
			con.pop_back();
			while (--x) {
				con.pop_back();
			}
		}
		break;
	}
	default:
		NATIVE_RAISE_TERROR(vm, "Expected at most 1 argument");
	}

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_remove) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument as the value to remove");
	}

	const auto& specimen = *arg;
	auto& clist = AS_TOBJECT(thisarg, ListObject)->list;
	VAR_BASET(clist) con;

	for (const auto& v : clist) {
		if (v != specimen) {
			con.push_back(v);
		}
	}

	clist = con;

	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(ListObject, list_swap) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "2 arguments are expected");
	}
	else if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
		NATIVE_RAISE_TERROR(vm, "Arguments of list.swap function must be numbers");
	}

	auto& list = AS_LIST(thisarg)->list;

	const Value temp = list[AS_NUMBER(*arg)];
	list[AS_NUMBER(*arg)] = list[AS_NUMBER(*(arg + 1))];
	list[AS_NUMBER(*(arg + 1))] = temp;

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_clear) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	AS_TOBJECT(thisarg, ListObject)->list.clear();
	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(ListObject, list_find) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (!IS_NUMBER(*(arg + 1)) || AS_NUMBER(*(arg + 1)) < 0) {
			NATIVE_RAISE_TERROR(vm, "Second argument must be a number greater or equal to 0");
		}
		i = AS_NUMBER(*(arg + 1));
		break;
	default:
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	const auto& elem = *arg;
	const auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	
	for (; i < con.size(); ++i) {
		if (con[i] == elem) {
			NATIVE_RETURN(vm, (double)i);
		}
	}
	NATIVE_RETURN(vm, -1.);
}
NATIVE_METHOD_SIG(ListObject, list_findsub) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	size_t i = 0;
	ListObject* sublist_ptr = NULL;

	switch (arg_count) {
	case 2:
		if (!IS_NUMBER(*(arg + 1)) || AS_NUMBER(*(arg + 1)) < 0) {
			NATIVE_RAISE_TERROR(vm, "Second argument must be a number greater or equal to 0");
		}
		i = AS_NUMBER(*(arg + 1));
	case 1:
		if (!IS_LISTOBJ(*arg)) {
			NATIVE_RAISE_TERROR(vm, "First argument must be a list");
		}
		sublist_ptr = AS_LIST(*arg);
		break;
	default:
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	const auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	auto& sublist = sublist_ptr->list;

	for (; i < con.size() && con.size() - i >= sublist.size(); ++i) {
		if (con[i] == sublist.front()) {

			bool check = true;
			for (int j = 1; j < sublist.size(); ++j) {
				if (con[i + j] != sublist[j]) {
					check = false;
					break;
				}
			}

			if (check) {
				NATIVE_RETURN(vm, (double)i);
			}
		}
	}
	NATIVE_RETURN(vm, -1.);
}
NATIVE_METHOD_SIG(ListObject, list_count) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (!IS_NUMBER(*(arg + 1)) || AS_NUMBER(*(arg + 1)) < 0) {
			NATIVE_RAISE_TERROR(vm, "Startindex must be a number greater or equal to 0");
		}
		i = AS_NUMBER(*(arg + 1));
		break;
	default:
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	const auto& elem = *arg;
	const auto& con = AS_TOBJECT(thisarg, ListObject)->list;

	int count = 0;
	for (; i < con.size(); ++i) {
		if (con[i] == elem) {
			++count;
		}
	}
	NATIVE_RETURN(vm, (double)count);
}

NATIVE_METHOD_SIG(ListObject, list_any) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "No argument is expected");
	}

	for (const auto& elem : AS_TOBJECT(thisarg, ListObject)->list) {
		if (elem) {
			NATIVE_RETURN(vm, true);
		}
	}
	NATIVE_RETURN(vm, false);
}
NATIVE_METHOD_SIG(ListObject, list_all) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count) {
		NATIVE_RAISE_TERROR(vm, "No argument is expected");
	}

	for (const auto& elem : AS_TOBJECT(thisarg, ListObject)->list) {
		if (!elem) {
			NATIVE_RETURN(vm, false);
		}
	}
	NATIVE_RETURN(vm, true);
}

NATIVE_METHOD_SIG(ListObject, list_filter) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto filteredlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		if (*vm->stack_end) {
			filteredlist->push(x);
		}

		vm->stack_end = arg_ptr;
	}

	vm->pool->registerList(filteredlist);

	----vm->stack_end;
	NATIVE_RETURN(vm, filteredlist);
}
NATIVE_METHOD_SIG(ListObject, list_replace) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "2 arguments are expected");
	}

	const auto& find = *arg, &replacement = *(arg + 1);

	for (auto& x : AS_TOBJECT(thisarg, ListObject)->list) {
		if (x == find) {
			x = replacement;
		}
	}

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_replaceif) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 arguments");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	const auto& replacement = *(arg + 1);
	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		if (*vm->stack_end)
			x = replacement;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(ListObject, list_accumulate) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 arguments");
	}
	else if (!IS_CALLABLE(*(arg + 1))) {
		NATIVE_RAISE_TERROR(vm, "Expected second argument to be a function");
	}

	Value accumulation(*arg);
	Object* callee = AS_OBJECT(*(arg + 1));
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;
		*++vm->stack_end = accumulation;
		
		if (!vm->callValue(callee, call_ptr, 2, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		accumulation = *vm->stack_end;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm, accumulation);
}
NATIVE_METHOD_SIG(ListObject, list_each) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_map) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto mappedlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		mappedlist->push(*vm->stack_end);

		vm->stack_end = arg_ptr;
	}

	vm->pool->registerList(mappedlist);

	----vm->stack_end;
	NATIVE_RETURN(vm, mappedlist);
}

NATIVE_METHOD_SIG(ListObject, list_reverse) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	reverse(con.begin(), con.end());

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_sort) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	bool reversed = arg_count && *arg;
	auto& con = AS_TOBJECT(thisarg, ListObject)->list;

	if (arg_count < 2) {
		for (const auto& x : con) {
			if (!IS_NUMBER(x)) {
				NATIVE_RAISE_TERROR(vm, "List members must be numbers");
			}
		}

		sort(con.begin(), con.end(), reversed ? Value_CMP_GREATER_Unchecked : Value_CMP_LESS_Unchecked);
	}
	else if (arg_count == 2) {
		if (!IS_CALLABLE(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "Expected second argument to be a function");
		}

		auto key_list = new pair<double, int>[con.size()];

		Object* callee = AS_OBJECT(*(arg + 1));
		Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

		*call_ptr = *vm->stack_begin;

		int i = 0;
		for (const auto& x : con) {
			*call_ptr = *vm->stack_begin;
			*arg_ptr = x;

			if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
				vm->interpret();
			if (vm->throwing)
				return;

			if (!IS_NUMBER(*vm->stack_end)) {
				NATIVE_RAISE_TERROR(vm, "Expected key function to return numbers");
			}
			key_list[i] = {AS_NUMBER(*vm->stack_end), i};

			vm->stack_end = arg_ptr;
			++i;
		}

		----vm->stack_end;

		if (reversed) {
			sort(key_list, key_list + con.size(), greater<pair<double, int>>());
		}
		else {
			sort(key_list, key_list + con.size());
		}

		auto result = new Value[con.size()];
		for (auto it = key_list + con.size() - 1; it >= key_list; --it) {
			result[it - key_list] = con[it->second];
		}
		memcpy(&con.front(), result, con.size() * sizeof(Value));

		delete[] result;
		delete[] key_list;
	}
	else {
		NATIVE_RAISE_TERROR(vm, "Expected at most 2 arguments");
	}

	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(ListObject, list_shuffle) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	shuffle(con.begin(), con.end(), vm->pool->random_generator);

	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(ListObject, list_sub) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}

	const auto& src_clist = AS_LIST(thisarg)->list;
	if (src_clist.empty()) {
		auto list = new ListObject();
		vm->pool->registerList(list);
		NATIVE_RETURN(vm, list);
	}

	size_t start_pos, end_pos = UINT64_MAX;
	long long step = 1, _t;

	switch (arg_count) {
	case 3:
		if (!IS_NUMBER(*(arg + 2))) {
			NATIVE_RAISE_TERROR(vm, "Third argument must be a number indicating step count");
		}
		step = AS_NUMBER(*(arg + 2));
	case 2:
		if (!IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE_TERROR(vm, "Second argument must be a number indicating end position");
		}
		_t = AS_NUMBER(*(arg + 1));
		if (_t < 0)
			end_pos = _t % src_clist.size() + src_clist.size();
		else
			end_pos = _t;
	case 1:
		if (!IS_NUMBER(*arg)) {
			NATIVE_RAISE_TERROR(vm, "First argument must be a number indicating start position");
		}
		_t = AS_NUMBER(*arg);
		if (_t < 0)
			start_pos = _t % src_clist.size() + src_clist.size();
		else
			start_pos = _t;
		break;
	default: {
		NATIVE_RAISE_TERROR(vm, "Expected at least 1 and no more than 2 arguments, start and end index of the segment");
	}
	}

	end_pos = min(end_pos, src_clist.size());

	auto list = new ListObject();
	auto& clist = list->list;

	if (step == 1) {
		if (start_pos < src_clist.size())
			clist.insert(clist.end(), &src_clist.front() + start_pos, &src_clist.front() + end_pos);
	}
	else {
		auto ptr = src_clist.data();
		if (step < 0) {
			for (size_t i = start_pos; i > end_pos; i += step) {
				clist.push_back(ptr[i]);
			}
		}
		else {
			for (size_t i = start_pos; i < end_pos; i += step) {
				clist.push_back(ptr[i]);
			}
		}
	}

	vm->pool->registerList(list);
	NATIVE_RETURN(vm, list);
}
NATIVE_METHOD_SIG(ListObject, list_cycle) {
	if (!IS_LISTOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a list");
	}
	else if (arg_count != 1 || !IS_NUMBER(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument as cycle index");
	}

	auto& clist = AS_LIST(thisarg)->list;
	int cycle_index = AS_NUMBER(*arg);
	cycle_index %= clist.size();

	if (!cycle_index) {
		NATIVE_RETURN_NULL(vm);
	}
	else if (cycle_index < 0) {
		cycle_index += clist.size();
	}

	const int list_size = clist.size();
	auto temp = new Value[list_size];
	
	for (int i = 0; i < list_size; ++i) {
		temp[(i + cycle_index) % list_size] = clist[i];
	}
	clist.assign(temp, temp + list_size);

	delete[] temp;
	NATIVE_RETURN_NULL(vm);
}

//
// Tuple prototype
//

NATIVE_METHOD_SIG(TupleObject, tuple_size) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}

	NATIVE_RETURN(vm, (double)AS_TUPLE(thisarg)->size);
}

NATIVE_METHOD_SIG(TupleObject, tuple_find) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}

	const auto& tuple = AS_TOBJECT(thisarg, TupleObject);
	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (AS_NUMBER(*(arg + 1)) <= 0) {
			NATIVE_RAISE_TERROR(vm, "Startindex must be a number greater or equal to 0");
		}
		i = AS_NUMBER(*(arg + 1));
		if (tuple->size <= i) {
			NATIVE_RETURN(vm, 0.);
		}
		break;
	default:
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	const auto& elem = *arg;
	const auto start = tuple->list, end = start + tuple->size;

	for (auto it = start + i; it != end; ++it) {
		if (*it == elem) {
			NATIVE_RETURN(vm, (double)(it - start));
		}
	}
	NATIVE_RETURN(vm, -1.);
}
NATIVE_METHOD_SIG(TupleObject, tuple_count) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}

	const auto& tuple = AS_TOBJECT(thisarg, TupleObject);
	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (AS_NUMBER(*(arg + 1)) <= 0) {
			NATIVE_RAISE_TERROR(vm, "Startindex must be a number greater or equal to 0");
		}
		i = AS_NUMBER(*(arg + 1));
		if (tuple->size <= i) {
			NATIVE_RETURN(vm, 0.);
		}
		break;
	default:
		NATIVE_RAISE_TERROR(vm, "Only 1 to 2 arguments is expected");
	}

	const auto& elem = *arg;
	const auto end = tuple->list + tuple->size;

	int count = 0;
	for (auto it = tuple->list + i; it != end; ++it) {
		if (*it == elem) {
			++count;
		}
	}
	NATIVE_RETURN(vm, (double)count);
}

NATIVE_METHOD_SIG(TupleObject, tuple_any) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}

	for (const auto& x : ITER_TUPLE(AS_TUPLE(thisarg))) {
		if (x) {
			NATIVE_RETURN(vm, true);
		}
	}
	NATIVE_RETURN(vm, false);
}
NATIVE_METHOD_SIG(TupleObject, tuple_all) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}

	for (const auto& x : ITER_TUPLE(AS_TUPLE(thisarg))) {
		if (!x) {
			NATIVE_RETURN(vm, false);
		}
	}
	NATIVE_RETURN(vm, true);
}

NATIVE_METHOD_SIG(TupleObject, tuple_each) {
	if (!IS_TUPLEOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (const auto& x : ITER_TUPLE(AS_TUPLE(thisarg))) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(TupleObject, tuple_map) {
	if (!IS_TUPLEOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected 1 argument");
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected first argument to be a function");
	}

	Object* callee = AS_OBJECT(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto mappedlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : ITER_TUPLE(AS_LIST(thisarg))) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		if (!vm->callValue(callee, call_ptr, 1, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		mappedlist->push(*vm->stack_end);

		vm->stack_end = arg_ptr;
	}

	vm->pool->registerList(mappedlist);

	----vm->stack_end;
	NATIVE_RETURN(vm, mappedlist);
}
NATIVE_METHOD_SIG(TupleObject, tuple_accumulate) {
	if (!IS_TUPLEOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "'this' argument must be a tuple");
	}
	else if (arg_count != 2) {
		NATIVE_RAISE_TERROR(vm, "Expected 2 arguments");
	}
	else if (!IS_CALLABLE(*(arg + 1))) {
		NATIVE_RAISE_TERROR(vm, "Expected second argument to be a function");
	}

	Value accumulation(*arg);
	Object* callee = AS_OBJECT(*(arg + 1));
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : ITER_TUPLE(AS_TUPLE(thisarg))) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;
		*++vm->stack_end = accumulation;

		if (!vm->callValue(callee, call_ptr, 2, VMCODE_BREAK))
			vm->interpret();
		if (vm->throwing)
			return;

		accumulation = *vm->stack_end;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm, accumulation);
}

// 
// Set prototype
//

NATIVE_METHOD_SIG(SetObject, set_has) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument as the target");
	}

	NATIVE_RETURN(vm, AS_SET(thisarg)->values.contains(*arg));
}
NATIVE_METHOD_SIG(SetObject, set_size) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}

	NATIVE_RETURN(vm, (double)AS_SET(thisarg)->size());
}

NATIVE_METHOD_SIG(SetObject, set_add) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}

	AS_SET(thisarg)->values.insert(arg, arg + arg_count);
	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(SetObject, set_extend) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_ITERABLE(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single iterable as the argument");
	}

	size_t size = getIterableSize(AS_OBJECT(*arg));
	auto temp = new Value[size];
	vm->pool->itertool->listElements2Mem(AS_OBJECT(*arg), temp);

	AS_SET(thisarg)->values.insert(temp, temp + size);

	delete[] temp;
	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(SetObject, set_pop) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}

	auto& set = AS_SET(thisarg)->values;
	set.erase(set.begin());
	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(SetObject, set_remove) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1) {
		NATIVE_RAISE_TERROR(vm, "Expected a single argument as the target");
	}

	AS_SET(thisarg)->values.erase(*arg);
	NATIVE_RETURN_NULL(vm);
}
NATIVE_METHOD_SIG(SetObject, set_discard_intersection) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& origin_set = AS_SET(*arg)->values;

	AS_SET(thisarg)->values.erase(origin_set.begin(), origin_set.end());
	NATIVE_RETURN_NULL(vm);
}

NATIVE_METHOD_SIG(SetObject, set_union) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	const auto& target_set = AS_SET(*arg)->values;
	auto new_set = new SetObject(AS_SET(thisarg)->values);
	new_set->values.insert(target_set.begin(), target_set.end());

	vm->pool->registerSet(new_set);
	NATIVE_RETURN(vm, new_set);
}
NATIVE_METHOD_SIG(SetObject, set_difference) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& origin_set = AS_SET(*arg)->values, & target_set = AS_SET(thisarg)->values;
	vector<Value> differs;

	auto f1 = origin_set.begin(), l1 = origin_set.end(),
		f2 = target_set.begin(), l2 = target_set.end();
	while (f1 != l1)
	{
		if (f2 == l2) {
			differs.insert(differs.end(), f1, l1);
			break;
		}

		if (*f1 < *f2)
			differs.push_back(*f1++);
		else
		{
			if (!(*f2 < *f1))
				++f1;
			++f2;
		}
	}

	auto new_set = new SetObject(&differs.front(), differs.size());
	vm->pool->registerSet(new_set);
	NATIVE_RETURN(vm, new_set);
}
NATIVE_METHOD_SIG(SetObject, set_intersection) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& origin_set = AS_SET(*arg)->values, & target_set = AS_SET(thisarg)->values;
	vector<Value> intersects;

	auto f1 = origin_set.begin(), l1 = origin_set.end(),
		f2 = target_set.begin(), l2 = target_set.end();
	while (f1 != l1 && f2 != l2)
	{
		if (*f1 < *f2)
			++f1;
		else
		{
			if (!(*f2 < *f1))
				intersects.push_back(*f1++); // *first1 and *first2 are equivalent.
			++f2;
		}
	}

	auto new_set = new SetObject(&intersects.front(), intersects.size());
	vm->pool->registerSet(new_set);
	NATIVE_RETURN(vm, new_set);
}

NATIVE_METHOD_SIG(SetObject, set_issuperset) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& target_set = AS_SET(thisarg)->values;
	
	for (const auto& x : ITER_SET(AS_SET(*arg))) {
		if (!target_set.contains(x)) {
			NATIVE_RETURN(vm, false);
		}
	}

	NATIVE_RETURN(vm, true);
}
NATIVE_METHOD_SIG(SetObject, set_issubset) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& target_set = AS_SET(*arg)->values;

	for (const auto& x : ITER_SET(AS_SET(thisarg))) {
		if (!target_set.contains(x)) {
			NATIVE_RETURN(vm, false);
		}
	}

	NATIVE_RETURN(vm, true);
}
NATIVE_METHOD_SIG(SetObject, set_isdisjoint) {
	if (!IS_SETOBJ(thisarg)) {
		NATIVE_RAISE_TERROR(vm, "Expected 'this' argument to be a set");
	}
	else if (arg_count != 1 || !IS_SET(*arg)) {
		NATIVE_RAISE_TERROR(vm, "Expected a single set as the argument");
	}

	auto& target_set = AS_SET(*arg)->values;

	for (const auto& x : ITER_SET(AS_SET(thisarg))) {
		if (target_set.contains(x)) {
			NATIVE_RETURN(vm, false);
		}
	}

	NATIVE_RETURN(vm, true);
}