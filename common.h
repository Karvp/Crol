#pragma once
#include "token.h"
#include <string>

#define STACK_MAX_SIZE 4096 * 4
#define GC_HEAP_GROW_FACTOR 2
//#define GC_SHOW_SUMMARY
//#define GC_SHOW_DEALLOCS
#define VM_SHOW_SNAPSHOTTING
#define NAN_BOXING


class VM;

#define NATIVE_FN(name) __native_##name
#define NATIVE_METHOD(classname, name) classname::NATIVE_FN(name)
#define NATIVE_FN_ARG_SIG (VM* vm, const Value& thisarg, Value* arg, const size_t arg_count)
#define NATIVE_FN_SIG(name) void NATIVE_FN(name)NATIVE_FN_ARG_SIG
#define NATIVE_METHOD_SIG(classname, name) void NATIVE_METHOD(classname, name)NATIVE_FN_ARG_SIG

#define NATIVE_LHS_RETURNEXP(vm) *((vm)->stack_end -= arg_count)
#define NATIVE_RETURN(vm, retexp) NATIVE_LHS_RETURNEXP(vm) = Value(retexp); return;
#define NATIVE_RETURN_NOWRAP(vm, retexp) NATIVE_LHS_RETURNEXP(vm) = (retexp); return;

#define NATIVE_RAISE(vm, value) (vm)->throwValue(value); return;

#define NATIVE_TYPE_ERROR(vm, msg) ((vm)->newException<TypeErrorObject>(msg))
#define NATIVE_EXECUTION_ERROR(vm) ((vm)->newException<InternalErrorObject>("Internal error."))

using namespace std;

typedef unsigned short ushort;

string tokenToString(const Token* token);



template <typename T>
struct base_type;

template <typename T>
struct base_type<T&> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type<T*> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type<const T> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type<const T&> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type<const T*> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type<const T*&> {
	typedef T T;
	typedef base_type<T> R;
	static inline constexpr size_t size = sizeof(T);
};

template <typename T>
struct base_type {
	typedef T T;
	static inline constexpr size_t size = sizeof(T);
};