#pragma once

#include "ast.h"
#include "operations.h"
#include "nodes.h"

#include <vector>
#include <set>
#include <unordered_map>

using namespace std;

class Value;
class ValuePool;
class Chunk;
class OperationIterator;

class BytecodeOptimizer {
public:
	BytecodeOptimizer(Chunk* chunk, Value* consts);

	~BytecodeOptimizer();

	void optimize();

private:
	const set<OP> Pointers{ OP_GOTO, OP_GOTO_IF_FALSE, OP_GOTO_IF_TRUE, OP_FLAG_CATCH },
				  Pushies{ OP_CONST, OP_TRUE, OP_FALSE, OP_NULL, OP_COPY, OP_GET_UPVALUE,
						OP_CLOSURE },
				  BinaryOperations{ 
						OP_LESS, OP_GREATER, OP_LESS_EQUAL, OP_GREATER_EQUAL, OP_EQUAL, OP_NOT_EQUAL,
						OP_ADD, OP_SUB, OP_DIV, OP_MUL, OP_POW,
						OP_IS_PROTOTYPE
					};

	Chunk* chunk;
	OperationIterator* opit;

	void error(const char* msg) {
		printf("BytecodeOptimizerError: %s\n", msg);
		throw exception(msg);
	}

	Value* consts;
};

class ASTOptimizer {
public:

	ASTOptimizer(Node* root, ValuePool* pool) : root(root), pool(pool) {
	}

	~ASTOptimizer() {
	}

	bool start() {
		try {
			root = this->optimize(root);
			this->removeNodes();
		}
		catch (exception) {
			return false;
		}
		return true;
	}

	exception error(const char* msg) {
		printf("OptimizationError: %s\n", msg);
		return exception(msg);
	}

	Node* optimize(Node* node) {
#define SHORTCUT(node_name) return this->visit ## node_name ## Node((node_name*)node);

		switch (node->type) {
		case NODE_LITERAL:
			SHORTCUT(Literal);
		case NODE_NUMERIC_BINARY:
			SHORTCUT(Binary);
		case NODE_NOT:
			SHORTCUT(Not);
		case NODE_NEGATE:
			SHORTCUT(Negate);
		case NODE_VAR:
			SHORTCUT(Var);
		case NODE_COMPARE:
			SHORTCUT(Compare);
		case NODE_ASSIGN:
			SHORTCUT(Assign);
		case NODE_STMTS:
			SHORTCUT(Stmts);
		case NODE_BLOCK:
			SHORTCUT(Block);
		case NODE_VAR_DECL:
			SHORTCUT(VarDecl);
		case NODE_PRINT:
			SHORTCUT(Print);
		case NODE_IF:
			SHORTCUT(If);
		case NODE_WHILE:
			SHORTCUT(While);
		case NODE_FOR:
			SHORTCUT(For);
		case NODE_RETURN:
			SHORTCUT(Return);
		case NODE_BREAK:
			SHORTCUT(Break);
		case NODE_CONTINUE:
			SHORTCUT(Continue);
		case NODE_TERNARY:
			SHORTCUT(Ternary);
		case NODE_OR:
			SHORTCUT(Or);
		case NODE_AND:
			SHORTCUT(And);
		case NODE_INCREASE:
			SHORTCUT(Increase);
		case NODE_DECREASE:
			SHORTCUT(Decrease);
		case NODE_FUNC:
			SHORTCUT(Func);
		case NODE_CALL:
			SHORTCUT(Call);
		case NODE_INVOKE:
			SHORTCUT(Invoke);
		case NODE_MEMBER:
			SHORTCUT(Member);
		case NODE_MEMBER_ASSIGN:
			SHORTCUT(MemberAssign);
		case NODE_OBJECT_GENERATOR:
			SHORTCUT(ObjectGenerator);
		case NODE_LIST_GENERATOR:
			SHORTCUT(ListGenerator);
		case NODE_TUPLE_GENERATOR:
			SHORTCUT(TupleGenerator);
		case NODE_THIS:
			SHORTCUT(This);
		case NODE_TRY_CATCH:
			SHORTCUT(TryCatch);
		case NODE_RAISE:
			SHORTCUT(Raise);
		case NODE_NEW:
			SHORTCUT(New);
		case NODE_INSTANCEOF:
			SHORTCUT(InstanceOf);
		case NODE_CHAIN_EXPRESSION:
			SHORTCUT(ChainExpression);
		case NODE_CONDITION:
			SHORTCUT(Condition);
		default:
			// Unreachable
			throw this->error("Unknown oldie");
		}

#undef SHORTCUT
	}

	template <typename T>
	void optimize(vector<T>& list) {
		for (auto& x : list) {
			x = this->optimize(x);
		}
	}

	Node* getRoot() {
		return root;
	}

protected:
	typedef unordered_map<Node*, Value*> precompute_map;

	const set<NodeType> NotExpr{ NODE_IF, NODE_BLOCK, NODE_STMTS, NODE_PRINT, NODE_VAR_DECL, NODE_WHILE, NODE_FOR, NODE_RETURN, NODE_TRY_CATCH, NODE_BREAK, NODE_CONTINUE, NODE_RAISE };
	const set<NodeType> Subscriptable{ NODE_TUPLE_GENERATOR, NODE_LIST_GENERATOR };

	Node* root;
	ValuePool* pool;
	precompute_map literals;
	vector<Node*> trash_nodes, trash_branches;

	
	Node* getSubscriptable(Node* node) {
		if (Subscriptable.contains(node->type)) {
			return node;
		}

		if (IS_NODE(*node, NODE_EXPRESSION_IN_STATEMENTS)) {
			auto EIS = AS_NODE(node, ExpressionInStatements);
			return getSubscriptable(EIS->nodes[EIS->consider]);
		}

		return NULL;
	}

	void pendingRemoval(const initializer_list<Node*>& nodes) {
		trash_nodes.insert(trash_nodes.end(), nodes.begin(), nodes.end());
	}
	void removeBranch(Node* node) {
		trash_branches.push_back(node);
	}
	void removeNodes() {
		for (const auto& x : trash_nodes) {
			delete x;
		}
		trash_nodes.clear();

		if (!trash_branches.empty()) {
			AST cleaner;
			for (const auto& x : trash_branches) {
				cleaner.startScan(x);
				cleaner.freeNodes();
			}
		}
	}


	Value* extractValue(Node* literal_alike) {

_extracter_began:
		switch (literal_alike->type) {
		case NODE_LITERAL:
			return AS_NODE(literal_alike, Literal)->v;
		case NODE_EXPRESSION_IN_STATEMENTS: {
			auto EIS = AS_NODE(literal_alike, ExpressionInStatements);
			literal_alike = EIS->nodes[EIS->consider];
			goto _extracter_began;
		}
		default:
			return NULL;
		}
	}
	void markLiteral(Node* replacement) {
		literals[replacement] = this->extractValue(replacement);
	}
	void markLiteral(Node* replacement, Node* oldie) {
		this->pendingRemoval({ oldie });
		literals[replacement] = this->extractValue(replacement);
	}

	bool isLiteral(precompute_map::key_type node) const noexcept {
		return literals.contains(node);
	}
	precompute_map::mapped_type getLiteral(precompute_map::key_type node) noexcept {
		auto v(literals[node]);
		literals.erase(node);
		return v;
	}

	void flatten(vector<Node*>& nodes) {
		for (auto& x : nodes) {
			x = this->flatten(x);
		}
	}

	Node* flatten(Node* node);

	void flatten(Node* node, vector<Node*>& nodes) {
		switch (node->type) {
		case NODE_TUPLE_GENERATOR: {
			const auto list_gen = (TupleGenerator*)node;

			for (auto& x : list_gen->elems) {
				x = this->optimize(x);
				this->flatten(x, nodes);
			}
			break;
		}
		case NODE_LIST_GENERATOR: {
			const auto list_gen = (ListGenerator*)node;

			for (auto& x : list_gen->elems) {
				x = this->optimize(x);
				this->flatten(x, nodes);
			}
			break;
		}
		case NODE_OBJECT_GENERATOR: {
			const auto obj_gen = (ObjectGenerator*)node;

			for (auto& x : obj_gen->props) {
				x = { this->optimize(x.first), this->optimize(x.second) };

				this->flatten(x.first, nodes);
				this->flatten(x.second, nodes);
			}
			if (obj_gen->prototypeExp)
				this->flatten(obj_gen->prototypeExp, nodes);
			break;
		}
		case NODE_COMPARE: {
			auto cmp = (Compare*)node;

			if (this->isLiteral(cmp)) {
				nodes.push_back(cmp);
				return;
			}
			
			cmp->a = this->optimize(cmp->a);
			cmp->b = this->optimize(cmp->b);

			this->flatten(cmp->a, nodes);
			this->flatten(cmp->b, nodes);
			break;
		}
		case NODE_NUMERIC_BINARY: {
			const auto bin = (Binary*)node;

			if (this->isLiteral(bin)) {
				nodes.push_back(bin);
				return;
			}

			bin->a = this->optimize(bin->a);
			bin->b = this->optimize(bin->b);

			this->flatten(bin->a, nodes);
			this->flatten(bin->b, nodes);
			break;
		}
		case NODE_INSTANCEOF: {
			const auto ins = (InstanceOf*)node;

			ins->a = this->optimize(ins->a);
			ins->b = this->optimize(ins->b);

			this->flatten(ins->a, nodes);
			this->flatten(ins->b, nodes);
			break;
		}
		default:
			nodes.push_back(node);
			return;
		}

		delete node;
	}

	Value fromToken(const Token& token);


#define SHORTCUT(node_name) Node* visit ## node_name ## Node(node_name* node);

	SHORTCUT(Func);
	SHORTCUT(Return);

	SHORTCUT(For);
	SHORTCUT(While);

	SHORTCUT(TryCatch);
	SHORTCUT(If);

	SHORTCUT(Condition);

	SHORTCUT(Stmts);
	SHORTCUT(Block);

	SHORTCUT(Print);
	SHORTCUT(VarDecl);

	SHORTCUT(Break);
	SHORTCUT(Continue);
	SHORTCUT(Raise);


	SHORTCUT(ChainExpression);

	SHORTCUT(InstanceOf);

	SHORTCUT(Assign);
	SHORTCUT(Ternary);
	SHORTCUT(Or);
	SHORTCUT(And);
	SHORTCUT(Compare);
	SHORTCUT(Binary);

	SHORTCUT(Not);
	SHORTCUT(Negate);
	SHORTCUT(Increase);
	SHORTCUT(Decrease);

	SHORTCUT(Call);
	SHORTCUT(Invoke);
	SHORTCUT(New);

	SHORTCUT(Member);
	SHORTCUT(MemberAssign);

	SHORTCUT(ObjectGenerator);
	SHORTCUT(ListGenerator);
	SHORTCUT(TupleGenerator);

	SHORTCUT(This);
	SHORTCUT(Var);
	SHORTCUT(Literal);
};