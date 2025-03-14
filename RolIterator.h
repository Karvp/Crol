#pragma once
#include "value.h"
#include "vm.h"
#include <algorithm>
#include <vector>

using namespace std;

class Object;
class IterableObject;

class RolIteratorBase {
public:
	RolIteratorBase(){}

	virtual inline bool notsameas(const RolIteratorBase& a) {
		throw exception("Unimplemented behaviour");
	}
	virtual inline const Value deref() {
		throw exception("Unimplemented behaviour");
	}
	virtual inline const Value next() {
		throw exception("Unimplemented behaviour");
	}
	virtual inline const void next_noret() {
		throw exception("Unimplemented behaviour");
	}
	virtual inline const void feed(const Value& v) {

	}
};

template <typename iter_t>
class RolCustomIterator : public RolIteratorBase {
public:
	iter_t it;

	RolCustomIterator() {}
	RolCustomIterator(const iter_t& it) : it(it) {}

	inline bool notsameas(const RolIteratorBase& a) override {
		return it != ((RolCustomIterator<iter_t>*)&a)->it;
	}
	inline const Value deref() override {
		return *it;
	}
	inline const void next_noret() override {
		++it;
	}
	inline const Value next() override {
		return *it++;
	}
};

typedef RolCustomIterator<Value*> RolLinearIterator;
typedef RolCustomIterator<rol_set_iterator> RolSetIterator;

class RolListIterator : public RolIteratorBase {
public:
	rol_list_container* vec;
	size_t index;

	inline constexpr size_t getIndex() const noexcept {
		return min(index, vec->size());
	}

	RolListIterator(const rol_list_container& v, size_t i = 0) : vec((rol_list_container*)&v), index(i) {}

	inline bool notsameas(const RolIteratorBase& a) override {
		return getIndex() != ((const RolListIterator*)&a)->getIndex();
	}
	inline const Value deref() override {
		return vec->at(index);
	}
	inline const void next_noret() override {
		++index;
	}
	inline const Value next() override {
		return vec->at(index++);
	}
};

class RolStringIterator : public RolIteratorBase {
public:
	rol_string_iterator it;
	StringObject** characters;

	RolStringIterator(const rol_string_iterator& it, StringObject** characters) : it(it), characters(characters) {}

	inline bool notsameas(const RolIteratorBase& a) override {
		return it != ((RolStringIterator*)&a)->it;
	}
	inline const Value deref() override {
		return toValue((Object*)characters[127 + *it]);
	}
	inline const void next_noret() override {
		++it;
	}
	inline const Value next() override {
		return toValue((Object*)characters[127 + *it++]);
	}
};

class RolObjectIterator : public RolIteratorBase {
public:
	dynamic_map::iterator it;

	RolObjectIterator(const dynamic_map::iterator& it) : it(it) {}

	inline bool notsameas(const RolIteratorBase& a) noexcept override {
		return it != ((RolObjectIterator*)&a)->it;
	}
	inline const Value deref() noexcept override {
		return it->first;
	}
	inline const void next_noret() noexcept override {
		++it;
	}
	inline const Value next() noexcept override {
		return (it++)->first;
	}
};

// The problem arises within the generator's mechanic
// is that it will use the vm and change its current states
// i.e. stack_end will change,...
class RolGeneratorIterator : public RolIteratorBase {
public:
	struct GeneratorIterationException {};


	CallFrame frame;
	VM* vm;
	Value* locals = NULL;
	chunk_size locals_count = 0;
	bool finished = false, ignition = true;
	Value last_yield_val, yield_return_val;

	// `End` iterator constructor
	RolGeneratorIterator() : finished(true), ignition(false) {}

	// `Begin` iterator constructor
	RolGeneratorIterator(VM* vm, ClosureObject* callee, Value* arg, chunk_size arg_count) :
		frame(callee, NULL, false, VMCODE_BREAK),
		vm(vm),
		locals_count(arg_count),
		locals(arg_count ? (Value*)malloc(arg_count) : (Value*)NULL)
	{
		if (locals)
			memcpy(locals, arg, sizeof(Value) * locals_count);
	}

	~RolGeneratorIterator() {
		freeLocalsArray();
	}

	inline bool notsameas(const RolIteratorBase& a) override {
		return finished != ((RolGeneratorIterator*)&a)->finished;
	}
	inline const Value deref() override {
		if (ignition) {
			this->next_noret();
		}
		return last_yield_val;
	}
	inline const void next_noret() override {
		if (finished)
			return;

		// Make the stack_begin to be the stack_end increased by 1
		frame.stack_begin = ++vm->stack_end;
		// First insert our own frame onto the call stack
		vm->newCallFrame(frame);

		// Then insert the locals array that we have snapshot (if we had)
		if (locals_count) {
			memcpy(vm->stack_end, locals, sizeof(Value) * locals_count);
			vm->stack_end += locals_count - 1;
		}
		if (!ignition) {
			*++vm->stack_end = yield_return_val;
			yield_return_val = vm->pool->ValueConstNull;
		}
		ignition = false;

		// Execute the generator...

		switch (vm->interpret()) {
		case EXEC_HALTED_BY_INTERNAL:
			finished = true;
		case EXEC_HALTED_BY_YIELD:
			last_yield_val = *vm->stack_end--;
			break;
		case EXEC_HALTED_BY_EXCEPTION:
			finished = true;
			freeLocalsArray();
			return;
		default:
			throw exception("Bad interpretation return code");
		}

		vm->closureCalls.pop_back();
		if (finished) {
			// Execution finished! Lets free the locals array
			freeLocalsArray();
		}
		else {
			// Now let's capture locals array
			locals_count = vm->stack_end - vm->stack_begin + 1;
			locals = REALLOCATE(locals, locals_count);
			// locals' always > 0
			memcpy(locals, vm->stack_begin, sizeof(Value) * locals_count);
			vm->stack_end -= locals_count;

			// Then retake the frame
			frame = vm->frames.back();
			frame.ops = vm->ops;

			// Return everything to its original state...
			vm->frames.pop_back();
			vm->updateLocals();
		}
	}
	inline const Value next() override {
		this->next_noret();
		return last_yield_val;
	}
	inline const void feed(const Value& v) override {
		this->yield_return_val = v;
	}

protected:
	void freeLocalsArray() {
		if (locals_count) {
			locals_count = 0;
			free(locals);
			locals = NULL;
		}
	}
};

class RolIterator {
public:
	RolIteratorBase* it;
	const size_t it_size;

	template <typename roliter_t>
	RolIterator(const roliter_t& it) : it(new roliter_t(it)), it_size(sizeof(roliter_t)) {}

	// Initialize a copy of the iterator
	RolIterator(const RolIterator& rolit) : 
		it((RolIteratorBase*)malloc(rolit.it_size)),
		it_size(rolit.it_size) 
	{
		if (!it)
			throw exception("Invalid iterator base pointer value");

		memcpy(it, rolit.it, rolit.it_size);
	}

	~RolIterator() {
		delete it;
	}

	inline bool operator!=(const RolIterator& a) const {
		return it->notsameas(*a.it);
	}
	inline RolIterator& operator++() const {
		it->next_noret();
		return *(RolIterator*)this;
	}
	inline const Value operator*() const {
		return it->deref();
	}
};

class RolIterable {
private:
	RolIterator b, e;

public:
	template <typename roliter_t>
	RolIterable(const roliter_t& begin, const roliter_t& end) : b(begin), e(end) {}

	inline constexpr RolIterator& begin() const noexcept {
		return *(RolIterator*)&b;
	}

	inline constexpr RolIterator& end() const noexcept {
		return *(RolIterator*)&e;
	}

	inline bool next(Value& v) const {
		if (b != e) {
			v = b.it->next();
			return true;
		}
		return false;
	}
};


class RolContainerHelper {
public:
	RolContainerHelper(){}

	virtual inline void add_element(const Value& v) {
		throw exception("Unimplemented behaviour");
	}
	virtual inline void append_linear_range(Value* begin, Value* end) {
		throw exception("Unimplemented behaviour");
	}
	virtual inline void append_custom_range(RolIterator begin, const RolIterator& end) {
		throw exception("Unimplemented behaviour");
	}
};

class RolVectorHelper : public RolContainerHelper {
public:
	vector<Value>& container;

	RolVectorHelper(vector<Value>& vec) : container(vec) {}

	inline void add_element(const Value& v) override {
		container.push_back(v);
	}
	inline void append_linear_range(Value* begin, Value* end) override {
		container.insert(container.end(), begin, end);
	}
	inline void append_custom_range(RolIterator begin, const RolIterator& end) override {
		for (; begin != end; ++begin) {
			container.push_back(*begin);
		}
	}
};

class RolSetHelper : public RolContainerHelper {
public:
	rol_set_container& container;

	RolSetHelper(rol_set_container& sett) : container(sett) {}

	inline void add_element(const Value& v) override {
		container.insert(v);
	}
	inline void append_linear_range(Value* begin, Value* end) override {
		container.insert(begin, end);
	}
	inline void append_custom_range(RolIterator begin, const RolIterator& end) override {
		container.insert(begin, end);
	}
};

class RolStringHelper : public RolContainerHelper {
public:
	string& container;

	RolStringHelper(string& str) : container(str) {}

	inline void add_element(const Value& v) override {
		container.push_back(v);
	}
	inline void append_linear_range(Value* begin, Value* end) override;
	inline void append_custom_range(RolIterator begin, const RolIterator& end) override;
};


// Iterator tools hub helping with iterateration through rol object
// Also supporting others functionalities like listing or appending
// result of the iteration
// 
class RolIterTools {
public:
	ValuePool* pool;

	RolIterTools(ValuePool* pool) : pool(pool) {}

	// Produce an iterator through all of
	// the object's elements
	RolIterable iterateObject(Object* object) const;

	// Produce an iterable object
	inline IterableObject* produceIterable(Object* object) const;

	// Writes all of the object's elements onto a continuous linear memory address,
	// step by step
	void listElements2Mem(Object* object, Value*);

	// Expands the object's elements onto a vector
	// but faster than `iterateObject(obj).extendto()`
	void listElements2Vec(Object* object, vector<Value>&);

	// Lists all elements onto pointers,
	// starting from the given pointer, then continue
	// depending on the implemented behaviour for the pointer
	template <typename iter_t>
	inline chunk_size listElements2Ptr(Object* object, iter_t);

	// Append all elements to the underlying container
	// Automatically halts if an exception is thrown
	void addElements2Container(Object* source, Object* dest);

	// Append all elements to a vector
	void addElements2Vec(Object* source, vector<Value>& dest);
};

// Return size of the iterable
// If the object is not iterable or its size is not defined, return UNSIZED_ITERABLE
inline size_t getIterableSize(Object*);

// Return a helper of the iterable
inline RolContainerHelper* getContainerHelper(Object*);