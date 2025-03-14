#include "RolIterator.h"
#include "object.h"

using namespace std;

inline void RolStringHelper::append_linear_range(Value* begin, Value* end) {
	for (; begin != end; ++begin) {
		container.append(AS_TOBJECT(*begin, StringObject)->content);
	}
}
inline void RolStringHelper::append_custom_range(RolIterator begin, const RolIterator& end) {
	for (; begin != end; ++begin) {
		container.append(AS_TOBJECT(*begin, StringObject)->content);
	}
}

RolIterable RolIterTools::iterateObject(Object* object) const {
	switch (object->type) {
	case OBJECT_ITERABLE:
		return ((IterableObject*)object)->rol_it;
	case OBJECT_LIST: {
		auto list = (ListObject*)object;
		return RolIterable(
			RolListIterator(list->list),
			RolListIterator(list->list, list->list.size())
		);
	}
	case OBJECT_TUPLE: {
		auto range = GET_ITER_SPECIE(object, Value*);
		return RolIterable(
			RolLinearIterator(range->iter_begin()),
			RolLinearIterator(range->iter_end())
		);
	}
	case OBJECT_SET: {
		auto range = GET_ITER_SPECIE(object, rol_set_iterator);
		return RolIterable(
			RolSetIterator(range->iter_begin()),
			RolSetIterator(range->iter_end())
		);
	}
	case OBJECT_STRING: {
		auto range = GET_ITER_SPECIE(object, rol_string_iterator);
		return RolIterable(
			RolStringIterator(range->iter_begin(), pool->character_strings),
			RolStringIterator(range->iter_end(), pool->character_strings)
		);
	}
	default: {
		auto range = ITER_PURE_OBJ(object);
		return RolIterable(
			RolObjectIterator(range.begin()),
			RolObjectIterator(range.end())
		);
	}
	}
}

inline IterableObject* RolIterTools::produceIterable(Object* object) const {
	return new IterableObject(iterateObject(object), pool);
}

void RolIterTools::listElements2Mem(Object* object, Value* ptr) {
	switch (object->type) {
	case OBJECT_LIST: {
		auto range = ITER_LIST(object);
		memcpy(ptr, range.begin(), (range.end() - range.begin()) * sizeof(Value));
		break;
	}
	case OBJECT_TUPLE: {
		auto range = ITER_TUPLE(object);
		memcpy(ptr, range.begin(), (range.end() - range.begin()) * sizeof(Value));
		break;
	}
	case OBJECT_SET: {
		for (const auto& x : ITER_SET(object))
			*ptr++ = x;
		break;
	}
	case OBJECT_STRING: {
		for (const auto& x : ITER_STRING(object))
			*ptr++ = toValue(pool->character_strings[127 + x]);
		break;
	}
	default: {
		for (const auto& x : ITER_PURE_OBJ(object))
			*ptr++ = x.first;
		break;
	}
	}
}

void RolIterTools::listElements2Vec(Object* object, vector<Value>& vec) {
	Value* ptr;
	size_t size = getIterableSize(object);

	if (size == UNSIZED_ITERABLE) {
		throw exception("Listing unsized iterable");
	}

	if (vec.empty()) {
		vec.resize(size);
		ptr = vec.empty() ? NULL : &vec.front();
	}
	else {
		ptr = &vec.back() + 1;
		vec.resize(vec.size() + size);
	}
	listElements2Mem(object, ptr);
}

template <typename iter_t>
inline chunk_size RolIterTools::listElements2Ptr(Object* object, iter_t ptr) {
	chunk_size count(0);

	switch (object->type) {
	case OBJECT_LIST:
		for (const auto& x : ITER_LIST(object)) {
			*ptr = x;
			++ptr;
			++count;
		}
		return count;
	
	case OBJECT_TUPLE:
		for (const auto& x : ITER_TUPLE(object)) {
			*ptr = x;
			++ptr;
			++count;
		}
		return count;
	
	case OBJECT_SET:
		for (const auto& x : ITER_SET(object)) {
			*ptr = x;
			++ptr;
			++count;
		}
		return count;
	
	case OBJECT_STRING:
		for (const auto& x : ITER_STRING(object)) {
			*ptr = pool->character_strings[127 + x];
			++ptr;
			++count;
		}
		return count;
	
	case OBJECT_ITERABLE:
		for (const auto& x : ITER_ITERABLE(object)) {
			*ptr = x;
			++ptr;
			++count;
		}
		return count;

	default:
		for (const auto& x : ITER_PURE_OBJ(object)) {
			*ptr = x.first;
			++ptr;
			++count;
		}
		return count;
	
	}
}

void RolIterTools::addElements2Container(Object* source, Object* dest) {
	auto iterable = this->iterateObject(source);
	auto& begin = iterable.begin(), & end = iterable.end();

	switch (dest->type) {
	case OBJECT_LIST: {
		auto& vec = ((ListObject*)dest)->list;
		for (; begin != end; ++begin) {
			if (pool->vm->throwing)
				return;

			vec.push_back(*begin);
		}
		break;
	}
	case OBJECT_STRING: {
		auto& str = ((StringObject*)dest)->content;
		for (; begin != end; ++begin) {
			if (pool->vm->throwing)
				return;

			str.append(AS_TOBJECT(*begin, StringObject)->content);
		}
		break;
	}
	case OBJECT_SET: {
		vector<Value> vec;
		for (; begin != end; ++begin) {
			if (pool->vm->throwing)
				return;

			vec.push_back(*begin);
		}

		((SetObject*)dest)->values.insert(vec.begin(), vec.end());
		break;
	}
	default:
		throw exception("Unsupported container type");
	}
}

void RolIterTools::addElements2Vec(Object* source, vector<Value>& dest) {
	for (const auto& x : this->iterateObject(source)) {
		if (pool->vm->throwing)
			return;
		dest.push_back(x);
	}
}

inline size_t getIterableSize(Object* object) {
	switch (object->type) {
	case OBJECT_LIST:
		return ((ListObject*)object)->size();
	case OBJECT_TUPLE:
		return ((TupleObject*)object)->size;
	case OBJECT_SET:
		return ((SetObject*)object)->size();
	case OBJECT_STRING:
		return ((StringObject*)object)->content.size();
	default:
		return UNSIZED_ITERABLE;
	}
}

inline RolContainerHelper* getContainerHelper(Object* object) {
	switch (object->type) {
	case OBJECT_LIST:
		return new RolVectorHelper(((ListObject*)object)->list);
	case OBJECT_SET:
		return new RolSetHelper(((SetObject*)object)->values);
	case OBJECT_STRING:
		return new RolStringHelper(((StringObject*)object)->content);
	default:
		throw exception("Unsupported container type");
	}
}