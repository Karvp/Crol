#pragma once

#include <string>
#include <vector>
#include <set>
#include "token.h"
#include "nodes.h"

using namespace std;

enum LoopType {
	LOOP_NONE, LOOP_WHILE, LOOP_FOR,
};

enum FuncType {
	FUNC_NONE, FUNC_FUNC
};


class Parser {
public:
	Parser(const vector<Token>& tokens) : tokens(tokens), i(0), looptype(LOOP_NONE), functype(FUNC_NONE) {
	}

	Node* parse() {
		ok = true;
		Node* root = this->statements();

		if (ok) {
			ok = false;
			nodes.clear();
			return root;
		}
		else {
			this->deleteNodes();
			return NULL;
		}
	}

private:
	const set<TokenType> statement_starters{ TOKEN_IF, TOKEN_TRY, TOKEN_WHILE, TOKEN_FOR, TOKEN_BREAK, TOKEN_VAR, TOKEN_CONTINUE };
	const vector<Token>& tokens;
	size_t i;

	vector<Node*> nodes;

	LoopType looptype;
	FuncType functype;
	bool ok = false;

	inline const Token& current() {
		return this->tokens[this->i];
	}

	void back() {
		--this->i;
	}

	const Token& previous() const noexcept {
		return this->tokens[this->i - 1];
	}

	const Token& advance() noexcept {
		return this->tokens[this->i++];
	}

	bool check(TokenType type) const noexcept {
		return this->tokens[this->i].type == type;
	}

	bool match(TokenType type) {
		if (this->check(type)) {
			++this->i;
			return true;
		}
		return false;
	}

	void consume(TokenType type, const char* msg) {
		if (!this->match(type)) {
			throw this->error(msg);
		}
	}

	exception error(const char* msg) {
		ok = false;

		Token curr = this->current();
		printf("ParseError at '%s': %s on line %d\n", string(curr.start, curr.length).data(), msg, curr.line);
		return exception(msg);
	}

	void synchronizeError() {
		do {
			if (this->match(TOKEN_SEMICOLON) or statement_starters.count(this->current().type)) {
				break;
			}
		} while (!this->check(TOKEN_EOF) && ++this->i);
	}


	template <typename T, typename... Args>
	T* createNode(Args... args) {
		auto node = new T(args...);
		this->nodes.push_back((Node*)node);
		return node;
	}

	constexpr void popNode() noexcept {
		this->nodes.pop_back();
	}

	void deleteNodes() {
		for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
			delete *it;
		}
		nodes.clear();
	}


	Stmts* statements();
	Node* statement();

	Return* returnStatement();
	Raise* raiseStatement();
	Break* breakStatement();
	Continue* continueStatement();

	While* whileStatement();
	For* forStatement();
	If* ifStatement();
	TryCatch* tryCatchStatement();
	Condition* conditionComponent();
	Block* blockStatement();
	Print* printStatement();
	VarDecl* varDeclStatment();

	Node* expression();
	Node* instanceOf();
	Node* assign();
	Node* ternary();
	Node* _or();
	Node* _and();
	Node* comparison();
	Node* term();
	Node* factor();
	Node* pow();

	Node* unary();
	Node* unary(bool require_reference);
	Node* singularity();

	vector<Node*> argumentList();
	Func* functionExpression();
	ObjectGenerator* objectGeneratorExpression();
	ListGenerator* listGeneratorExpression();
	Node* preferredExpressionOrTupleGenerator();
	This* thisExpression();
	New* newExpression();

	Stmts* blockToStmts();

	Node* literal();
};