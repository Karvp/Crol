#include "chunk.h"
#include "object.h"
#include "token.h"

using namespace std;

size_t Chunk::addConst(const string& str) {
	StringObject* stri = this->pool->internStr(str);
	if (hub->hasString(stri)) {
		return hub->getString(stri);
	}
	this->consts.emplace_back(toValue(stri));
	return hub->getString(stri) = this->consts.size() - 1;
}
size_t Chunk::addConst(const double& number) {
	Value num = NUM_VAL(number);
	if (hub->hasNumber(num)) {
		return hub->getNumber(num);
	}
	this->consts.push_back(num);
	return hub->getNumber(num) = this->consts.size() - 1;
}

size_t Chunk::addTokenConst(const Token& token) {
	switch (token.type) {
	case TOKEN_NUMBER:
		return this->addConst(strtod(token.start, NULL));
	case TOKEN_STRING:
		return this->addConst(Token::toString((Token*) &token));
	case TOKEN_IDENTIFIER:
		return this->addConst(string(token.start, token.length));

	default:
		// Unreachable
		throw exception("Cannot convert Token to Value");
	}
}

Chunk* Chunk::copy() {
	throw exception("Deprecated feature");

	auto chunk = new Chunk();
	chunk->ops.insert(chunk->ops.end(), ops.begin(), ops.end());
	chunk->consts.insert(chunk->consts.end(), consts.begin(), consts.end());

	for (auto& v : chunk->consts) {
		if (IS_OBJECT(v)) {
			v = toValue(AS_OBJECT(v)->obj_copy());
		}
	}

	return chunk;
}

	
void Chunk::print() {

#define READ_USHORT(o) ((((chunk_size)*(o++)) << 8) + *(o++))
#define READ_UCHAR(o) (*(o++))
#define GET_USHORT(o) ((((chunk_size)*(o)) << 8) + *((o) + 1))

	printf("========================================================\n\n");

	OperationIterator opit(this);

	for (const auto& op : opit) {
		printf("%d | ", opit.opPos(op));

		switch (op.arg_count) {
		case 0:
			printf("%s\n", op.name());
			break;
		case 2:
			switch (op.type) {
			case OP_CONST: {
				unsigned short pos = GET_USHORT(op.arg);
				auto& constVal = this->consts[pos];

				printf("%s    %d -> ", op.name(), pos);

				if (IS_STRINGOBJ(constVal)) {
					printf("'");
				}
				TValue::print(constVal);
				if (IS_STRINGOBJ(constVal)) {
					printf("'");
				}

				printf("\n");
				break;
			}
			case OP_LIST_FROM_CONST_TUPLE: {
				unsigned short pos = GET_USHORT(op.arg);

				printf("%s    %d -> ", op.name(), pos);
				TValue::print(this->consts[pos]);
				printf("\n");
				break;
			}
			case OP_GOTO:
			case OP_GOTO_IF_FALSE:
			case OP_GOTO_IF_TRUE:
				printf("%s  ->  %d\n", op.name(), GET_USHORT(op.arg));
				break;
			default:
				printf("%s    %d\n", op.name(), GET_USHORT(op.arg));
				break;
			}
			break;
		case 4:
			switch (op.type) {
			case OP_ITER:
				printf("%s    %d  ->  %d\n", op.name(), GET_USHORT(op.arg), GET_USHORT(op.arg + 2));
				break;
			case OP_GOTO_SET_STACK:
				printf("%s  ->  %d    %d\n", op.name(), GET_USHORT(op.arg), GET_USHORT(op.arg + 2));
				break;
			default:
				printf("UNKNOWN\n");
				break;
			}
			break;
		default:
			switch (op.type) {
			case OP_CLOSURE: {
				auto ptr = op.arg;
				auto pos = READ_USHORT(ptr);

				printf("%s    %d    ", op.name(), pos);
				auto func = AS_TOBJECT(this->consts[pos], FunctionObject);
				func->print();
				printf("\n");

				for (int i = func->upvalue_count - 1; i >= 0; --i) {
					int capture_pos = READ_USHORT(ptr);
					printf("            %s    %d\n", READ_UCHAR(ptr) ? "local" : "upvalue", capture_pos);
				}
				break;
			}
			default:
				printf("UNKNOWN\n");
				break;
			}
			break;
		}
	}
	printf("\n========================================================\n");

#undef READ_USHORT
#undef READ_UCHAR
}


void OperationIterator::transform() {
#define READ(o) (*(o++))
#define READ_DOUBLE(o) ((READ(o) << 8) + READ(o))
#define NEXT(o, t) ((o) += (t))


	bool run = true;

	bytecode* ops = &chunk.getOPs()[0], * start = ops, * end = ops + chunk.length();
	OP current;

	while (ops != end) {
		switch (current = (OP)*ops++) {

			// Quadratic op
		case OP_ITER:
		case OP_GOTO_SET_STACK:
			this->markOperation(current, ops, 4);
			NEXT(ops, 4);
			break;

			// Double args
		case OP_CALL:
		case OP_INVOKE:
		case OP_NEW:
		case OP_COPY:
		case OP_SET:
		case OP_INCREASE:
		case OP_DECREASE:
		case OP_INCREASE_UPVALUE:
		case OP_DECREASE_UPVALUE:
		case OP_GOTO:
		case OP_GOTO_IF_FALSE:
		case OP_GOTO_IF_TRUE:
		case OP_SET_UPVALUE:
		case OP_GET_UPVALUE:
		case OP_GEN_OBJ:
		case OP_GEN_LIST:
		case OP_GEN_TUPLE:
		case OP_FLAG_CATCH:
		case OP_CONST:
		case OP_LIST_FROM_CONST_TUPLE:
			this->markOperation(current, ops, 2);
			NEXT(ops, 2);
			break;

		case OP_CLOSURE: {
			bytecode* start = ops;
			auto x = AS_TOBJECT(chunk.getConsts()[READ_DOUBLE(ops)], FunctionObject);
			NEXT(ops, 3 * x->upvalue_count);
			this->markOperation(current, start, 2 + 3 * x->upvalue_count);
			break;
		}

					   // no arg
		case OP_IS_PROTOTYPE:
		case OP_CLOSE_UPVALUE:
		case OP_EQUAL:
		case OP_NOT_EQUAL:
		case OP_LESS:
		case OP_GREATER:
		case OP_LESS_EQUAL:
		case OP_GREATER_EQUAL:
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
		case OP_POW:
		case OP_NOT:
		case OP_NEGATE:
		case OP_POP:
		case OP_TRUE:
		case OP_FALSE:
		case OP_PRINT:
		case OP_RETURN:
		case OP_YIELD:
		case OP_MEM_SET:
		case OP_MEM_GET:
		case OP_NULL:
		case OP_RAISE:
		case OP_UNFLAG_CATCH:
		case OP_ITER_NEXT:
		case OP_ITER_END:
			this->markOperation(current, (bytecode*)NULL, 0);
			break;

		default:
			printf("BytecodeOptimizer <WARNING>: encountered unknown operation: %d\n", current);
			break;
		}
	}

	this->finalizeMarkings();

#undef READ_USHORT
#undef READ_UCHAR
#undef NEXT
}