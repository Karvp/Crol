#pragma once
#include <string>
#include "common.h"

using namespace std;

enum _TokenType {
	// Single marks
	TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT,
	TOKEN_OPEN_PAREN, TOKEN_CLOSE_PAREN, TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE, TOKEN_OPEN_SQ_PAREN, TOKEN_CLOSE_SQ_PAREN,
	TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_BACK_SLASH, TOKEN_POW, TOKEN_EQUAL, TOKEN_GREATER, TOKEN_LESS,
	TOKEN_EXCLAM, TOKEN_QUES,
	// Double marks
	TOKEN_EQ_EQ, TOKEN_NEQ,
	TOKEN_LESS_EQ, TOKEN_GREATER_EQ,
	TOKEN_AND, TOKEN_OR,
	TOKEN_PLUS_PLUS, TOKEN_MINUS_MINUS,
	// Keywords
	TOKEN_FALSE, TOKEN_TRUE, TOKEN_NULL,
	TOKEN_VAR, TOKEN_FUNC, TOKEN_NEW,
	TOKEN_INSTANCEOF, TOKEN_IN,
	TOKEN_RETURN, TOKEN_CONTINUE, TOKEN_BREAK, TOKEN_RAISE,
	TOKEN_TRY, TOKEN_CATCH,
	TOKEN_IF, TOKEN_ELIF, TOKEN_ELSE,
	TOKEN_WHILE, TOKEN_FOR, TOKEN_FOREACH,
	TOKEN_THIS, TOKEN_CLASS,
	TOKEN_YIELD, TOKEN_GATOR,
	// Literals
	TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENTIFIER,
	// Specials
	TOKEN_EOF, TOKEN_PRINT,
};
typedef uint8_t TokenType;

#define NULL_TOKEN ((Token*)NULL)

struct Token {
	TokenType type;
	unsigned short length;
	unsigned int line;
	const char* start;

	Token() {}
	Token(TokenType type, const char* start, unsigned short length, unsigned int line) : type(type), start(start), length(length), line(line) {}

	inline constexpr string getStr() const noexcept {
		return string(start, length);
	}

	static void stringToRepr(const string& str, string& output) {
		for (const auto& x : str) {
			switch (x) {
			case '\\':
				output.append("\\\\");
				break;
			case '\n':
				output.append("\\n");
				break;
			case '\t':
				output.append("\\t");
				break;
			case '\b':
				output.append("\\b");
				break;
			case '\r':
				output.append("\\r");
				break;
			case '\v':
				output.append("\\v");
				break;
			case '\a':
				output.append("\\a");
				break;
			case '\f':
				output.append("\\f");
				break;
			case '\'':
				output.append("\\'");
				break;
			case '\"':
				output.append("\\\"");
				break;
			default:
				output.push_back(x);
				break;
			}
		}
	}
	static string toString(Token* token) {
		const char* raw = token->start;

		string output;
		size_t index = 0, length = token->length;

		while (index < length) {
			switch (raw[index]) {
			case '\\':
				switch (raw[++index]) {
				case 'n':
					output.push_back('\n');
					break;
				case 't':
					output.push_back('\t');
					break;
				case 'b':
					output.push_back('\b');
					break;
				case 'r':
					output.push_back('\r');
					break;
				case 'v':
					output.push_back('\v');
					break;
				case 'a':
					output.push_back('\a');
					break;
				case 'f':
					output.push_back('\f');
					break;
				default:
					output.push_back(raw[index]);
					break;
				}
				++index;
				continue;
			case '\n':
				++index;
				break;
			}
			output.push_back(raw[index++]);
		}

		return output;
	}
};