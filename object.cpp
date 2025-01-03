#include "object.h"
#include "value.h"
#include "vm.h"
#include <algorithm>

using namespace std;

// Internal implementations

Value StringObject::get(const Value& key, VM* vm) {
	if (IS_NUMBER(key)) {
		double num = AS_NUMBER(key);
		if (num >= 0 && num < content.size()) {
			return vm->pool->character_strings[127 + content[(size_t)num]];
		}
		return Value();
	}

	return Object::get(key, vm);
}

Object* FunctionObject::obj_copy() const {
	auto obj = this->create_copy<FunctionObject>(fnname, chunk->copy(), arity);
	obj->upvalue_count = this->upvalue_count;
	return obj;
}

// Object prototype implementation

NATIVE_METHOD_SIG(Object, copy) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be an object"));
	}
	else if (IS_STRING(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument should not be a string"));
	}
	else if (arg_count) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "No argument is expected"));
	}

	auto obj = AS_OBJECT(thisarg)->obj_copy();
	obj->props = AS_OBJECT(thisarg)->props;

	NATIVE_RETURN(vm, obj);
}

NATIVE_METHOD_SIG(Object, has) {
	if (!IS_OBJECT(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be an object"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected"));
	}

	NATIVE_RETURN(vm, AS_OBJECT(thisarg)->has(*arg));
}

// String prototype

NATIVE_METHOD_SIG(StringObject, repeat) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected"));
	}
	else if (!IS_NUMBER(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a number"));
	}
	
	const char* str = AS_STRING(thisarg)->content.c_str();
	string newstr;
	
	for (size_t i = max(AS_NUMBER(*arg), 0.); i; --i) {
		newstr.append(str);
	}

	NATIVE_RETURN(vm, vm->pool->internString(newstr));
}

NATIVE_METHOD_SIG(StringObject, substr) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}

	if (arg_count == 1) {
		// (start_pos,)
		if (!IS_NUMBER(*arg)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a number"));
		}
		NATIVE_RETURN(vm, vm->pool->internString(string(AS_STRING(thisarg)->content, max(AS_NUMBER(*arg), 0.))));
	}
	else if (arg_count == 2) {
		// (start_pos, length)
		if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First and second arguments must be a number"));
		}
		NATIVE_RETURN(vm, vm->pool->internString(string(AS_STRING(thisarg)->content, max(AS_NUMBER(*arg), 0.), max(AS_NUMBER(*(arg + 1)), 0.))));
	}
	else {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
	}
}

NATIVE_METHOD_SIG(StringObject, find) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count != 1 && arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
	}

	if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a string"));
	}

	size_t start = string::npos;

	if (arg_count == 2) {
		if (!IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Startindex must be a number"));
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	} else if (arg_count != 1 && arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
	}

	if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a string"));
	}

	size_t start = string::npos;

	if (arg_count == 2) {
		if (!IS_NUMBER(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Second argument must be a number"));
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "0 argument is expected"));
	}

	const auto& original = AS_STRING(thisarg)->content;

	NATIVE_RETURN(vm, vm->pool->internString(string(original.rbegin(), original.rend())));
}

NATIVE_METHOD_SIG(StringObject, split) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count && arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 0 to 1 argument is expected"));
	}

	const auto& original = AS_STRING(thisarg)->content;
	ListObject* tokens = NULL;

	if (arg_count) {
		if (!IS_STRINGOBJ(*arg)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a string"));
		}

		const auto& delim = AS_STRING(*arg)->content;
		tokens = new ListObject();

		size_t last = 0, next = 0;
		while ((next = original.find(delim, last)) != string::npos) {
			tokens->push(vm->pool->internString(original.substr(last, next - last), true));
			last = next + 1;
		}
		tokens->push(vm->pool->internString(original.substr(last), true));
	}
	else {
		tokens = new ListObject();

		for (const auto& c : original) {
			tokens->push(Value(vm->pool->character_strings[127 + c]));
		}
	}

	vm->pool->registerList(tokens);
	NATIVE_RETURN(vm, tokens);
}

NATIVE_METHOD_SIG(StringObject, join) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_OBJECT(*arg) || (!IS_LIST(*arg) && !IS_TUPLE(*arg))) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a tup or tuple"));
	}

	size_t size = 0;
	const Value* ptr = NULL;

	switch (AS_OBJECT(*arg)->type) {
	case OBJECT_LIST: {
		const auto& list = AS_TOBJECT(*arg, ListObject)->list;
		size = list.size();
		ptr = &list.front();
		break;
	}
	case OBJECT_TUPLE: {
		auto tup = AS_TOBJECT(*arg, TupleObject);
		size = tup->size;
		ptr = tup->list;
		break;
	}
	}

	if (!size) {
		NATIVE_RETURN(vm, vm->pool->internString(""));
	}

	const auto& delim = AS_STRING(thisarg)->content;
	string result;

	while (--size) {
		if (!IS_STRINGOBJ(*ptr)) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Elements must be strings"));
		}
		result.append(AS_STRING(*ptr++)->content);
		result.append(delim);
	}
	result.append(AS_STRING(*ptr++)->content);

	NATIVE_RETURN(vm, vm->pool->internString(result));
}

NATIVE_METHOD_SIG(StringObject, replace) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "2 arguments is expected"));
	} else if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First and second arguments must be strings"));
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

	NATIVE_RETURN(vm, vm->pool->internString(result));
}

NATIVE_METHOD_SIG(StringObject, count) {
	if (!IS_STRINGOBJ(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a string"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected"));
	} else if (!IS_STRINGOBJ(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "First argument must be a string"));
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

NATIVE_FN_SIG(function_bind) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a function"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 argument is expected"));
	}

	auto bound_func = new BoundFunctionObject(IS_BOUND_FUNCTION(thisarg) ? AS_TOBJECT(thisarg, BoundFunctionObject)->function : AS_OBJECT(thisarg), *arg);
	vm->pool->registerObject(bound_func);
	NATIVE_RETURN(vm, (Object*)bound_func);
}

NATIVE_FN_SIG(function_arity) {
	if (!IS_OBJECT(thisarg) || !IS_CLOSURE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a defined function"));;
	}
	else if (arg_count) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "No argument is expected"));;
	}

	NATIVE_RETURN(vm, (double)AS_CLOSURE(thisarg)->function->arity);
}

NATIVE_FN_SIG(function_apply) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a function"));;
	}

	vm->callValue(AS_OBJECT(thisarg), arg, arg_count - 1, true);
}

NATIVE_FN_SIG(function_invoke) {
	if (!IS_CALLABLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a function"));
	}

	const Value invokeas = *arg, callee = thisarg;

	Object* args = AS_OBJECT(*(arg + 1));
	ushort s_arg_count;

	vm->stack_end -= arg_count;
	*++vm->stack_end = callee;
	*++vm->stack_end = invokeas;

	switch (args->type) {
	case OBJECT_LIST:
		for (auto& x : *(ListObject*)args) {
			*++vm->stack_end = x;
		}
		s_arg_count = ((ListObject*)args)->size();
		break;
	case OBJECT_TUPLE:
		for (auto& x : *(TupleObject*)args) {
			*++vm->stack_end = x;
		}
		s_arg_count = ((TupleObject*)args)->size;
		break;
	default:
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Second parameter must be a tup or tuple"));
	}

	vm->callValue(AS_OBJECT(callee), vm->stack_end - s_arg_count, s_arg_count, true);
}


// List prototype

NATIVE_METHOD_SIG(ListObject, list_size) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	NATIVE_RETURN(vm, (double)AS_LIST(thisarg)->list.size());
}

NATIVE_METHOD_SIG(ListObject, list_push) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	con.insert(con.end(), arg, arg + arg_count);

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_pop) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;

	switch (arg_count) {
	case 0:
		con.pop_back();
		break;
	case 1: {
		if (!IS_NUMBER(*arg) || AS_NUMBER(*arg) < 0) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Second argument must be a number greater or equal to 0"));
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected at most 1 argument"));
	}

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_swap) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "2 arguments are expected"));
	}
	else if (!IS_NUMBER(*arg) || !IS_NUMBER(*(arg + 1))) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Arguments of list.swap function must be numbers"));
	}

	auto& list = AS_LIST(thisarg)->list;

	const Value temp = list[AS_NUMBER(*arg)];
	list[AS_NUMBER(*arg)] = list[AS_NUMBER(*(arg + 1))];
	list[AS_NUMBER(*(arg + 1))] = temp;

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_clear) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	AS_TOBJECT(thisarg, ListObject)->list.clear();
	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_find) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (!IS_NUMBER(*(arg + 1)) || AS_NUMBER(*(arg + 1)) < 0) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Second argument must be a number greater or equal to 0"));
		}
		i = AS_NUMBER(*(arg + 1));
		break;
	default:
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
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

NATIVE_METHOD_SIG(ListObject, list_count) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (!IS_NUMBER(*(arg + 1)) || AS_NUMBER(*(arg + 1)) < 0) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Startindex must be a number greater or equal to 0"));
		}
		i = AS_NUMBER(*(arg + 1));
		break;
	default:
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
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
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "No argument is expected"));
	}

	for (const auto& elem : AS_TOBJECT(thisarg, ListObject)->list) {
		if (elem) {
			NATIVE_RETURN(vm, true);
		}
	}
	NATIVE_RETURN(vm, false);
}

NATIVE_METHOD_SIG(ListObject, list_all) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "No argument is expected"));
	}

	for (const auto& elem : AS_TOBJECT(thisarg, ListObject)->list) {
		if (!elem) {
			NATIVE_RETURN(vm, false);
		}
	}
	NATIVE_RETURN(vm, true);
}

NATIVE_METHOD_SIG(ListObject, list_replace) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "2 arguments are expected"));
	}

	const auto& find = *arg, &replacement = *(arg + 1);

	for (auto& x : AS_TOBJECT(thisarg, ListObject)->list) {
		if (x == find) {
			x = replacement;
		}
	}

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_replaceif) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 2 arguments"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	const auto& replacement = *(arg + 1);
	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
			vm->interpret();
		if (vm->throwing)
			return;

		if (*vm->stack_end)
			x = replacement;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_accumulate) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 2 arguments"));
	}
	else if (!IS_CALLABLE(*(arg + 1))) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected second argument to be a function"));
	}

	Value accumulation(*arg);
	Object* callee = AS_OBJECT(*(arg + 1));
	const bool isDefined = IS_DEFINED_FUNCTION(*(arg + 1));
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;
		*++vm->stack_end = accumulation;

		vm->callValue(callee, call_ptr, 2, false, VMCODE_BREAK);
		if (isDefined)
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
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
			vm->interpret();
		if (vm->throwing)
			return;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_map) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto mappedlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
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

NATIVE_METHOD_SIG(ListObject, list_filter) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto filteredlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : AS_LIST(thisarg)->list) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
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

NATIVE_METHOD_SIG(ListObject, list_reverse) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	reverse(con.begin(), con.end());

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_sort) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	bool reversed = arg_count && *arg;
	auto& con = AS_TOBJECT(thisarg, ListObject)->list;

	if (arg_count < 2) {
		for (const auto& x : con) {
			if (!IS_NUMBER(x)) {
				NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "List members must be numbers"));
			}
		}

		sort(con.begin(), con.end(), reversed ? Value_CMP_GREATER_Unchecked : Value_CMP_LESS_Unchecked);
	}
	else if (arg_count == 2) {
		if (!IS_CALLABLE(*(arg + 1))) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected second argument to be a function"));
		}

		auto key_list = new pair<double, int>[con.size()];

		Object* callee = AS_OBJECT(*(arg + 1));
		const bool isDefined = IS_DEFINED_FUNCTION(*(arg+1));
		Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

		*call_ptr = *vm->stack_begin;

		int i = 0;
		for (const auto& x : con) {
			*call_ptr = *vm->stack_begin;
			*arg_ptr = x;

			vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
			if (isDefined)
				vm->interpret();
			if (vm->throwing)
				return;

			if (!IS_NUMBER(*vm->stack_end)) {
				NATIVE_RAISE(vm, "Expected key function to return numbers");
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected at most 2 arguments"));
	}

	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(ListObject, list_shuffle) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a list"));
	}

	auto& con = AS_TOBJECT(thisarg, ListObject)->list;
	shuffle(con.begin(), con.end(), vm->pool->random_generator);

	NATIVE_RETURN(vm);
}

//
// Tuple prototype
//

NATIVE_METHOD_SIG(TupleObject, tuple_size) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}

	NATIVE_RETURN(vm, (double)AS_TUPLE(thisarg)->size);
}

NATIVE_METHOD_SIG(TupleObject, tuple_find) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}

	const auto& tuple = AS_TOBJECT(thisarg, TupleObject);
	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (AS_NUMBER(*(arg + 1)) <= 0) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Startindex must be a number greater or equal to 0"));
		}
		i = AS_NUMBER(*(arg + 1));
		if (tuple->size <= i) {
			NATIVE_RETURN(vm, 0.);
		}
		break;
	default:
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}

	const auto& tuple = AS_TOBJECT(thisarg, TupleObject);
	size_t i = 0;

	switch (arg_count) {
	case 1:
		break;
	case 2:
		if (AS_NUMBER(*(arg + 1)) <= 0) {
			NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Startindex must be a number greater or equal to 0"));
		}
		i = AS_NUMBER(*(arg + 1));
		if (tuple->size <= i) {
			NATIVE_RETURN(vm, 0.);
		}
		break;
	default:
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Only 1 to 2 arguments is expected"));
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
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}

	for (const auto& x : *AS_TOBJECT(thisarg, TupleObject)) {
		if (x) {
			NATIVE_RETURN(vm, true);
		}
	}
	NATIVE_RETURN(vm, false);
}

NATIVE_METHOD_SIG(TupleObject, tuple_all) {
	if (!IS_TUPLE(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}

	for (const auto& x : *AS_TOBJECT(thisarg, TupleObject)) {
		if (!x) {
			NATIVE_RETURN(vm, false);
		}
	}
	NATIVE_RETURN(vm, true);
}

NATIVE_METHOD_SIG(TupleObject, tuple_each) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	const auto& tup = *AS_TUPLE(thisarg);
	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (const auto& x : tup) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
			vm->interpret();
		if (vm->throwing)
			return;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm);
}

NATIVE_METHOD_SIG(TupleObject, tuple_map) {
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}
	else if (arg_count != 1) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 1 argument"));
	}
	else if (!IS_CALLABLE(*arg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected first argument to be a function"));
	}

	const auto& tup = *AS_TUPLE(thisarg);
	Object* callee = AS_OBJECT(*arg);
	const bool isDefined = IS_DEFINED_FUNCTION(*arg);
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	auto mappedlist = new ListObject();

	*call_ptr = *vm->stack_begin;

	for (const auto& x : tup) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;

		vm->callValue(callee, call_ptr, 1, false, VMCODE_BREAK);
		if (isDefined)
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
	if (!IS_LIST(thisarg)) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "'this' argument must be a tuple"));
	}
	else if (arg_count != 2) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected 2 arguments"));
	}
	else if (!IS_CALLABLE(*(arg + 1))) {
		NATIVE_RAISE(vm, NATIVE_TYPE_ERROR(vm, "Expected second argument to be a function"));
	}

	Value accumulation(*arg);
	Object* callee = AS_OBJECT(*(arg + 1));
	const bool isDefined = IS_DEFINED_FUNCTION(*(arg+1));
	Value* call_ptr = ++vm->stack_end, * arg_ptr = ++vm->stack_end;

	*call_ptr = *vm->stack_begin;

	for (auto& x : *AS_TUPLE(thisarg)) {
		*call_ptr = *vm->stack_begin;
		*arg_ptr = x;
		*++vm->stack_end = accumulation;

		vm->callValue(callee, call_ptr, 2, false, VMCODE_BREAK);
		if (isDefined)
			vm->interpret();
		if (vm->throwing)
			return;

		accumulation = *vm->stack_end;

		vm->stack_end = arg_ptr;
	}

	----vm->stack_end;
	NATIVE_RETURN(vm, accumulation);
}