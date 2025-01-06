
#include "chunk.h"
#include "common.h"
#include "optimizer.h"
#include "object.h"
#include "value.h"

#include <map>
#include <unordered_map>
#include <vcruntime_string.h>

using namespace std;

BytecodeOptimizer::BytecodeOptimizer(Chunk* chunk, Value* consts) : chunk(chunk), consts(consts) {
	opit = new OperationIterator(chunk);
}

BytecodeOptimizer::~BytecodeOptimizer() {
	delete opit;
}

void BytecodeOptimizer::optimize() {

#define THIS_OP (this->ops[i])
#define NEXT_OP (this->ops[i + 1])
#define IS_NEXT(v) (i + 1 < size && NEXT_OP.type == (v))
#define NEXT_OP_POS (origin_i + 1 + THIS_OP.arg_count)
#define OP_SIZE(x) ((x).size())
#define GET_USHORT(p) (*(p) * 256 + *((p) + 1))


	// Remove dummy binary operations
	for (auto it = opit->begin(), eit = opit->end(); it != eit; ++it) {
		if (it.belongsTo(BinaryOperations) && it.isNext(OP_POP) && !it.isTargeted()) {
			opit->replace(it, Operation(OP_POP, NULL, 0));
		}
	}

	vector<pair<size_t, size_t>> pushers;

	// Remove dummy push & pop
	for (auto it = opit->begin(), eit = opit->end(); it != eit; ++it) {
		if (it.belongsTo(Pushies)) {
			pushers.emplace_back(it.pos, it.pos);
			continue;
		}
		
		switch (it->type) {
		/*case OP_GEN_OBJ: {
			chunk_size length = GET_USHORT(it->arg) * 2 + 1;

			if (pushers.size() >= length) {
				while (--length)
					pushers.pop_back();
				size_t lend = pushers.back().first;

				pushers.back() = { lend, it.pos };
				break;
			}

			pushers.clear();
			break;
		}
		case OP_GEN_TUPLE:
		case OP_GEN_LIST: {
			chunk_size length = GET_USHORT(it->arg);

			if (!length) {
				pushers.emplace_back(it.pos, it.pos);
				break;
			}

			if (pushers.size() >= length) {
				while (--length)
					pushers.pop_back();
				size_t lend = pushers.back().first;

				pushers.back() = { lend, it.pos };
				break;
			}

			pushers.clear();
			break;
		}*/
		case OP_POP:
			if (!pushers.empty()) {
				size_t pop_streak = 0, non_targeted_pops = 0;

				while (it != eit && it->type == OP_POP && pop_streak < pushers.size()) {
					++pop_streak;

					if (!it.isTargeted()) {
						++non_targeted_pops;
					}

					it.next();
				}
				it.back();

				size_t j = 0;

				for (auto pop_ptr = it; j < non_targeted_pops; ++j, --pop_ptr) {
					const auto& range = *(pushers.rbegin() + j);

					this->opit->remove(pop_ptr);
					this->opit->remove(range.first, range.second);
				}

				j = 0;
				for (size_t k = it.pos - pop_streak - non_targeted_pops; j < pop_streak - non_targeted_pops; ++j) {
					this->opit->replace(k - j, Operation(OP_NULL, NULL, 0));
				}

				pushers.clear();
			}
			break;

		default:
			pushers.clear();
			break;
		}
	}

	opit->finalizeEdits(true);
}


// ---------------
//  AST Optimizer
// ---------------

Value ASTOptimizer::fromToken(const Token& token) {
	switch (token.type) {
	case TOKEN_NULL:
		return Value();
	case TOKEN_TRUE:
		return Value(true);
	case TOKEN_FALSE:
		return Value(false);
	case TOKEN_NUMBER:
		return strtod(token.start, NULL);
	case TOKEN_STRING:
		return pool->internNativeString(__tokenToString((Token*)&token));
	case TOKEN_IDENTIFIER:
		return pool->internNativeString(string(token.start, token.length));

	default:
		// Unreachable
		throw exception("Cannot convert Token to Value");
	}
}

Node* ASTOptimizer::flatten(Node* node) {
	vector<Node*> nodes;
	flatten(node, nodes);

	switch (nodes.size()) {
	case 0:
		return new Literal(new Value());
	case 1:
		return nodes.front();
	default:
		return new ChainExpression(nodes);
	}
}

#define FUNC_SHORTCUT(node_name) Node* ASTOptimizer::visit ## node_name ## Node(node_name* node)
#define OPTIMIZE(node) (node) = (decltype(node))this->optimize(node);
#define FLATTEN(node) (node) = (decltype(node))this->flatten(node);

FUNC_SHORTCUT(Literal) {
	if (!node->v)
		node->v = new Value(this->fromToken(node->token));
	this->markLiteral(node);
	return node;
}

FUNC_SHORTCUT(Return) {
	OPTIMIZE(node->retexp);
	return node;
}

FUNC_SHORTCUT(For) {
	if (node->init) OPTIMIZE(node->init);
	if (node->incr) OPTIMIZE(node->incr);
	if (node->cond) OPTIMIZE(node->cond);
	OPTIMIZE(node->body);

	if (node->cond && this->isLiteral(node->cond)) {
		auto cond = this->getLiteral(node->cond);

		Value prep((bool)*cond);

		if (prep) {
			delete node->cond;
			auto replace = new Literal(new Value(prep));
			this->markLiteral(replace);
		}
		else {
			if (node->init) {
				auto init = node->init;
				delete node;
				return init;
			}
			else {
				auto replace = new Literal(new Value());
				this->markLiteral(replace, node);
				return replace;
			}
		}
	}
	return node;
}

FUNC_SHORTCUT(While) {
	if (node->exp) OPTIMIZE(node->exp);
	OPTIMIZE(node->body);

	if (node->exp && this->isLiteral(node->exp)) {
		auto exp = this->getLiteral(node->exp);

		this->pendingRemoval({ node->exp, node });

		bool prep = *exp;

		if (prep) {
			auto replace = new Literal(new Value(prep));
			this->markLiteral(replace);
			return replace;
		}
		else {
			auto replace = new Literal(new Value());
			this->markLiteral(replace);
			return replace;
		}
	}
	return node;
}

FUNC_SHORTCUT(TryCatch) {
	OPTIMIZE(node->tries);
	OPTIMIZE(node->excepts);

	return node;
}

FUNC_SHORTCUT(If) {
	OPTIMIZE(node->main);

	if (node->else_)
		OPTIMIZE(node->else_);

	vector<Node*> checkers;

	switch (node->main->type) {
	case NODE_BLOCK:
		for (const auto& x : node->elifs) {
			this->removeBranch(x);
		}
		if (node->else_)
			this->removeBranch(node->else_);
		return node->main;
	case NODE_IF:
		checkers.push_back(node->main);
		break;
	case NODE_LITERAL:
		break;
	}

	bool flag = false;
	for (auto& x : node->elifs) {
		Node* opted = this->optimize(x);

		switch (opted->type) {
		case NODE_BLOCK:
			flag = true;
		case NODE_IF:
			checkers.push_back(opted);
		case NODE_LITERAL:
			break;
		}

		if (flag)
			break;
	}

	if (node->else_ && !checkers.empty() && IS_NODE(*checkers.back(), NODE_BLOCK))
		checkers.push_back(node->else_);

	if (checkers.empty()) {
		return node->else_ ? (Node*)node->else_ : (Node*)new Literal(new Value());
	}
	else {
		const auto& new_main = checkers.front();

		if (IS_NODE(*new_main, NODE_BLOCK)) {
			for (size_t i = 1; i < checkers.size(); ++i) {
				this->removeBranch(checkers[i]);
			}
			if (node->else_)
				this->removeBranch(node->else_);
			return new_main;
		}
		else {
			node->main = (Condition*)new_main;

			if (IS_NODE(*checkers.back(), NODE_BLOCK)) {
				if ((Node*)node->else_ != checkers.back()) {
					this->removeBranch(node->else_);
				}
				node->else_ = (Block*)checkers.back();
				checkers.pop_back();
			}

			node->elifs.clear();
			for (size_t i = 1; i < checkers.size(); ++i) {
				node->elifs.push_back((Condition*)checkers[i]);
			}

			return node;
		}
	}
}

FUNC_SHORTCUT(Condition) {
	OPTIMIZE(node->exp);

	if (this->isLiteral(node->exp)) {
		auto exp = this->getLiteral(node->exp);

		Value cond((bool)*exp);

		this->pendingRemoval({ node, node->exp });
		if (!cond)
			this->pendingRemoval({ node->body });

		return cond ? (Node*)node->body : (Node*)new Literal(new Value());
	}

	return node;
}

// Normal Scope

FUNC_SHORTCUT(Stmts) {
	this->optimize(node->stmts);
	this->flatten(node->stmts);
	return node;
}

FUNC_SHORTCUT(Block) {
	this->optimize(node->stmts);
	this->flatten(node->stmts);
	return node;
}

// Functional nodes

FUNC_SHORTCUT(Print) {
	OPTIMIZE(node->node);
	return node;
}
FUNC_SHORTCUT(VarDecl) {
	for (auto& x : node->declarations) {
		if (x.init)
			OPTIMIZE(x.init);
	}
	return node;
}

// Control flow

FUNC_SHORTCUT(Break) {
	return node;
}
FUNC_SHORTCUT(Continue) {
	return node;
}
FUNC_SHORTCUT(Raise) {
	OPTIMIZE(node->raiseexp);
	return node;
}

// Special expressions

FUNC_SHORTCUT(Func) {
	OPTIMIZE(node->stmts);
	return node;
}

FUNC_SHORTCUT(ChainExpression) {
	this->optimize(node->nodes);
	this->flatten(node->nodes);
	return node;
}

FUNC_SHORTCUT(InstanceOf) {
	OPTIMIZE(node->a);
	OPTIMIZE(node->b);
	return node;
}

FUNC_SHORTCUT(Assign) {
	OPTIMIZE(node->value);
	return node;
}

FUNC_SHORTCUT(Ternary) {

	// Unwrap Not operators
	while (node->cond->type == NODE_NOT) {
		Node* temp = node->texp;
		node->texp = node->fexp;
		node->fexp = temp;

		Node* inner = ((Not*)node->cond)->node;
		delete node->cond;
		node->cond = inner;
	}

	OPTIMIZE(node->cond);
	OPTIMIZE(node->texp);
	OPTIMIZE(node->fexp);

	uint8_t state = 2;
	
	if (this->isLiteral(node->cond)) {
		auto cond = this->getLiteral(node->cond);

		state = (bool)*cond;
		this->pendingRemoval({ node, state ? node->fexp : node->texp });
	}

	if (state != 2) {
		return state ? node->texp : node->fexp;
	}

	return node;
}

// Expressions

FUNC_SHORTCUT(Or) {
	OPTIMIZE(node->a);
	OPTIMIZE(node->b);

	if (this->isLiteral(node->a)) {
		auto a = this->getLiteral(node->a);

		this->pendingRemoval({ node, node->a });
		if (!*a)
			this->pendingRemoval({ node->b });

		return *a ? new Literal(new Value(*a)) : node->b;
	}

	return node;
}

FUNC_SHORTCUT(And) {
	OPTIMIZE(node->a);
	OPTIMIZE(node->b);

	if (this->isLiteral(node->a)) {
		auto a = this->getLiteral(node->a);

		this->pendingRemoval({ node });
		if (*a)
			this->pendingRemoval({ node->b });

		return *a ? node->b : new Literal(new Value(*a));
	}

	return node;
}

FUNC_SHORTCUT(Compare) {
	OPTIMIZE(node->a);
	OPTIMIZE(node->b);

	if (!this->isLiteral(node->a) || !this->isLiteral(node->b))
		return node;

	auto a = this->getLiteral(node->a), b = this->getLiteral(node->b);

	bool status = false;
	Value prep;

	switch (node->compare_type) {
	case CMP_LESS:
		prep = Value_CMP_LESS(*a, *b, status);
		break;
	case CMP_GREATER:
		prep = Value_CMP_GREATER(*a, *b, status);
		break;
	case CMP_LESS_EQUAL:
		prep = Value_CMP_LESS_EQ(*a, *b, status);
		break;
	case CMP_GREATER_EQUAL:
		prep = Value_CMP_GREATER_EQ(*a, *b, status);
		break;
	case CMP_EQUAL:
		prep = *a == *b;
		break;
	case CMP_NOT_EQUAL:
		prep = *a != *b;
		break;
	}

	if (status) {
		auto replace = new Literal(new Value(prep));
		this->markLiteral(replace, node);
		return replace;
	}
	else {
		throw this->error("Invalid comparing operand's types");
	}
	
	return node;
}

FUNC_SHORTCUT(Binary) {
	OPTIMIZE(node->a);
	OPTIMIZE(node->b);

	if (!this->isLiteral(node->a) || !this->isLiteral(node->b))
		return node;

	auto a = this->getLiteral(node->a), b = this->getLiteral(node->b);

	bool status = false;
	Value prep;

	switch (node->optype) {
	case ASSIGN_ADD:
		prep = Value_ADD(*a, *b, pool, status);
		break;
	case ASSIGN_SUB:
		prep = Value_SUB(*a, *b, status);
		break;
	case ASSIGN_MUL:
		prep = Value_MUL(*a, *b, status);
		break;
	case ASSIGN_DIV:
		prep = Value_DIV(*a, *b, status);
		break;
	case ASSIGN_POW:
		prep = Value_POW(*a, *b, status);
		break;
	}

	if (status) {
		auto replace = new Literal(new Value(prep));
		this->markLiteral(replace, node);
		return replace;
	}
	else {
		throw this->error("Invalid mathmetic binary operand's types");
	}

	return node;
}

FUNC_SHORTCUT(Not) {
	OPTIMIZE(node->node);

	if (this->isLiteral(node->node)) {
		auto prep(this->getLiteral(node->node));

		auto replace = new Literal(new Value(!*prep));
		this->markLiteral(replace, node);

		return replace;
	}

	return node;
}

FUNC_SHORTCUT(Negate) {
	OPTIMIZE(node->node);

	if (this->isLiteral(node->node)) {
		auto a(this->getLiteral(node->node));

		bool status = false;
		Value prep = Value_UNARY_NEGATE(*a, status);

		if (status) {
			auto replace = new Literal(new Value(prep));
			this->markLiteral(replace, node);

			return replace;
		}
		else {
			throw this->error("Invalid negation operand");
		}
	}

	return node;
}

FUNC_SHORTCUT(Increase) {
	return node;
}

FUNC_SHORTCUT(Decrease) {
	return node;
}

// Invoking

FUNC_SHORTCUT(Call) {
	this->optimize(node->args);
	OPTIMIZE(node->callee);

	return node;
}

FUNC_SHORTCUT(Invoke) {
	this->optimize(node->args);
	OPTIMIZE(node->callee);

	return node;
}

FUNC_SHORTCUT(New) {
	OPTIMIZE(node->activation);
	return node;
}

// Operation on objects

FUNC_SHORTCUT(Member) {
	OPTIMIZE(node->obj);
	OPTIMIZE(node->prop);

	if (this->isLiteral(node->prop)) {
		Node* subscr_able = this->getSubscriptable(node->obj);

		if (subscr_able) {
			const auto& prop = *this->getLiteral(node->prop);

			if (IS_NUMBER(prop)) {
				this->pendingRemoval({ node, node->prop, node->obj });

				const size_t index = AS_NUMBER(prop);

				size_t i = UINT64_MAX;
				vector<Node*> vec;
				size_t elem_count = 0;
				Node** sequence = NULL;

				switch (subscr_able->type) {
				case NODE_TUPLE_GENERATOR: {
					auto tup = AS_NODE(subscr_able, TupleGenerator);
					elem_count = tup->elems.size();
					sequence = &tup->elems.front();
					break;
				}
				case NODE_LIST_GENERATOR: {
					auto list = AS_NODE(subscr_able, ListGenerator);
					elem_count = list->elems.size();
					sequence = &list->elems.front();
					break;
				}
				}

				if (0ull > index || index >= elem_count) {
					// If index is out of range, automatically return null
					flatten(subscr_able, vec);
					vec.push_back(new Literal(new Value()));
					return new ChainExpression(vec);
				}
				else {
					// Else, only consider the value at the return index
					size_t i = UINT64_MAX;
					for (size_t j = 0; j < elem_count; ++j) {
						if (j == index) {
							vec.push_back(sequence[j]);
							i = vec.size() - 1;
						}
						else {
							flatten(sequence[j], vec);
						}
					}

					auto replace = new ExpressionInStatements(vec, i);
					this->markLiteral(replace);

					return replace;
				}
			}
		}
	}
	return node;
}

FUNC_SHORTCUT(MemberAssign) {
	OPTIMIZE(node->obj);
	OPTIMIZE(node->prop);
	OPTIMIZE(node->val);
	return node;
}

FUNC_SHORTCUT(ObjectGenerator) {
	for (auto& x : node->props) {
		OPTIMIZE(x.first);
		OPTIMIZE(x.second);
	}
	if (node->prototypeExp)
		OPTIMIZE(node->prototypeExp);
	return node;
}

FUNC_SHORTCUT(ListGenerator) {
	this->optimize(node->elems);

	bool isLiteral = true;

	for (auto& x : node->elems) {
		if (!this->isLiteral(x)) {
			isLiteral = false;
		}
		else {
			this->getLiteral(x);
		}
	}

	if (node->isLiteral = isLiteral) {
		this->markLiteral(node);
	}
	return node;
}

FUNC_SHORTCUT(TupleGenerator) {
	this->optimize(node->elems);

	bool isLiteral = true;

	for (auto& x : node->elems) {
		if (!this->isLiteral(x)) {
			isLiteral = false;
		}
		else {
			this->getLiteral(x);
		}
	}

	if (node->isLiteral = isLiteral) {
		this->markLiteral(node);
	}
	return node;
}

FUNC_SHORTCUT(This) {
	return node;
}

FUNC_SHORTCUT(Var) {
	return node;
}