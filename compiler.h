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
class StringObject;
class FunctionObject;

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
	typedef string name_type;
	typedef vector<name_type> name_array;

	typedef map<name_type, Local> locals_map;
	typedef vector<locals_map> mapping_type;

	Compiler(ValuePool* pool, Compiler* parent = NULL) : pool(pool), parent(parent) {
		preinit();
	}

	Compiler(Node* root, ValuePool* pool, Compiler* parent = NULL, string name = "(main)") : pool(pool), parent(parent) {
		preinit();
		initialize(root, name);
	}

	void preinit();

	void initialize(Node* root, string name = "(main)") {
		if (!this->name.empty())
			this->reset();

		this->root = root;
		this->name = name;

		chunk = new Chunk(pool);
		pool->currentCompiler = this;
		pool->addChunk(chunk);
	}

	~Compiler() {
		pool->currentCompiler = this->parent;
	}

	bool start(const Compiler::name_array& reg_names, bool save_globals = false) {
		try {
			auto& uppermost_scope = this->bindings.back();
			for (auto& x : reg_names) {
				uppermost_scope[x] = { this->local_count, this->local_count++, false };
			}

			this->compile(this->root);

			if (!save_globals && !this->parent) {
				this->exitScope();
				this->chunk->addOPs(OP_NULL);
			}
			this->chunk->addOPs(OP_RETURN);

			// Finalize Compilation
			this->chunk->finalize();
			// Optimize bytecode
			BytecodeOptimizer bc(this->chunk);
			bc.optimize();

			return true;
		}
		catch (exception) {
			// Reset incase failure
			this->reset();
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
		case NODE_FOREACH:
			SHORTCUT(ForEach);
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
		case NODE_GENERATOR:
			SHORTCUT(Generator);
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
		case NODE_YIELD:
			SHORTCUT(Yield);
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

	// Be cautious while using this function!
	// Always remember to register the object with the value pool
	FunctionObject* produceFunction(int arity = 0);

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

protected:
	void reset() {
		while (bindings.size() > 1) {
			bindings.pop_back();
		}
		local_count = bindings[0].size();

		current_scope = 0;
		listen_flags = false;
		flags.clear();
	}
private:
	const set<NodeType> NotExpr{ NODE_IF, NODE_BLOCK, NODE_STMTS, NODE_PRINT, NODE_VAR_DECL, NODE_WHILE, NODE_FOR, NODE_RETURN, NODE_TRY_CATCH, NODE_BREAK, NODE_CONTINUE, NODE_RAISE };
	const string firstSlotName = "this";

	ValuePool* pool;
	Node* root;
	Chunk* chunk;

	map<string, Upvalue> upvalues;

	mapping_type bindings;
	chunk_size local_count, upvalue_count, special_var_count = 0, flag_stack_pos;
	bool listen_flags = false;

	int current_scope = -1;

	vector<ControlFlow> flags;

	void newScope() {
		if (current_scope == MAX_SCOPE_DEPTH)
			throw this->error("Maximum scope depth reached (255)");

		++current_scope;
		if (current_scope + 1 >= bindings.size()) {
			bindings.emplace_back();
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
		if (!listen_flags) {
			error("Control flow not allowed");
		}
		this->flags.emplace_back(type, pos);
	}

	void listenFlags() {
		listen_flags = true;
		flag_stack_pos = local_count;
		this->flags.clear();
	}

	void resolveFlags(size_t continue_jump, size_t break_jump) {
		for (auto& flag : this->flags) {
			this->chunk->writeUshort(flag.pos, flag.type == FLOW_BREAK ? break_jump : continue_jump);
		}
		this->flags.clear();
		listen_flags = false;
		flag_stack_pos = MAX_CHUNK_SIZE;
	}


	ResolveResult addLocal(const string& name);
	ResolveResult defaultLocal(const string& name) {
		for (int i = this->current_scope; i >= 0; --i) {
			auto& nameMapping = this->bindings[i];
			if (nameMapping.count(name)) {
				return { this, NAME_LOCAL, nameMapping[name].pos };
			}
		}

		return this->addLocal(name);
	}
	ResolveResult addSpecialVar() {
		return this->addLocal(string("$") + to_string(special_var_count++));
	}

	// Resolving names
	ResolveResult resolveName(const string& name, bool isfrominner = false);


	pair<FunctionObject*, pair<int, bool>*> compileInnerFunction(Stmts* stmts, const string& name, const Compiler::name_array& locals, int arity);

#define SHORTCUT(node_name) void visit ## node_name ## Node(node_name* node);

	// Special nodes

	SHORTCUT(ExpressionInStatements);

	// Usual nodes

	SHORTCUT(Func);
	SHORTCUT(Generator);

	SHORTCUT(For);
	SHORTCUT(ForEach);
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
	SHORTCUT(Return);
	SHORTCUT(Yield);
	
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