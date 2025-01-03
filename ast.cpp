#include "ast.h"

using namespace std;

void AST::scan(Node* curr) {
	if (curr == NULL) {
		return;
	}

	this->nodes.push_back(curr);

	switch (curr->type) {
	case NODE_NOT: {
		Not* node = (Not*)curr;
		this->scan(node->node);
		break;
	}
	case NODE_NEGATE: {
		Negate* node = (Negate*)curr;
		this->scan(node->node);
		break;
	}
	case NODE_INCREASE: {
		Increase* node = (Increase*)curr;
		this->scan(node->var);
		break;
	}
	case NODE_DECREASE: {
		Decrease* node = (Decrease*)curr;
		this->scan(node->var);
		break;
	}
	case NODE_NUMERIC_BINARY: {
		Binary* node = (Binary*)curr;
		this->scan(node->a, node->b);
		break;
	}
	case NODE_COMPARE: {
		Compare* node = (Compare*)curr;
		this->scan(node->a, node->b);
		break;
	}
	case NODE_AND: {
		And* node = (And*)curr;
		this->scan(node->a, node->b);
		break;
	}
	case NODE_OR: {
		Or* node = (Or*)curr;
		this->scan(node->a, node->b);
		break;
	}
	case NODE_TERNARY: {
		Ternary* node = (Ternary*)curr;
		this->scan(node->cond, node->fexp, node->texp);
		break;
	}
	case NODE_ASSIGN: {
		Assign* node = (Assign*)curr;
		this->scan(node->name, node->value);
		break;
	}
	case NODE_VAR_DECL: {
		VarDecl* node = (VarDecl*)curr;
		for (auto& declare : node->declarations) {
			this->scan(declare.init);
		}
		break;
	}
	case NODE_PRINT: {
		Print* node = (Print*)curr;
		this->scan(node->node);
		break;
	}
	case NODE_STMTS: {
		Stmts* node = (Stmts*)curr;
		for (auto& x : node->stmts) {
			this->scan(x);
		}
		break;
	}
	case NODE_BLOCK: {
		Block* node = (Block*)curr;
		for (auto& x : node->stmts) {
			this->scan(x);
		}
		break;
	}
	case NODE_CONDITION: {
		Condition* node = (Condition*)curr;
		this->scan(node->exp, node->body);
		break;
	}
	case NODE_IF: {
		If* node = (If*)curr;
		this->scan(node->main, node->else_);
		for (auto& x : node->elifs) {
			this->scan(x);
		}
		break;
	}
	case NODE_WHILE: {
		While* node = (While*)curr;
		this->scan(node->exp, node->body);
		break;
	}
	case NODE_FOR: {
		For* node = (For*)curr;
		this->scan(node->init, node->cond, node->incr, node->body);
		break;
	}
	case NODE_FUNC: {
		Func* node = (Func*)curr;
		this->scan(node->stmts);
		break;
	}
	case NODE_CALL: {
		Call* node = (Call*)curr;
		this->scan(node->callee);
		for (auto& x : node->args) {
			this->scan(x);
		}
		break;
	}
	case NODE_MEMBER: {
		Member* node = (Member*)curr;
		this->scan(node->obj, node->prop);
		break;
	}
	case NODE_MEMBER_ASSIGN: {
		MemberAssign* node = (MemberAssign*)curr;
		this->scan(node->obj, node->prop, node->val);
		break;
	}
	case NODE_INVOKE: {
		Invoke* node = (Invoke*)curr;
		this->scan(node->callee);
		for (auto& x : node->args) {
			this->scan(x);
		}
		break;
	}
	case NODE_OBJECT_GENERATOR: {
		ObjectGenerator* objgen = (ObjectGenerator*)curr;
		this->scan(objgen->prototypeExp);
		for (auto& x : objgen->props) {
			this->scan(x.first, x.second);
		}
		break;
	}
	case NODE_LIST_GENERATOR:
		for (auto& x : ((ListGenerator*)curr)->elems) {
			this->scan(x);
		}
		break;
	case NODE_TUPLE_GENERATOR:
		for (auto& x : ((TupleGenerator*)curr)->elems) {
			this->scan(x);
		}
		break;
	case NODE_TRY_CATCH: {
		auto node = (TryCatch*)curr;
		this->scan(node->tries, node->excepts);
		break;
	}
	case NODE_RAISE:
		this->scan(((Raise*)curr)->raiseexp);
		break;
	case NODE_NEW:
		this->scan(((New*)curr)->activation);
		break;
	case NODE_INSTANCEOF: {
		auto node = (InstanceOf*)curr;
		this->scan(node->a, node->b);
		break;
	}
	case NODE_CHAIN_EXPRESSION:
		for (auto& x : ((ChainExpression*)curr)->nodes) {
			this->scan(x);
		}
		break;
	case NODE_EXPRESSION_IN_STATEMENTS: 
		for (auto& x : ((ExpressionInStatements*)curr)->nodes) {
			this->scan(x);
		}
		break;
	}
}