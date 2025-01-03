#pragma once

#include <vector>
#include <cstdarg>
#include <string>
#include <set>
#include <map>

#include "operations.h"
#include "optimizer.h"
#include "value.h"

using namespace std;

class Token;
class TupleObject;

typedef ushort chunk_size;

class Chunk {
public:
	Chunk() {}
	Chunk(ValuePool* pool) : pool(pool) {}

	constexpr vector<uint8_t>& getOPs() {
		return this->ops;
	}

	constexpr vector<Value>& getConsts() {
		return this->consts;
	}

	constexpr ValuePool* getPool() {
		return this->pool;
	}

	const inline void popOPs(int n) {
		while (n-- > 0) {
			this->ops.pop_back();
		}
	}

	const inline void addOPs(uint8_t op) {
		this->ops.push_back(op);
	}

	template <typename... Args>
	const inline void addOPs(uint8_t op, Args... ops) {
		this->addOPs(op);
		this->addOPs(ops...);
	}

	const size_t addGoto(uint8_t op) {
		this->addOPs(op, 0, 0);
		return this->length() - 2;
	}

	const size_t argedOP(uint8_t op, unsigned short target) {
		this->addOPs(op, target / 256, target % 256);
		return this->length() - 2;
	}

	const inline void gotoTarget(size_t goto_pos) {
		this->writeUshort(goto_pos, this->length());
	}

	const inline void gotoTarget(size_t goto_pos, unsigned short target) {
		this->writeUshort(goto_pos, target);
	}

	const inline void writeUshort(size_t i, unsigned short value) {
		this->ops[i] = value / 256;
		this->ops[i + 1] = value % 256;
	}

	const inline void writeUchar(size_t i, unsigned char value) {
		this->ops[i] = value;
	}

	constexpr size_t length() const noexcept {
		return this->ops.size();
	}

	constexpr size_t constCount() const noexcept {
		return this->consts.size();
	}

	size_t addTokenConst(const Token& token);

	size_t addConst(const string& str);
	size_t addConst(const double& number);

	template <typename T>
	size_t addObjectConst(T* obj) {
		this->pool->registerObject(obj);
		this->consts.emplace_back((Object*)obj);
		return this->consts.size() - 1;
	}

	// Add an tuple constant that has been internalized (won't register the object)
	size_t addTupleConst(TupleObject* tup) {
		if (this->tuple_consts.contains(tup)) {
			return this->tuple_consts[tup];
		}

		this->consts.emplace_back((Object*)tup);
		return this->consts.size() - 1;
	}

	size_t addRegisteredObjectConst(Object* obj) {
		this->consts.emplace_back(obj);
		return this->consts.size() - 1;
	}

	void print();

	Chunk* copy();

private:
	vector<uint8_t> ops;
	vector<Value> consts;

	ValuePool* pool = NULL;

	// This map is used to avoid unnecessary doubling, not to store constants
	map<StringObject*, int> string_consts;
	map<Value*, int> number_consts;
	map<TupleObject*, int> tuple_consts;
};


struct Code {
	Code(Chunk& chunk) : chunk(chunk) {}

	void optimizeBytecode() {
		BytecodeOptimizer bc(&this->chunk, this->chunk.getConsts().data());

		bc.optimize();
	}

	Chunk& chunk;
};

static string __tokenToString(Token* token) {
	const char* raw = token->start;

	string output;
	size_t index = 0, length = token->length;

	while (index < length) {
		switch (raw[index]) {
		case '\\':
			switch (raw[++index]) {
			case 'n':
				output.push_back('\n');
				break;
			case 't':
				output.push_back('\t');
				break;
			case 'b':
				output.push_back('\b');
				break;
			case 'r':
				output.push_back('\r');
				break;
			case 'v':
				output.push_back('\v');
				break;
			case 'a':
				output.push_back('\a');
				break;
			case 'f':
				output.push_back('\f');
				break;
			default:
				output.push_back(raw[index]);
				break;
			}
			++index;
			continue;
		case '\n':
			++index;
			break;
		}
		output.push_back(raw[index++]);
	}

	return output;
}

class OperationIterator {
public:
	vector<Operation> ops;
	vector<size_t> op_sizes{ 0 };
	size_t last_op_index = 0;
	bool* removed = NULL;

	OperationIterator(Chunk* chunk) : chunk(*chunk) {
		this->transform();
	}

	~OperationIterator() {
		if (removed) {
			delete[] removed;
		}
	}

	class iterator {
	public:
		OperationIterator* opit;
		size_t pos;

		iterator(OperationIterator* op, size_t i) : opit(op), pos(i) {
			if (pos >= opit->size())
				return;

			while (opit->removed[pos] && ++pos != opit->size()) {}
		}

		constexpr bool isLast() const noexcept {
			return pos >= opit->size();
		}

		constexpr bool isFirst() const noexcept {
			return !pos;
		}

		bool isTargeted() const noexcept {
			return opit->targets.contains(pos);
		}

		constexpr bool isNext(const OP& type) const noexcept {
			return !isLast() && (*this + 1)->type == type;
		}

		template<typename T>
		constexpr bool belongsTo(const set<T>& batch) const noexcept {
			return batch.contains((**this).type);
		}

		constexpr Operation& operator*() const noexcept {
			return opit->ops[pos];
		}

		constexpr Operation* operator->() const noexcept {
			return &opit->ops[pos];
		}

		constexpr bool operator==(const OperationIterator::iterator& it) const noexcept {
			return opit == it.opit && (pos == it.pos || (pos >= opit->size() && it.pos >= it.opit->size()));
		}

		bool operator!=(const OperationIterator::iterator& it) {
			return !(*this == it);
		}

		iterator& operator++() {
			this->next();
			return *this;
		}
		iterator operator++(int) {
			return ++iterator(*this);
		}

		iterator& operator--() {
			this->back();
			return *this;
		}
		iterator operator--(int) {
			return --iterator(*this);
		}

		iterator& operator+=(long long x) {
			this->jump(abs(x), x > 0);
			return *this;
		}
		iterator operator+(long long x) const noexcept {
			return iterator(*this) += x;
		}

		iterator& operator-=(long long x) {
			this->jump(abs(x), x < 0);
			return *this;
		}
		iterator operator-(long long x) const noexcept {
			return iterator(*this) -= x;
		}

		void next() {
			if (pos >= opit->size() || ++pos == opit->size())
				return;

			while (opit->removed[pos] && ++pos != opit->size()) {}
		}

		void back() {
			if (pos <= 0)
				return;

			if (--pos == 0 && opit->removed[pos]) {
				pos = UINT64_MAX;
				return;
			}

			while (opit->removed[pos]) {
				if (--pos == 0 && opit->removed[pos]) {
					pos = UINT64_MAX;
					return;
				}
			}
		}

		void jump(size_t i, bool forward) {
			if (forward) {
				while (i--) {
					this->next();
				}
			}
			else {
				while (i--) {
					this->back();
				}
			}
		}

		constexpr size_t origin_pos() const noexcept {
			return opit->op_sizes[pos];
		}
	};

	void remove(const Operation& op) {
		removed[&op - &ops[0]] = true;

		if (Pointers.contains(op.type)) {
			this->deflagPointer(op);
		}
	}
	void remove(const size_t pos) {
		removed[pos] = true;

		if (Pointers.contains(ops[pos].type)) {
			this->deflagPointer(ops[pos]);
		}
	}
	void remove(const iterator& it) {
		return remove(it.pos);
	}
	void remove(const Operation* start, const Operation* end) {
		const Operation* real_end = min((Operation*)end, &this->ops.back());

		for (auto* i = (Operation*)start, * astart = &ops[0]; i <= real_end; ++i) {
			removed[i - astart] = true;
		}
	}
	void remove(const size_t start, const size_t end) {
		const size_t real_end = min(end, this->size());

		for (size_t i = start; i <= end; ++i) {
			removed[i] = true;
		}
	}
	void remove(const iterator& start, const iterator& end) {
		remove(start.pos, end.pos);
	}

	void restore(const Operation& op) {
		removed[&op - &ops[0]] = false;
	}
	void restore(const size_t pos) {
		removed[pos] = false;
	}
	void restore(const iterator& it) {
		restore(it.pos);
	}

	void replace(const size_t pos, const Operation& op) {
		ops[pos] = op;

		if (Pointers.contains(ops[pos].type)) {
			this->deflagPointer(ops[pos]);
		}

		if (Pointers.contains(op.type)) {
			this->flagPointer(op, pos);
		}
	}
	void replace(const Operation& op, const Operation& newop) {
		const size_t pos = &op - &ops[0];

		ops[pos] = newop;

		if (Pointers.contains(op.type)) {
			this->deflagPointer(op);
		}

		if (Pointers.contains(op.type)) {
			this->flagPointer(op, pos);
		}
	}
	void replace(const iterator& it, const Operation& newop) {
		replace(it.pos, newop);
	}

	constexpr size_t opPos(const Operation& op) const noexcept {
		return op_sizes[&op - &ops[0]];
	}

	constexpr bool isLast(const Operation& op) const noexcept {
		return &op == &ops.back();
	}

	bool isTargeted(const Operation& op) const noexcept {
		return targets.contains(this->opPos(op));
	}
	
	inline iterator begin() const noexcept {
		return iterator((OperationIterator*)this, 0);
	}

	inline iterator end() const noexcept {
		return iterator((OperationIterator*)this, last_op_index);
	}

	constexpr size_t size() const noexcept {
		return this->ops.size();
	}

	void finalizeEdits(bool isfinal) {
		this->updatePointers();

		vector<uint8_t> rebaked;

		size_t origin_i = 0;
		for (auto& op : *this) {
			rebaked.push_back(op.type);

			uint8_t* ptr = op.arg, * end = op.arg + op.arg_count;
			while (ptr != end) {
				rebaked.push_back(*ptr++);
			}
		}

		chunk.getOPs() = rebaked;

		if (!isfinal) {
			this->reset();
			this->transform();
		}
	}

protected:
	Chunk& chunk;

	void deflagPointer(const Operation& op) {
		auto& vec = targets[(*op.arg) * 256 + *(op.arg + 1)];

		const size_t i = &op - &ops[0];
		for (auto x = vec.begin(); x != vec.end(); ++x) {
			if (*x == i) {
				vec.erase(x);
				break;
			}
		}
	}

	void flagPointer(const Operation& op, const size_t i) {
		chunk_size pos = (*op.arg) * 256 + *(op.arg + 1);

		if (targets.contains(pos)) {
			targets[pos].push_back(i);
		}
		else {
			targets[pos] = { i };
		}
	}

	template <typename... Args>
	void markOperation(Args... args) {
		ops.emplace_back(args...);
		op_sizes.push_back(op_sizes.back() + ops.back().size());

		if (Pointers.contains(ops.back().type)) {
			flagPointer(ops.back(), ops.size() - 1);
		}
	}

	void finalizeMarkings() {
		ops.shrink_to_fit();
		op_sizes.shrink_to_fit();

		last_op_index = op_sizes.back();

		removed = new bool[ops.size()];
		memset(removed, false, ops.size());
	}

	void scanPointers() {
		size_t i = 0;

		for (auto& op : this->ops) {
			if (Pointers.contains(op.type)) {
				this->flagPointer(op, i);
			}
			++i;
		}
	}

	void updatePointers() {
		decltype(OperationIterator::op_sizes) temp_new_op_sizes{ 0 };

		bool* i_removed = removed;
		for (auto& op : this->ops) {
			if (*i_removed) {
				temp_new_op_sizes.push_back(temp_new_op_sizes.back());
			}
			else {
				temp_new_op_sizes.push_back(temp_new_op_sizes.back() + op.size());
			}
			++i_removed;
		}

		size_t i = 0;

		for (auto& it : targets) {
			int target_pos = it.first;

			while (target_pos != op_sizes[i])
				++i;

			int new_target_pos = temp_new_op_sizes[i];

			if (new_target_pos != target_pos) {
				for (auto& op_pos : it.second) {
					uint8_t* arg = this->ops[op_pos].arg;
					arg[0] = new_target_pos / 256;
					arg[1] = new_target_pos % 256;
				}
			}
		}
	}

private:
	const set<OP> Pointers{ OP_GOTO, OP_GOTO_IF_FALSE, OP_GOTO_IF_TRUE, OP_FLAG_CATCH };
	map<ushort, vector<size_t>> targets;

	void transform();

	void reset() {
		ops.clear();

		op_sizes.clear();
		op_sizes.push_back(0);

		targets.clear();

		last_op_index = 0;
		if (removed) {
			delete[] removed;
		}

		removed = NULL;
	}
};