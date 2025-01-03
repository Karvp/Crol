#pragma once

#include "nodes.h"
#include "chunk.h"
#include <map>
#include <vector>
#include <set>

using namespace std;

enum NameType {
	NAME_LOCAL,
	NAME_UPVALUE
};

enum FlowType {
	FLOW_BREAK,
	FLOW_CONTINUE
};

struct ControlFlow {
	FlowType type;
	size_t pos;
};

class Compiler;

struct ResolveResult {
	Compiler* compiler;
	NameType type;
	int position;
};

struct Upvalue {
	int pos, capture_pos;
	ResolveResult res;
	// Used for capture process
	bool isLocal;
};

struct Local {
	int pos; // Position on the stack
	int scope_pos; // Relative pos in innermost scope
	bool captured;
};

class Compiler {
public:
	typedef vector<map<string, Local>> mapping_type;
	typedef vector<string> name_array;


	Compiler(Node* root, ValuePool* pool) : root(root), parent(NULL), chunk(new Chunk(pool)), name("Main") {
		pool->currentCompiler = this;
		pool->addChunk(this->chunk);
	}

	Compiler(Node* root, ValuePool* pool, Compiler* parent, string name) : root(root), parent(parent), chunk(new Chunk(pool)), name(name) {
		pool->currentCompiler = this;
		pool->addChunk(this->chunk);
	}

	~Compiler() {
		chunk->getPool()->currentCompiler = this->parent;
	}

	bool start(const name_array& reg_names, mapping_type::value_type* globals_ref) {

		try {
			// Creates the first scope
			this->newScope();

			auto& uppermost_scope = this->bindings.back();

			// Occupy first slot
			uppermost_scope[this->firstSlotName] = { this->local_count, this->local_count++, false };

			for (auto& x : reg_names) {
				uppermost_scope[x] = { this->local_count, this->local_count++, false };
			}

			this->compile(this->root);

			if (globals_ref) {
				auto& globals = *globals_ref;
				globals.clear();
				globals.insert(uppermost_scope.begin(), uppermost_scope.end());
				globals.erase(this->firstSlotName);
			}

			this->exitScope();
			this->chunk->addOPs(OP_NULL, OP_RETURN);

			return true;
		}
		catch (exception) {
			return false;
		}
	}

	exception error(const char* msg) {
		printf("CompileError: %s\n", msg);
		return exception(msg);
	}

	void compile(Node* node) {

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
		case NODE_EXPRESSION_IN_STATEMENTS:
			SHORTCUT(ExpressionInStatements);
		default:
			// Unreachable
			throw this->error("Unknown node type");
		}

#undef SHORTCUT
	}

	template <typename T>
	void compile(const vector<T>* list) {
		for (auto& x : *list) {
			this->compile(x);
		}
	}

	Chunk* getChunk() {
		return this->chunk;
	}

	inline const map<string, Upvalue>& getUpvalues() noexcept {
		return this->upvalues;
	}

	inline const mapping_type& getBindings() const noexcept{
		return this->bindings;
	}

	void printSignature() {
		vector<const char *> names;
		Compiler* ptr = this;
		do {
			names.push_back(ptr->name.empty() ? 0 : ptr->name.c_str());
			ptr = ptr->parent;
		} while (ptr);
		
		for (int i = names.size() - 1; i >= 0; --i) {
			if (i != names.size() - 1) {
				printf("<%s function>.", names[i] ? names[i] : "(anonymous)");
			}
			else {
				printf("<%s>.", names[i]);
			}
		}
	}

	string name;
	Compiler* parent;
private:
	const set<NodeType> NotExpr{ NODE_IF, NODE_BLOCK, NODE_STMTS, NODE_PRINT, NODE_VAR_DECL, NODE_WHILE, NODE_FOR, NODE_RETURN, NODE_TRY_CATCH, NODE_BREAK, NODE_CONTINUE, NODE_RAISE };
	const string firstSlotName = "this";

	Node* root;
	Chunk* chunk;

	map<string, Upvalue> upvalues;

	mapping_type bindings;
	int local_count = 0, upvalue_count;

	int current_scope = -1;

	vector<ControlFlow> flags;


	void newScope() {
		++this->current_scope;
		if (this->current_scope + 1 >= this->bindings.size()) {
			this->bindings.emplace_back();
		}
	}

	void exitScope() {
		auto& scope = this->bindings[this->current_scope--];

		bool* captured = new bool[scope.size()];
		for (auto& x : scope) {
			captured[x.second.scope_pos] = x.second.captured;
		}

		for (int i = scope.size() - 1; i >= 0; --i) {
			this->chunk->addOPs(captured[i] ? OP_CLOSE_UPVALUE : OP_POP);
		}
		this->local_count -= scope.size();

		scope.clear();
		delete[] captured;
	}

	void addControlFlow(FlowType type, const size_t pos) {
		this->flags.emplace_back(type, pos);
	}

	void resolveFlags(size_t continue_jump, size_t break_jump) {
		for (auto& flag : this->flags) {
			this->chunk->writeUshort(flag.pos, flag.type == FLOW_BREAK ? break_jump : continue_jump);
		}
		this->flags.clear();
	}


	ResolveResult addLocal(string* name);

	// Resolving names
	ResolveResult resolveName(string* name);
	ResolveResult resolveName(string* name, bool mark);


#define SHORTCUT(node_name) void visit ## node_name ## Node(node_name* node);

	// Special nodes

	SHORTCUT(ExpressionInStatements);

	// Usual nodes

	SHORTCUT(Func);
	SHORTCUT(Return);

	SHORTCUT(For);
	SHORTCUT(While);

	SHORTCUT(TryCatch);
	SHORTCUT(If);

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

#undef SHORTCUT
};