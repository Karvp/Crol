#pragma once

// This file contain all AST node declarations
// When declaring one, if it has allocated new memory during its life-time,
// You must override the deconstructor no matter how
// This is because the default constructor used (~Node) only frees the memory
// of it's type (Node). Deconstructor of other containers like 'string', 'vector',...
// won't be called

#include "token.h"
#include <vector>
#include <string>
#include <map>

using namespace std;

class Value;


enum AssignType {
	ASSIGN_SET, ASSIGN_ADD, ASSIGN_SUB, ASSIGN_MUL, ASSIGN_DIV, ASSIGN_POW
};

enum CompareType {
	CMP_EQUAL, CMP_NOT_EQUAL, CMP_LESS, CMP_LESS_EQUAL, CMP_GREATER, CMP_GREATER_EQUAL
};

enum NodeType {
	NODE_LITERAL, NODE_VAR, NODE_THIS, NODE_OBJECT_GENERATOR, NODE_LIST_GENERATOR, NODE_TUPLE_GENERATOR,
	NODE_CALL, NODE_INVOKE, NODE_MEMBER, NODE_MEMBER_ASSIGN,
	NODE_NOT, NODE_NEGATE, NODE_INCREASE, NODE_DECREASE,
	NODE_NUMERIC_BINARY,
	NODE_COMPARE,
	NODE_AND, NODE_OR,
	NODE_TERNARY,
	NODE_ASSIGN,
	NODE_INSTANCEOF,
	NODE_CHAIN_EXPRESSION,
	NODE_FUNC, NODE_NEW,
	NODE_VAR_DECL,
	NODE_PRINT, NODE_BREAK, NODE_CONTINUE, NODE_RETURN, NODE_RAISE,
	NODE_STMTS, NODE_BLOCK,
	NODE_CONDITION,
	NODE_TRY_CATCH, NODE_IF, NODE_WHILE, NODE_FOR,

	NODE_EXPRESSION_IN_STATEMENTS
};



#define IS_NODE(node, name) ((node).type == name)
#define AS_NODE(node, type) ((type*)node)
#define NULL_NODE ((Node*)NULL);


class Node {
public:
	NodeType type;

	virtual ~Node() {}
};

class Literal : public Node {
public:
	Literal(const Token& token) : token(token) {
		this->type = NODE_LITERAL;
	}
	Literal(Value* value) : v(value) {
		this->type = NODE_LITERAL;
	}

	~Literal() override {
		if (v)
			delete v;
	}

	Token token;
	Value* v = NULL;
};



class Print : public Node {
public:
	Print(Node* node) : node(node) {
		this->type = NODE_PRINT;
	}

	Node* node;
};

class This : public Node {
public:
	This() {
		this->type = NODE_THIS;
	}
};

class Declaration {
public:
	Declaration(Token* name, Node* init) : name(name), init(init) {}

	Token* name;
	Node* init;
};

class VarDecl : public Node {
public:
	VarDecl(const vector<Declaration>& declarations) : declarations(declarations) {
		this->type = NODE_VAR_DECL;
	}

	~VarDecl() override {}

	vector<Declaration> declarations;
};

class Var : public Node {
public:
	Var(Token* name) : name(name) {
		this->type = NODE_VAR;
	}

	Token* name;
};

class Member : public Node {
public:
	Member(Node* obj, Node* prop) : obj(obj), prop(prop) {
		this->type = NODE_MEMBER;
	}

	Node* obj, * prop;
};

class MemberAssign : public Node {
public:
	MemberAssign(Node* obj, Node* prop, Node* val) : obj(obj), prop(prop), val(val) {
		this->type = NODE_MEMBER_ASSIGN;
	}

	Node* obj, * prop, * val;
	//AssignType assign_type;
};

class Increase : public Node {
public:
	Increase(Var* node, bool post_order) : var(node), post_order(post_order) {
		this->type = NODE_INCREASE;
	}

	Var* var;
	bool post_order;
};

class Decrease : public Node {
public:
	Decrease(Var* node, bool post_order) : var(node), post_order(post_order) {
		this->type = NODE_DECREASE;
	}

	Var* var;
	bool post_order;
};

class Assign : public Node {
public:
	Assign(Var* name, Node* value, AssignType type) : name(name), value(value), assign_type(type) {
		this->type = NODE_ASSIGN;
	}

	Var* name;
	Node* value;
	AssignType assign_type;
};

class Binary : public Node {
public:
	Binary(Node* a, Node* b, AssignType optype) : a(a), b(b), optype(optype) {
		this->type = NODE_NUMERIC_BINARY;
	}

	Node* a, * b;
	AssignType optype;
};

class Compare : public Node {
public:
	Compare(Node* a, Node* b, CompareType type) : a(a), b(b), compare_type(type) {
		this->type = NODE_COMPARE;
	}

	Node* a, * b;
	CompareType compare_type;
};

class Stmts : public Node {
public:
	Stmts(const vector<Node*>& stmts) : stmts(stmts) {
		this->type = NODE_STMTS;
	}

	~Stmts() override {}

	vector<Node*> stmts;
};

class Block : public Node {
public:
	Block(const vector<Node*>& stmts) : stmts(stmts) {
		this->type = NODE_BLOCK;
	}

	~Block() override {}

	vector<Node*> stmts;
};

class Func : public Node {
public:
	Func(string name, Stmts* stmts, const vector<string>& argsname) : name(name), argsname(argsname), stmts(stmts), arity(argsname.size()) {
		this->type = NODE_FUNC;
	}

	~Func() override {}

	int arity;
	string name;
	Stmts* stmts;
	vector<string> argsname;
};

class Call : public Node {
public:
	Call(Node* callee, const vector<Node*>& args) : callee(callee), args(args) {
		this->type = NODE_CALL;
	}

	~Call() override {}

	Node* callee;
	vector<Node*> args;
};

class Invoke : public Node {
public:
	Invoke(Member* callee, const vector<Node*>& args) : callee(callee), args(args) {
		this->type = NODE_INVOKE;
	}
	~Invoke() override {}

	Member* callee;
	vector<Node*> args;
};

class New : public Node {
public:
	New(Node* activation) : activation(activation) {
		this->type = NODE_NEW;
	}

	// An activation is an invoktion or call
	Node* activation;
};

class Return : public Node {
public:
	Return(Node* retexp) : retexp(retexp) {
		this->type = NODE_RETURN;
	}

	Node* retexp;
};

class Raise : public Node {
public:
	Raise(Node* raiseexp) : raiseexp(raiseexp) {
		this->type = NODE_RAISE;
	}

	Node* raiseexp;
};

class Break : public Node {
public:
	Break() {
		this->type = NODE_BREAK;
	}
};

class Continue : public Node {
public:
	Continue() {
		this->type = NODE_CONTINUE;
	}
};

class Condition : public Node {
public:
	Condition(Node* exp, Block* body) : exp(exp), body(body) {
		this->type = NODE_CONDITION;
	}

	Node* exp;
	Block* body;
};

class TryCatch : public Node {
public:
	TryCatch(Stmts* tries, Token* catcher, Stmts* excepts) : tries(tries), excepts(excepts), catcher(catcher) {
		this->type = NODE_TRY_CATCH;
	}

	Stmts* tries, * excepts;
	Token* catcher;
};

class If : public Node {
public:
	If(Condition* main, const vector<Condition*>& elifs, Block* else_) : main(main), elifs(elifs), else_(else_) {
		this->type = NODE_IF;
	}

	~If() override {}

	Condition* main;
	Block* else_;
	vector<Condition*> elifs;
};

class Not : public Node {
public:
	Not(Node* node) : node(node) {
		this->type = NODE_NOT;
	}

	Node* node;
};

class Negate : public Node {
public:
	Negate(Node* node) : node(node) {
		this->type = NODE_NEGATE;
	}

	Node* node;
};

class While : public Node {
public:
	While(Condition* cond) : exp(cond->exp), body(cond->body) {
		this->type = NODE_WHILE;

		delete cond;
	}

	Node* exp;
	Block* body;
};

class For : public Node {
public:
	For(Node* init, Node* cond, Node* incr, Block* body) : init(init), cond(cond), incr(incr), body(body) {
		this->type = NODE_FOR;
	}

	Node* init, * cond, * incr;
	Block* body;
};


class ChainExpression : public Node {
public:
	ChainExpression(const vector<Node*>& nodes) : nodes(nodes) {
		this->type = NODE_CHAIN_EXPRESSION;
	}

	~ChainExpression() override {}

	vector<Node*> nodes;
};

// Expression keywords

class InstanceOf : public Node {
public:
	InstanceOf(Node* a, Node* b) : a(a), b(b) {
		this->type = NODE_INSTANCEOF;
	}

	Node* a, * b;
};

// Generators
class ObjectGenerator : public Node {
public:
	ObjectGenerator(const vector<pair<Node*, Node*>>& props, Node* prototypeExp) : props(props), prototypeExp(prototypeExp) {
		this->type = NODE_OBJECT_GENERATOR;
	}

	~ObjectGenerator() override {}

	vector<pair<Node*, Node*>> props;
	Node* prototypeExp;
};

class TupleGenerator : public Node {
public:
	TupleGenerator(const vector<Node*>& elems) : elems(elems) {
		this->type = NODE_TUPLE_GENERATOR;
	}

	~TupleGenerator() override {}

	vector<Node*> elems;
	bool isLiteral = false;
};

class ListGenerator : public Node {
public:
	ListGenerator(const vector<Node*>& elems) : elems(elems) {
		this->type = NODE_LIST_GENERATOR;
	}

	~ListGenerator() override {}

	vector<Node*> elems;
	bool isLiteral = false;
};

class And : public Node {
public:
	And(Node* a, Node* b) : a(a), b(b) {
		this->type = NODE_AND;
	}

	Node* a, * b;
};

class Or : public Node {
public:
	Or(Node* a, Node* b) : a(a), b(b) {
		this->type = NODE_OR;
	}

	Node* a, * b;
};

class Ternary : public Node {
public:
	Ternary(Node* cond, Node* texp, Node* fexp) : cond(cond), texp(texp), fexp(fexp) {
		this->type = NODE_TERNARY;
	}

	Node* cond, * texp, * fexp;
};

//
// Special Nodes
//

class ExpressionInStatements : public Node {
public:
	ExpressionInStatements(const vector<Node*>& nodes, const size_t& consider) : nodes(nodes), consider(consider) {
		this->type = NODE_EXPRESSION_IN_STATEMENTS;
	}

	~ExpressionInStatements() override {}

	vector<Node*> nodes;
	size_t consider;
};