#include "parser.h"
#include "chunk.h"
#include "common.h"

using namespace std;

#define NODE(t) this->createNode<t>


Stmts* Parser::blockToStmts() {
	Block* block = this->blockStatement();
	const auto block_stmts = block->stmts;

	delete block;
	this->popNode();

	return NODE(Stmts)(block_stmts);
}

Stmts* Parser::statements() {
	vector<Node*> stmts;
	while (!this->match(TOKEN_EOF)) {
		try {
			stmts.push_back(this->statement());
		}
		catch (exception exc) {
			printf("%s\n", exc.what());
			this->synchronizeError();
		}
	}
	stmts.shrink_to_fit();
	return NODE(Stmts)(stmts);
}

Node* Parser::statement() {
	if (this->match(TOKEN_PRINT)) {
		return this->printStatement();
	}
	else if (this->match(TOKEN_VAR)) {
		return this->varDeclStatment();
	}
	else if (this->match(TOKEN_OPEN_BRACE)) {
		Node* node = this->blockStatement();
		this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after block statement");
		return node;
	}
	else if (this->match(TOKEN_IF)) {
		return this->ifStatement();
	}
	else if (this->match(TOKEN_TRY)) {
		return this->tryCatchStatement();
	}
	else if (this->match(TOKEN_WHILE)) {
		return this->whileStatement();
	}
	else if (this->match(TOKEN_FOR)) {
		return this->forStatement();
	}
	else if (this->match(TOKEN_FOREACH)) {
		return this->forEachStatement();
	}
	else if (this->match(TOKEN_RETURN)) {
		return this->returnStatement();
	}
	else if (this->match(TOKEN_RAISE)) {
		return this->raiseStatement();
	}
	else if (this->match(TOKEN_BREAK)) {
		return this->breakStatement();
	}
	else if (this->match(TOKEN_CONTINUE)) {
		return this->continueStatement();
	}
	else {
		// expression statement
		Node* exp = this->expression();
		this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after an expression");
		return exp;
	}
}

Return* Parser::returnStatement() {
	if (this->functype == FUNC_NONE) {
		throw this->error("Cannot use return statement out of a function");
	}
	Node* retexp = this->expression();
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after return statement");
	return NODE(Return)(retexp);
}

Raise* Parser::raiseStatement() {
	Node* raiseexp = this->expression();
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after raise statement");
	return NODE(Raise)(raiseexp);
}

Break* Parser::breakStatement() {
	if (this->looptype == LOOP_NONE) {
		throw this->error("Cannot use break statement out of a loop");
	}
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after break statement");
	return NODE(Break)();
}

Continue* Parser::continueStatement() {
	if (this->looptype == LOOP_NONE) {
		throw this->error("Cannot use continue statement out of a loop");
	}
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after continue statement");
	return NODE(Continue)();
}

For* Parser::forStatement() {
	LoopType parentloop = this->looptype;
	this->looptype = LOOP_FOR;

	this->consume(TOKEN_OPEN_PAREN, "Expected open parenthese");

	Node* init;
	if (this->match(TOKEN_SEMICOLON)) {
		init = NULL;
	}
	else if (this->match(TOKEN_VAR)) {
		init = this->varDeclStatment();
	}
	else {
		init = this->expression();
		this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after initialize expression");
	}

	Node* cond;
	if (this->match(TOKEN_SEMICOLON)) {
		cond = NULL;
	}
	else {
		cond = this->expression();
		this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after main expression");
	}

	Node* inc;
	if (this->match(TOKEN_CLOSE_PAREN)) {
		inc = NULL;
	}
	else {
		inc = this->expression();
		this->consume(TOKEN_CLOSE_PAREN, "Expected a closing parenthese after increment expression");
	}

	this->consume(TOKEN_OPEN_BRACE, "Expected an opening brace");

	For* forloop = NODE(For)(init, cond, inc, this->blockStatement());

	this->looptype = parentloop;
	return forloop;
}

ForEach* Parser::forEachStatement() {
	LoopType parentloop = this->looptype;
	this->looptype = LOOP_FOR;

	this->consume(TOKEN_OPEN_PAREN, "Expected open parenthese");
	this->consume(TOKEN_IDENTIFIER, "Expected an identifier");

	Token id = this->previous();

	this->consume(TOKEN_IN, "Expected `in` keyword");

	Node* itexpr = this->expression();

	this->consume(TOKEN_CLOSE_PAREN, "Expected close parenthese");
	this->consume(TOKEN_OPEN_BRACE, "Expected a `{` before loop's body");

	auto node = NODE(ForEach)(id, itexpr, this->blockStatement());

	this->looptype = parentloop;
	return node;
}

While* Parser::whileStatement() {
	LoopType parentloop = this->looptype;
	this->looptype = LOOP_WHILE;

	While* whileloop = NODE(While)(this->conditionComponent());

	this->looptype = parentloop;
	return whileloop;
}

If* Parser::ifStatement() {
	Condition* main = this->conditionComponent();
	vector<Condition*> elifs;

	while (this->match(TOKEN_ELIF)) {
		elifs.push_back(this->conditionComponent());
	}

	Block* else_ = NULL;

	if (this->match(TOKEN_ELSE)) {
		this->consume(TOKEN_OPEN_BRACE, "Expected a '{'");
		else_ = this->blockStatement();
	}

	return NODE(If)(main, elifs, else_);
}

TryCatch* Parser::tryCatchStatement() {
	this->consume(TOKEN_OPEN_BRACE, "Expected open brace after try keyword");
	Stmts* tries = this->blockToStmts();

	if (this->match(TOKEN_CATCH)) {
		Token* catcher = NULL;

		if (this->match(TOKEN_OPEN_PAREN)) {
			this->consume(TOKEN_IDENTIFIER, "Expected catcher variable name");
			catcher = (Token*)&this->previous();
			this->consume(TOKEN_CLOSE_PAREN, "Expected closing parenthese after catcher variable name");
		}

		this->consume(TOKEN_OPEN_BRACE, "Expected open brace before except statement block");
		return NODE(TryCatch)(tries, catcher, this->blockToStmts());
	}

	return NODE(TryCatch)(tries, NULL_TOKEN, (Stmts*)NULL);
}

Condition* Parser::conditionComponent() {
	this->consume(TOKEN_OPEN_PAREN, "Expected opening parenthese");
	Node* exp = this->expression();
	this->consume(TOKEN_CLOSE_PAREN, "Expected closing parenthese");
	this->consume(TOKEN_OPEN_BRACE, "Expected a '{'");
	return NODE(Condition)(exp, this->blockStatement());
}

Block* Parser::blockStatement() {
	vector<Node*> stmts;
	while (!this->match(TOKEN_CLOSE_BRACE)) {
		stmts.push_back(this->statement());
	}
	stmts.shrink_to_fit();
	return NODE(Block)(stmts);
}

Print* Parser::printStatement() {
	Print* stmt = NODE(Print)(this->expression());
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after print statement");
	return stmt;
}

VarDecl* Parser::varDeclStatment() {
	vector<Declaration> declarations;

	do {
		this->consume(TOKEN_IDENTIFIER, "Expected an identifier");
		const Token& name = this->previous();
		Node* init = NULL;
		if (this->match(TOKEN_EQUAL)) {
			init = this->expression();
		}
		else {
			init = NODE(Literal)(Token( TOKEN_NULL, name.start + name.length, 0, name.line ));
		}
		declarations.emplace_back((Token*)&name, init);
	} while (this->match(TOKEN_COMMA));

	declarations.shrink_to_fit();

	VarDecl* stmt = NODE(VarDecl)(declarations);
	this->consume(TOKEN_SEMICOLON, "Expected a semi-colon after variable declaration");
	return stmt;
}

Node* Parser::expression() {
	return this->assign();
}

Node* Parser::assign() {
	Node* a = this->ternary();

	if (this->match(TOKEN_EQUAL)) {
		switch (a->type) {
		case NODE_VAR:
			return NODE(Assign)((Var*)a, this->assign(), ASSIGN_SET);
		case NODE_MEMBER:
			return NODE(MemberAssign)(((Member*)a)->obj, ((Member*)a)->prop, this->assign());
		default:
			this->error("Invalid assignment target");
		}
	}

	return a;
}

Node* Parser::ternary() {
	Node* a = this->instanceOf();

	if (this->match(TOKEN_QUES)) {
		Node* texp = this->ternary();
		this->consume(TOKEN_COLON, "Expected a colon between true and false expressions");
		a = NODE(Ternary)(a, texp, this->ternary());
	}

	return a;
}

Node* Parser::instanceOf() {
	Node* a = this->_or();

	if (this->match(TOKEN_INSTANCEOF)) {
		return NODE(InstanceOf)(a, this->_or());
	}

	return a;
}

Node* Parser::_or() {
	Node* a = this->_and();

	while (this->match(TOKEN_OR)) {
		a = NODE(Or)(a, this->_and());
	}

	return a;
}

Node* Parser::_and() {
	Node* a = this->comparison();

	while (this->match(TOKEN_AND)) {
		a = NODE(And)(a, this->comparison());
	}

	return a;
}

Node* Parser::comparison() {
	Node* a = this->term();

	while (this->match(TOKEN_EQ_EQ) || this->match(TOKEN_NEQ) || this->match(TOKEN_LESS) || this->match(TOKEN_LESS_EQ) || this->match(TOKEN_GREATER) || this->match(TOKEN_GREATER_EQ)) {
		CompareType type;

		switch (this->previous().type) {
		case TOKEN_EQ_EQ:
			type = CMP_EQUAL;
			break;
		case TOKEN_NEQ:
			type = CMP_NOT_EQUAL;
			break;
		case TOKEN_LESS:
			type = CMP_LESS;
			break;
		case TOKEN_GREATER:
			type = CMP_GREATER;
			break;
		case TOKEN_LESS_EQ:
			type = CMP_LESS_EQUAL;
			break;
		case TOKEN_GREATER_EQ:
			type = CMP_GREATER_EQUAL;
			break;
		default:
			throw this->error("Unknown comparative token");
		}

		a = NODE(Compare)(a, this->term(), type);
	}
	return a;
}

Node* Parser::term() {
	Node* a = this->factor();
	AssignType type;
	while (this->check(TOKEN_PLUS) || this->check(TOKEN_MINUS)) {
		type = this->advance().type == TOKEN_PLUS ? ASSIGN_ADD : ASSIGN_SUB;
		a = NODE(Binary)(a, this->factor(), type);
	}
	return a;
}

Node* Parser::factor() {
	Node* a = this->pow();
	AssignType type;
	while (this->check(TOKEN_STAR) || this->check(TOKEN_SLASH)) {
		type = this->advance().type == TOKEN_STAR ? ASSIGN_MUL : ASSIGN_DIV;
		a = NODE(Binary)(a, this->pow(), type);
	}
	return a;
}

Node* Parser::pow() {
	Node* a = this->unary();
	if (this->match(TOKEN_POW)) {
		return NODE(Binary)(a, this->pow(), ASSIGN_POW);
	}
	return a;
}

Node* Parser::unary() {
	return this->unary(false);
}

Node* Parser::unary(bool require_reference) {
	Node* a;
	switch (this->advance().type) {
	case TOKEN_EXCLAM:
		a = NODE(Not)(this->unary(false));
		break;
	case TOKEN_MINUS:
		a = NODE(Negate)(this->unary(false));
		break;
	case TOKEN_PLUS_PLUS:
		a = NODE(Increase)((Var*)this->unary(true), false);
		break;
	case TOKEN_MINUS_MINUS:
		a = NODE(Decrease)((Var*)this->unary(true), false);
		break;
	default:
		this->back();

		a = this->singularity();

		if (this->match(TOKEN_PLUS_PLUS)) {
			if (a->type != NODE_VAR) {
				this->back();
				throw this->error("Invalid increment/decrement operand");
			}
			a = NODE(Increase)((Var*)a, true);
		}
		else if (this->match(TOKEN_MINUS_MINUS)) {
			if (a->type != NODE_VAR) {
				this->back();
				throw this->error("Invalid increment/decrement operand");
			}
			a = NODE(Decrease)((Var*)a, true);
		}
		break;
	}

	if (require_reference && a->type != NODE_VAR) {
		this->back();
		throw this->error("Invalid increment/decrement operand");
	}
	return a;
}

Node* Parser::singularity() {
	Node* a = this->literal();
	while (this->check(TOKEN_OPEN_PAREN) || this->check(TOKEN_DOT) || this->check(TOKEN_OPEN_SQ_PAREN)) {
		if (this->match(TOKEN_OPEN_PAREN)) {
			if (a->type == NODE_MEMBER) {
				a = NODE(Invoke)((Member*)a, this->argumentList());
			}
			else {
				a = NODE(Call)(a, this->argumentList());
			}
		} else if (this->match(TOKEN_DOT)) {
			this->consume(TOKEN_IDENTIFIER, "Expected property name in member expression");
			a = NODE(Member)(a, NODE(Literal)(this->previous()));
		}
		else if (this->match(TOKEN_OPEN_SQ_PAREN)) {
			a = NODE(Member)(a, this->expression());
			this->consume(TOKEN_CLOSE_SQ_PAREN, "Expected closing square parenthese after property expression");
		}
	}
	return a;
}

vector<Node*> Parser::argumentList() {
	vector<Node*> args;

	if (!this->match(TOKEN_CLOSE_PAREN)) {
		args.push_back(this->expression());
		while (!this->match(TOKEN_CLOSE_PAREN)) {
			if (args.size() >= UINT16_MAX) {
				throw this->error("Maximum number of arguments reached");
			}

			this->consume(TOKEN_COMMA, "Expected a comma after an argument");
			args.push_back(this->expression());
		}
	}

	args.shrink_to_fit();

	return args;
}

Func* Parser::functionLikeExpression() {
	string name;

	if (!this->match(TOKEN_OPEN_PAREN)) {
		this->consume(TOKEN_IDENTIFIER, "Expected function name");
		name = this->previous().getStr();
		this->consume(TOKEN_OPEN_PAREN, "Expected open parenthese before argument names");
	}

	vector<string> argnames;

	if (!this->match(TOKEN_CLOSE_PAREN)) {
		this->consume(TOKEN_IDENTIFIER, "Expected an argument name");
		argnames.emplace_back(this->previous().getStr());

		while (!this->match(TOKEN_CLOSE_PAREN)) {
			if (argnames.size() >= UINT16_MAX) {
				throw this->error("Maximum number of arguments reached");
			}

			this->consume(TOKEN_COMMA, "Expected a comma after an argument name");
			this->consume(TOKEN_IDENTIFIER, "Expected an argument name");
			argnames.emplace_back(this->previous().getStr());
		}
	}

	this->consume(TOKEN_OPEN_BRACE, "Expected open brace before function body");

	return NODE(Func)(name, this->blockToStmts(), argnames);
}

Func* Parser::functionExpression() {
	LoopType old_ltp = this->looptype;
	FuncType old_ftp = this->functype;

	this->looptype = LOOP_NONE;
	this->functype = FUNC_NORMAL;

	auto func = functionLikeExpression();

	this->looptype = old_ltp;
	this->functype = old_ftp;

	return func;
}

Generator* Parser::generatorFunctionExpression() {
	this->consume(TOKEN_FUNC, "Expected `func` keyword after `gator` keyword");

	LoopType old_ltp = this->looptype;
	FuncType old_ftp = this->functype;

	this->looptype = LOOP_NONE;
	this->functype = FUNC_GENERATOR;

	auto func = functionLikeExpression();

	this->looptype = old_ltp;
	this->functype = old_ftp;

	return NODE(Generator)(func);
}

New* Parser::newExpression() {
	Node* node = this->singularity();
	if (node->type != NODE_CALL && node->type != NODE_INVOKE) {
		throw this->error("`new` expression must be followed by an activation (invoke or call)");
	}
	return NODE(New)(node);
}

Yield* Parser::yieldExpression() {
	if (this->functype != FUNC_GENERATOR) {
		throw this->error("`yield` only allowed inside a generator function");
	}
	return NODE(Yield)(this->expression());
}

ObjectGenerator* Parser::objectGeneratorExpression() {
	vector<pair<Node*, Node*>> items;
	Node* prototype = NULL;

	if (this->match(TOKEN_BACK_SLASH)) {
		prototype = this->expression();
		this->consume(TOKEN_CLOSE_BRACE, "Expected closing brace after prototype expression");

		return NODE(ObjectGenerator)(items, prototype);
	}

	while (!this->match(TOKEN_CLOSE_BRACE)) {
		if (!items.empty()) {
			this->consume(TOKEN_COMMA, "Expected a comma between each entries");
		}

		Node* keyExp = this->expression();
		this->consume(TOKEN_COLON, "Expected a colon after key expression");
		items.emplace_back(keyExp, this->expression());

		if (this->match(TOKEN_BACK_SLASH)) {
			prototype = this->expression();
			this->consume(TOKEN_CLOSE_BRACE, "Expected closing brace after prototype expression");
			break;
		}
	}

	return NODE(ObjectGenerator)(items, prototype);
}

ListGenerator* Parser::listGeneratorExpression() {
	vector<Node*> items;

	if (this->match(TOKEN_CLOSE_SQ_PAREN)) {
		return NODE(ListGenerator)(items);
	}

	items.push_back(this->expression());
	while (this->match(TOKEN_COMMA)) {
		items.push_back(this->expression());
	}

	this->consume(TOKEN_CLOSE_SQ_PAREN, "Expected closing square parenthese at the end of list generator expression");

	return NODE(ListGenerator)(items);
}

This* Parser::thisExpression() {
	if (this->functype != FUNC_NORMAL) {
		this->back();
		throw this->error("Cannot use 'this' outside of function");
	}
	return NODE(This)();
}

Node* Parser::preferredExpressionOrTupleGenerator() {
	if (this->match(TOKEN_CLOSE_PAREN)) {
		return NODE(TupleGenerator)(vector<Node*>{});
	}

	Node* exp = this->expression();
	
	if (!this->match(TOKEN_COMMA)) {
		this->consume(TOKEN_CLOSE_PAREN, "Expected closing parenthese");
		return exp;
	}

	vector<Node*> items{ exp };

	if (this->match(TOKEN_CLOSE_PAREN)) {
		return NODE(TupleGenerator)(items);
	}

	items.push_back(this->expression());
	while (this->match(TOKEN_COMMA)) {
		items.push_back(this->expression());
	}

	this->consume(TOKEN_CLOSE_PAREN, "Expected closing parenthese at the end of tuple generator expression");
	return NODE(TupleGenerator)(items);
}

Node* Parser::literal() {
	if (this->check(TOKEN_NUMBER) || this->check(TOKEN_STRING) || this->check(TOKEN_NULL) || this->check(TOKEN_TRUE) || this->check(TOKEN_FALSE)) {
		return NODE(Literal)(this->advance());
	}
	else if (this->match(TOKEN_IDENTIFIER)) {
		return NODE(Var)((Token*)&this->previous());
	}
	else if (this->match(TOKEN_THIS)) {
		return this->thisExpression();
	}
	else if (this->match(TOKEN_OPEN_PAREN)) {
		return this->preferredExpressionOrTupleGenerator();
	}
	else if (this->match(TOKEN_OPEN_SQ_PAREN)) {
		return this->listGeneratorExpression();
	}
	else if (this->match(TOKEN_OPEN_BRACE)) {
		return this->objectGeneratorExpression();
	}
	else if (this->match(TOKEN_FUNC)) {
		return this->functionExpression();
	}
	else if (this->match(TOKEN_GATOR)) {
		return this->generatorFunctionExpression();
	}
	else if (this->match(TOKEN_NEW)) {
		return this->newExpression();
	}
	else if (this->match(TOKEN_YIELD)) {
		return this->yieldExpression();
	}

	throw this->error("Unexpected token");
}


#undef NODE