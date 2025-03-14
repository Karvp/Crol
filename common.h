#pragma once
#include "token.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>


#define UNSIZED_ITERABLE UINT64_MAX
#define VAR_BASET(v) baset_of<decltype(v)>
#define VAR_SIZET(v) sizet_of<decltype(v)>

#define STACK_MAX_SIZE 4096 * 4
#define CALL_STACK_MAX_COUNT 500
#define GC_HEAP_GROW_FACTOR 2

#define MAX_CHUNK_SIZE UINT16_MAX
#define MAX_SCOPE_CHILDREN 255
#define MAX_SCOPE_DEPTH 255

// Object architecture settings
#define FLAG_BYTESIZE 8

#if FLAG_BYTESIZE == 32
#define FLAG_TYPE uint32_t
#elif FLAG_BYTESIZE == 16
#define FLAG_TYPE uint16_t
#elif FLAG_BYTESIZE == 8
#define FLAG_TYPE uint8_t
#endif

//#define GC_SHOW_SUMMARY
#define GC_SHOW_DEALLOCS
#define NAN_BOXING

// Alternative modes
#define PRINT_CHUNK_AFTER_COMPILED


#define REALLOC(ptr, size, type) ((type*)realloc((ptr), (size) * sizeof(type)))
#define REALLOCATE(ptr, size) ((decltype(ptr))(ptr ? realloc(ptr, VAR_SIZET(ptr) * size) : malloc(VAR_SIZET(ptr) * size)))

// Specification macros

#define NT_HELPER_FN(name) __helper_##name
#define NT_HELPER_FN_SIG(name) void NT_HELPER_FN(name)

#define NATIVE_FN(name) __native_##name
#define NATIVE_METHOD(classname, name) classname::NATIVE_FN(name)
#define NATIVE_FN_ARG_SIG (VM* vm, const Value& thisarg, Value* arg, const size_t arg_count)
#define NATIVE_FN_SIG(name) void NATIVE_FN(name)NATIVE_FN_ARG_SIG
#define NATIVE_METHOD_SIG(classname, name) void NATIVE_METHOD(classname, name)NATIVE_FN_ARG_SIG

#define NATIVE_LHS_RETURNEXP(vm) *((vm)->stack_end -= arg_count)
#define NATIVE_RETURN(vm, retexp) NATIVE_LHS_RETURNEXP(vm) = toValue(retexp); return;
#define NATIVE_RETURN_NOWRAP(vm, retexp) NATIVE_LHS_RETURNEXP(vm) = (retexp); return;

#define NATIVE_RAISE(vm, value) (vm)->throwValue(value); return;

#define NATIVE_TYPE_ERROR(vm, msg) ((vm)->newException(msg, EXCEPT_TYPE_ERROR))
#define NATIVE_EXECUTION_ERROR(vm) ((vm)->newException("Internal error.", EXCEPT_INTERNAL_ERROR))
#define NATIVE_RAISE_TERROR(vm, msg) NATIVE_RAISE((vm), toValue((Object*) NATIVE_TYPE_ERROR((vm), msg)))

using namespace std;


typedef unsigned short ushort;

typedef uint8_t bytecode;
typedef uint8_t scope_size;
typedef uint16_t chunk_size;


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

template <typename T>
using baset_of = base_type<T>::T;

template <typename T>
constexpr size_t sizet_of = base_type<T>::size;

template <typename T>
using pair_of = pair<T, T>;

template <typename _T>
struct MemoryArray {
	_T* ptr;

	MemoryArray(_T* ptr) : ptr(ptr) {}
	~MemoryArray() {
		delete[] ptr;
	}
};