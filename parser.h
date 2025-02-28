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
	FUNC_NONE, FUNC_NORMAL, FUNC_GENERATOR,
};

// Returns true if token type is not semi-colon or closing brace, false otherwise
inline constexpr bool isTokenSCorCB(const TokenType& type) {
	return type == TOKEN_SEMICOLON || type == TOKEN_CLOSE_BRACE;
}


class Parser {
public:
	Parser() {
	}

	Node* parse(vector<Token>& tokens, bool& is_parsed_as_expression) {
		this->tokens = &tokens;
		this->reset();

		show_error = ok = true;
		Node* root;

		try {
			if (is_parsed_as_expression = (tokens.size() >= 2 && !isTokenSCorCB((++tokens.rbegin())->type))) {
				try {
					show_error = false;
					root = this->expression();
					show_error = true;
				}
				catch (exception) {
					show_error = ok = true;
					this->reset();
					root = this->statements();
				}
			}
			else
				root = this->statements();

			ok = false;
			nodes.clear();
			return root;
		}
		catch (exception) {
			this->deleteNodes();
			return NULL;
		}
	}

	void reset() {
		looptype = LOOP_NONE;
		functype = FUNC_NONE;
		i = 0;

		deleteNodes();
	}

private:
	const set<TokenType> statement_starters{ TOKEN_IF, TOKEN_TRY, TOKEN_WHILE, TOKEN_FOR, TOKEN_BREAK, TOKEN_VAR, TOKEN_CONTINUE, TOKEN_FOREACH };
	vector<Token>* tokens = NULL;
	size_t i = 0;

	vector<Node*> nodes;

	LoopType looptype = LOOP_NONE;
	FuncType functype = FUNC_NONE;
	bool ok = false, show_error = true;

	inline const Token& current() {
		return (*this->tokens)[this->i];
	}

	void back() {
		--this->i;
	}

	const Token& previous() const noexcept {
		return (*this->tokens)[this->i - 1];
	}

	const Token& advance() noexcept {
		return (*this->tokens)[this->i++];
	}

	bool check(TokenType type) const noexcept {
		return (*this->tokens)[this->i].type == type;
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
		if (show_error)
			printf("ParseError at '%s' on line %d: %s\n", string(curr.start, curr.length).data(), curr.line, msg);
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
	ForEach* forEachStatement();
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
	Func* functionLikeExpression();
	Func* functionExpression();
	Generator* generatorFunctionExpression();
	ObjectGenerator* objectGeneratorExpression();
	ListGenerator* listGeneratorExpression();
	Node* preferredExpressionOrTupleGenerator();
	This* thisExpression();
	New* newExpression();
	Yield* yieldExpression();

	Stmts* blockToStmts();

	Node* literal();
};