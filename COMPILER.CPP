#include "compiler.h"
#include "object.h"
#include "value.h"
#include "common.h"
#include "natives.h"
#include <iostream>

using namespace std;

void Compiler::preinit() {
	// Creates the first scope
	this->newScope();
	auto& uppermost_scope = this->bindings.back();

	uppermost_scope[this->firstSlotName] = { this->local_count, this->local_count++, false };

	if (this->parent)
		return;

	const auto& reg_names = pool->nativeBase->getNativeIDs();

	// Register native names
	for (auto& x : reg_names) {
		uppermost_scope[x] = { this->local_count, this->local_count++, false };
	}
}


ResolveResult Compiler::addLocal(const string& name) {
	auto& it = this->bindings[this->current_scope];
	if (it.count(name)) {
		throw this->error((name + " is already defined").data());
	}
	chunk_size pos = this->local_count++;
	it[name] = { pos, (int)it.size(), false};

	return {this, NAME_LOCAL, pos};
}

// Resolving names
ResolveResult Compiler::resolveName(const string& name, bool isfrominner) {
	for (int i = this->current_scope; i >= 0; --i) {
		auto& nameMapping = this->bindings[i];
		if (nameMapping.count(name)) {
			auto& local = nameMapping[name];

			// If variable search began from an inner function
			// We will mark the variable as captured
			// So we know when to use OP_CLOSE_UPVALUE instead of OP_POP
			if (isfrominner) {
				local.captured = true;
			}
			return {this, NAME_LOCAL, local.pos};
		}
	}

	if (this->upvalues.count(name)) {
		auto& upvalue = this->upvalues[name];
		return { upvalue.res.compiler, NAME_UPVALUE, upvalue.pos };
	}

	if (this->parent) {
		ResolveResult res = this->parent->resolveName(name, true);
		this->upvalues[name] = { this->upvalue_count, res.position, res, res.type == NAME_LOCAL };

		res.position = this->upvalue_count;
		res.type = NAME_UPVALUE;

		++this->upvalue_count;
		return res;
	}

	throw this->error(("Unresolved name: " + name).data());
}


FunctionObject* Compiler::produceFunction(int arity) {
	return new FunctionObject(this->name.c_str(), this->chunk, arity);
}

/// Visitors

#define FUNC_SHORTCUT(node_name) void Compiler::visit ## node_name ## Node(node_name* node)


// Special nodes

FUNC_SHORTCUT(ExpressionInStatements) {
	const auto& nodes = node->nodes;
	
	for (size_t i = 0; i < nodes.size(); ++i) {
		this->compile(nodes[i]);
		if (i != node->consider && !NotExpr.contains(nodes[i]->type)) {
			this->chunk->addOPs(OP_POP);
		}
	}
}


// Inner functions

pair<FunctionObject*, pair<int, bool>*> Compiler::compileInnerFunction(Stmts* stmts, const string& name, const Compiler::name_array& locals, int arity) {
	Compiler inner((Node*)stmts, this->chunk->getPool(), this, name);
	inner.start(locals, NULL);

	FunctionObject* obj = inner.produceFunction(arity);
	obj->upvalue_count = inner.getUpvalues().size();

#ifdef PRINT_CHUNK_AFTER_COMPILED
	printf("\n");
	this->printSignature();
	obj->print();
	printf("\n");
	inner.getChunk()->print();
#endif

	pair<int, bool>* upvalues = new pair<int, bool>[obj->upvalue_count];
	for (auto& x : inner.getUpvalues()) {
		upvalues[x.second.pos] = { x.second.capture_pos, x.second.isLocal };
	}

	return { obj, upvalues };
}

FUNC_SHORTCUT(Func) {
	const bool isanonymous = node->name.empty();
	ResolveResult res;

	if (!isanonymous) {
		res = this->addLocal(node->name);
	}

	auto pr = compileInnerFunction(node->stmts, node->name, node->argsname, node->arity);
	auto obj = pr.first;
	auto upvalues = pr.second;

	this->chunk->argedOP(OP_CLOSURE, this->chunk->addObjectConst(obj));

	for (auto end = upvalues + obj->countUpvalues(); upvalues != end; ++upvalues) {
		this->chunk->addOPs(upvalues->first / 256, upvalues->first & 255, upvalues->second);
	}

	upvalues -= obj->countUpvalues();
	delete[] upvalues;

	if (!isanonymous) {
		this->chunk->argedOP(OP_COPY, res.position);
	}
}

FUNC_SHORTCUT(Generator) {
	visitFuncNode(node->function);
	AS_FUNCTION(this->chunk->getConsts().back())->is_generator = true;
}

// Function caller

FUNC_SHORTCUT(Call) {
	size_t arg_count = node->args.size();

	this->compile(node->callee);
	this->compile(&node->args);

	this->chunk->argedOP(OP_CALL, arg_count);
}

FUNC_SHORTCUT(Invoke) {
	size_t arg_count = node->args.size();

	this->compile(node->callee->obj);
	this->compile(&node->args);
	this->compile(node->callee->prop);

	this->chunk->argedOP(OP_INVOKE, arg_count);
}

FUNC_SHORTCUT(New) {
	size_t arg_count;


	switch (node->activation->type) {
	case NODE_CALL: {
		auto call = (Call*)node->activation;
		arg_count = call->args.size();

		this->compile(call->callee);
		this->compile(&call->args);
		break;
	}
	case NODE_INVOKE: {
		auto invoke = (Invoke*)node->activation;
		arg_count = invoke->args.size();

		this->compile(invoke->callee);
		this->compile(&invoke->args);
		break;
	}
	default:
		throw this->error("Corrupted node");
	}

	this->chunk->argedOP(OP_NEW, arg_count);
}

// Functional statements

FUNC_SHORTCUT(Print) {
	this->compile(node->node);
	this->chunk->addOPs(OP_PRINT);
}

FUNC_SHORTCUT(VarDecl) {

	for (auto& declare : node->declarations) {
		ResolveResult res = this->addLocal(declare.name->getStr());

		this->compile(declare.init);
	}
	// Var declaration statement will pushes a variable field onto the stack
	// so there is no need to pop it over although it is a statement
}

// Functional syntax

FUNC_SHORTCUT(InstanceOf) {
	this->compile(node->b);
	this->compile(node->a);

	this->chunk->addOPs(OP_IS_PROTOTYPE);
}

// General scopes

FUNC_SHORTCUT(Stmts) {
	//for (auto exp = node->stmts; exp < node->end; ++exp) {
	for (auto exp : node->stmts) {
		this->compile(exp);
		if (!NotExpr.count(exp->type)) {
			this->chunk->addOPs(OP_POP);
		}
	}
}

FUNC_SHORTCUT(Block) {
	this->newScope();

	for (auto exp : node->stmts) {
		this->compile(exp);
		if (!NotExpr.count(exp->type)) {
			this->chunk->addOPs(OP_POP);
		}
	}

	this->exitScope();
}

// Special expressions

FUNC_SHORTCUT(ChainExpression) {
	if (node->nodes.empty()) {
		return;
	}

	for (size_t i = 0, csize = node->nodes.size() - 1; i < csize; ++i) {
		this->compile(node->nodes[i]);
		this->chunk->addOPs(OP_POP);
	}

	this->compile(node->nodes.back());
}

// Loops

FUNC_SHORTCUT(For) {
	if (node->init) {
		this->compile(node->init);
	}

	Node* cond = node->cond;
	if (cond && cond->type == NODE_LITERAL && ((Literal*)cond)->token.type == TOKEN_FALSE) {
		return;
	}

	size_t loop_start = this->chunk->length(), false_jump = UINT64_MAX;
	bool has_condition = cond && !(cond->type == NODE_LITERAL && ((Literal*)cond)->token.type == TOKEN_TRUE);

	if (has_condition) {
		this->compile(cond);

		false_jump = this->chunk->addGoto(OP_GOTO_IF_FALSE);
		this->chunk->addOPs(OP_POP);
	}

	this->listenFlags();
	this->compile(node->body);

	size_t continue_jump = this->chunk->length();
	if (node->incr) {
		this->compile(node->incr);
		this->chunk->addOPs(OP_POP);
	}
	this->chunk->argedOP(OP_GOTO, loop_start);

	size_t break_jump = this->chunk->length();
	if (has_condition) {
		this->chunk->writeUshort(false_jump, break_jump);
		this->chunk->addOPs(OP_POP);
	}

	this->resolveFlags(continue_jump, break_jump);
}

FUNC_SHORTCUT(ForEach) {
	ResolveResult iterable_holder = this->addSpecialVar(),
		container = this->defaultLocal(node->varname.getStr());
	
	this->compile(node->itexpr);

	const auto op_ptr = this->chunk->argedOP(OP_ITER, 0);
	this->chunk->addUshort(container.position);

	const auto loop_start = this->chunk->length();
	this->chunk->addOPs(OP_ITER_NEXT);

	this->compile(node->body);
	this->chunk->argedOP(OP_GOTO, loop_start);

	this->chunk->writeUshort(op_ptr, this->chunk->length());
	this->resolveFlags(loop_start, this->chunk->length());
	this->chunk->addOPs(OP_ITER_END);
}

FUNC_SHORTCUT(While) {
	if (node->exp->type == NODE_LITERAL && ((Literal*)node->exp)->token.type == TOKEN_FALSE) {
		return;
	}

	size_t loop_start = this->chunk->length(), false_jump = -1;

	if (!(node->exp->type == NODE_LITERAL && ((Literal*)node->exp)->token.type == TOKEN_TRUE)) {
		this->compile(node->exp);
		
		false_jump = this->chunk->addGoto(OP_GOTO_IF_FALSE);
		this->chunk->addOPs(OP_POP);
	}

	this->listenFlags();
	this->compile(node->body);

	this->chunk->argedOP(OP_GOTO, loop_start);

	size_t break_jump = this->chunk->length();

	if (false_jump != -1) {
		this->chunk->writeUshort(false_jump, break_jump);
		this->chunk->addOPs(OP_POP);
	}

	this->resolveFlags(loop_start, break_jump);
}

// Control flow

FUNC_SHORTCUT(Break) {
	this->addControlFlow(FLOW_BREAK, this->chunk->addGoto(OP_GOTO_SET_STACK));
	this->chunk->addUshort(this->flag_stack_pos);
}

FUNC_SHORTCUT(Continue) {
	this->addControlFlow(FLOW_CONTINUE, this->chunk->addGoto(OP_GOTO_SET_STACK));
	this->chunk->addUshort(this->flag_stack_pos);
}

FUNC_SHORTCUT(Return) {
	this->compile(node->retexp);
	this->chunk->addOPs(OP_RETURN);
}

FUNC_SHORTCUT(Yield) {
	this->compile(node->value);
	this->chunk->addOPs(OP_YIELD);
}

FUNC_SHORTCUT(Raise) {
	this->compile(node->raiseexp);
	this->chunk->addOPs(OP_RAISE);
}

FUNC_SHORTCUT(Or) {
	this->compile(node->a);

	size_t true_jump = this->chunk->addGoto(OP_GOTO_IF_TRUE);

	this->chunk->addOPs(OP_POP);
	this->compile(node->b);

	this->chunk->gotoTarget(true_jump);
}

FUNC_SHORTCUT(And) {
	this->compile(node->a);

	size_t false_jump = this->chunk->addGoto(OP_GOTO_IF_FALSE);

	this->chunk->addOPs(OP_POP);
	this->compile(node->b);

	this->chunk->gotoTarget(false_jump);
}

FUNC_SHORTCUT(If) {
	this->compile(node->main->exp);
	size_t false_jump_addr = this->chunk->addGoto(OP_GOTO_IF_FALSE);

	this->chunk->addOPs(OP_POP);
	this->compile(node->main->body);

	size_t main_true_jump_addr = this->chunk->addGoto(OP_GOTO);

	this->chunk->writeUshort(false_jump_addr, this->chunk->length());

	vector<size_t> elif_true_jump_addrs;

	for (auto elif : node->elifs) {
		this->chunk->writeUshort(false_jump_addr, this->chunk->length());
		this->chunk->addOPs(OP_POP);

		this->compile(elif->exp);
		false_jump_addr = this->chunk->addGoto(OP_GOTO_IF_FALSE);

		this->chunk->addOPs(OP_POP);
		this->compile(elif->body);

		elif_true_jump_addrs.push_back(this->chunk->addGoto(OP_GOTO));
	}

	this->chunk->writeUshort(false_jump_addr, this->chunk->length());
	this->chunk->addOPs(OP_POP);
	if (node->else_ != NULL) {
		this->compile(node->else_);
	}

	this->chunk->writeUshort(main_true_jump_addr, this->chunk->length());
	for (auto true_jump_addr : elif_true_jump_addrs) {
		this->chunk->writeUshort(true_jump_addr, this->chunk->length());
	}
}

FUNC_SHORTCUT(TryCatch) {
	this->newScope();
	int pos = this->chunk->addGoto(OP_FLAG_CATCH);
	this->compile(node->tries);
	this->exitScope();


	if (node->excepts) {
		int trysuccess = this->chunk->addGoto(OP_GOTO);

		this->chunk->writeUshort(pos, this->chunk->length());
		this->chunk->addOPs(OP_UNFLAG_CATCH);

		this->newScope();
		if (node->catcher) {
			this->addLocal(node->catcher->getStr());
		}
		else {
			this->chunk->addOPs(OP_POP);
		}
		this->compile(node->excepts);
		this->exitScope();

		int catchsuccess = this->chunk->addGoto(OP_GOTO);

		this->chunk->writeUshort(trysuccess, this->chunk->length());
		this->chunk->addOPs(OP_UNFLAG_CATCH);

		this->chunk->writeUshort(catchsuccess, this->chunk->length());
	}
	else {
		this->chunk->writeUshort(pos, this->chunk->length());
		this->chunk->addOPs(OP_POP, OP_UNFLAG_CATCH);
	}
}

// Literals

FUNC_SHORTCUT(This) {
	this->chunk->argedOP(OP_COPY, 0);
}

FUNC_SHORTCUT(Literal) {
	if (node->v) {
		switch (TValue::vtype(*node->v)) {
		case VAL_BOOL:
			this->chunk->addOPs(AS_BOOLEAN(*node->v) ? OP_TRUE : OP_FALSE);
			break;
		case VAL_NULL:
			this->chunk->addOPs(OP_NULL);
			break;
		case VAL_NUM:
			this->chunk->argedOP(OP_CONST, this->chunk->addConst(AS_NUMBER(*node->v)));
			break;
		case VAL_OBJ:
			this->chunk->argedOP(OP_CONST, IS_STRING(*node->v) ?
				this->chunk->addConst(AS_STRING(*node->v)->content)
				:
				this->chunk->addRegisteredObjectConst(AS_OBJECT(*node->v))
			);
			break;
		}
	}
	else {
		switch (node->token.type) {
		case TOKEN_TRUE:
			this->chunk->addOPs(OP_TRUE);
			break;
		case TOKEN_FALSE:
			this->chunk->addOPs(OP_FALSE);
			break;
		case TOKEN_NULL:
			this->chunk->addOPs(OP_NULL);
			break;
		default:
			this->chunk->argedOP(OP_CONST, this->chunk->addTokenConst(node->token));
			break;
		}
	}
}

FUNC_SHORTCUT(ObjectGenerator) {
	// Expression order in stack:
	// |key|value|

	for (auto& x : node->props) {
		this->compile(x.first);
		this->compile(x.second);
	}
	if (node->prototypeExp) {
		this->compile(node->prototypeExp);
	}
	else {
		this->chunk->addOPs(OP_NULL);
	}

	this->chunk->argedOP(OP_GEN_OBJ, node->props.size());
}

FUNC_SHORTCUT(ListGenerator) {
	if (node->isLiteral && node->elems.size() > 2) {
		vector<Value> vec;
		for (const auto& x : node->elems) {
			vec.push_back(*AS_NODE(x, Literal)->v);
		}

		auto tup = this->chunk->getPool()->internTupleConst(vec);
		this->chunk->argedOP(OP_LIST_FROM_CONST_TUPLE, this->chunk->addTupleConst(tup));
	}
	else {
		this->compile(&node->elems);
		this->chunk->argedOP(OP_GEN_LIST, node->elems.size());
	}
}

FUNC_SHORTCUT(TupleGenerator) {
	if (node->isLiteral) {
		vector<Value> vec;
		for (const auto& x : node->elems) {
			vec.push_back(*AS_NODE(x, Literal)->v);
		}

		auto tup = this->chunk->getPool()->internTupleConst(vec);
		this->chunk->argedOP(OP_CONST, this->chunk->addTupleConst(tup));
	}
	else {
		this->compile(&node->elems);
		this->chunk->argedOP(OP_GEN_TUPLE, node->elems.size());
	}
}

FUNC_SHORTCUT(Var) {
	ResolveResult res = this->resolveName(node->name->getStr());

	if (res.type == NAME_LOCAL) {
		this->chunk->argedOP(OP_COPY, res.position);
	}
	else {
		this->chunk->argedOP(OP_GET_UPVALUE, res.position);
	}
}

// Binary operations

FUNC_SHORTCUT(Binary) {
	this->compile(node->a);
	this->compile(node->b);

	OP mathop;

	switch (node->optype) {
	case ASSIGN_ADD:
		mathop = OP_ADD;
		break;
	case ASSIGN_SUB:
		mathop = OP_SUB;
		break;
	case ASSIGN_MUL:
		mathop = OP_MUL;
		break;
	case ASSIGN_DIV:
		mathop = OP_DIV;
		break;
	case ASSIGN_POW:
		mathop = OP_POW;
		break;
	default:
		// Unreachable
		throw this->error("Unknown math operation");
	}

	this->chunk->addOPs(mathop);
}

FUNC_SHORTCUT(Compare) {
	this->compile(node->a);
	this->compile(node->b);

	OP cmpop;

	switch (node->compare_type) {
	case CMP_EQUAL:
		cmpop = OP_EQUAL;
		break;
	case CMP_NOT_EQUAL:
		cmpop = OP_NOT_EQUAL;
		break;
	case CMP_LESS:
		cmpop = OP_LESS;
		break;
	case CMP_GREATER:
		cmpop = OP_GREATER;
		break;
	case CMP_LESS_EQUAL:
		cmpop = OP_LESS_EQUAL;
		break;
	case CMP_GREATER_EQUAL:
		cmpop = OP_GREATER_EQUAL;
		break;
	default:
		// Unreachable
		throw this->error("Unknown comparison operation");
	}

	this->chunk->addOPs(cmpop);
}

FUNC_SHORTCUT(Assign) {
	Var* identifier = node->name;
	string name(identifier->name->start, identifier->name->length);
	ResolveResult res = this->resolveName(name);

	this->compile(node->value);

	if (res.type == NAME_LOCAL) {
		this->chunk->argedOP(OP_SET, res.position);
	}
	else {
		this->chunk->argedOP(OP_SET_UPVALUE, res.position);
	}
}

FUNC_SHORTCUT(Member) {
	this->compile(node->obj);
	this->compile(node->prop);
	this->chunk->addOPs(OP_MEM_GET);
}

FUNC_SHORTCUT(MemberAssign) {
	this->compile(node->val);
	this->compile(node->obj);
	this->compile(node->prop);
	this->chunk->addOPs(OP_MEM_SET);
}

// Unary operations

FUNC_SHORTCUT(Increase) {
	Var* identifier = node->var;
	string name(identifier->name->start, identifier->name->length);
	ResolveResult res = this->resolveName(name);

	if (res.type == NAME_LOCAL) {
		if (node->post_order) {
			this->chunk->argedOP(OP_COPY, res.position);
		}
		this->chunk->argedOP(OP_INCREASE, res.position);
		if (!node->post_order) {
			this->chunk->argedOP(OP_COPY, res.position);
		}
	}
	else {
		if (node->post_order) {
			this->chunk->argedOP(OP_GET_UPVALUE, res.position);
		}
		this->chunk->argedOP(OP_INCREASE_UPVALUE, res.position);
		if (!node->post_order) {
			this->chunk->argedOP(OP_GET_UPVALUE, res.position);
		}
	}
}

FUNC_SHORTCUT(Decrease) {
	Var* identifier = node->var;
	string name(identifier->name->start, identifier->name->length);
	ResolveResult res = this->resolveName(name);

	if (res.type == NAME_LOCAL) {
		if (node->post_order) {
			this->chunk->argedOP(OP_COPY, res.position);
		}
		this->chunk->argedOP(OP_DECREASE, res.position);
		if (!node->post_order) {
			this->chunk->argedOP(OP_COPY, res.position);
		}
	}
	else {
		if (node->post_order) {
			this->chunk->argedOP(OP_GET_UPVALUE, res.position);
		}
		this->chunk->argedOP(OP_DECREASE_UPVALUE, res.position);
		if (!node->post_order) {
			this->chunk->argedOP(OP_GET_UPVALUE, res.position);
		}
	}
}

FUNC_SHORTCUT(Not) {
	this->compile(node->node);
	this->chunk->addOPs(OP_NOT);
}

FUNC_SHORTCUT(Negate) {
	this->compile(node->node);
	this->chunk->addOPs(OP_NEGATE);
}

// Ternary operation(s)

FUNC_SHORTCUT(Ternary) {
	this->compile(node->cond);

	size_t false_jump = this->chunk->addGoto(OP_GOTO_IF_FALSE);

	this->chunk->addOPs(OP_POP);
	this->compile(node->texp);
	size_t end_jump = this->chunk->addGoto(OP_GOTO);

	this->chunk->gotoTarget(false_jump);

	this->chunk->addOPs(OP_POP);
	this->compile(node->fexp);

	this->chunk->gotoTarget(end_jump);
}


#undef FUNC_SHORTCUT