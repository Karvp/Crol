#include "common.h"

using namespace std;

string tokenToString(const Token* token) {
	return string(token->start, token->length);
}