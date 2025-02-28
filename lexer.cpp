#include "lexer.h"

void Lexer::lex() {
	while (this->index < this->length) {
		switch ((*this->source)[this->index]) {
		case ':':
			this->addToken(TOKEN_COLON, 1);
			break;
		case ';':
			this->addToken(TOKEN_SEMICOLON, 1);
			break;
		case '.':
			this->addToken(TOKEN_DOT, 1);
			break;
		case ',':
			this->addToken(TOKEN_COMMA, 1);
			break;
		case '(':
			this->addToken(TOKEN_OPEN_PAREN, 1);
			break;
		case ')':
			this->addToken(TOKEN_CLOSE_PAREN, 1);
			break;
		case '[':
			this->addToken(TOKEN_OPEN_SQ_PAREN, 1);
			break;
		case ']':
			this->addToken(TOKEN_CLOSE_SQ_PAREN, 1);
			break;
		case '{':
			this->addToken(TOKEN_OPEN_BRACE, 1);
			break;
		case '}':
			this->addToken(TOKEN_CLOSE_BRACE, 1);
			break;
		case '+':
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '+') {
				this->addToken(TOKEN_PLUS_PLUS, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_PLUS, 1);
			}
			break;
		case '-':
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '-') {
				this->addToken(TOKEN_MINUS_MINUS, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_MINUS, 1);
			}
			break;
		case '*':
			this->addToken(TOKEN_STAR, 1);
			break;
		case '/':
			this->addToken(TOKEN_SLASH, 1);
			break;
		case '\\':
			this->addToken(TOKEN_BACK_SLASH, 1);
			break;
		case '^':
			this->addToken(TOKEN_POW, 1);
			break;
		case '=': {
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '=') {
				this->addToken(TOKEN_EQ_EQ, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_EQUAL, 1);
			}
			break;
		}
		case '<': {
			if (this->index + 1 < this->length && (*this->source)[this->index] == '=') {
				this->addToken(TOKEN_LESS_EQ, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_LESS, 1);
			}
			break;
		}
		case '>': {
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '=') {
				this->addToken(TOKEN_GREATER_EQ, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_GREATER, 1);
			}
			break;
		}
		case '!': {
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '=') {
				this->addToken(TOKEN_NEQ, 2);
				++this->index;
			}
			else {
				this->addToken(TOKEN_EXCLAM, 1);
			}
			break;
		}
		case '?':
			this->addToken(TOKEN_QUES, 1);
			break;
		case '&':
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '&') {
				this->addToken(TOKEN_AND, 2);
				++this->index;
				break;
			}
			throw this->error("Unexpected token");
		case '|':
			if (this->index + 1 < this->length && (*this->source)[this->index + 1] == '|') {
				this->addToken(TOKEN_OR, 2);
				++this->index;
				break;
			}
			throw this->error("Unexpected token");
		case '"':
		case '\'':
			this->addString((*this->source)[this->index]);
			break;
		case ' ':
		case '\t':
		case '\r':
			break;
		case '\n':
			++this->line;
			break;
		default:
			if (isDigit((*this->source)[this->index])) {
				this->addNumber();
			}
			else if (isAlpha((*this->source)[this->index])) {
				this->addIdentifier();
			}
			else {
				throw this->error("Unexpected token");
			}
			continue;
		}
		++this->index;
	}

	// End of file token
	this->tokens.push_back(Token( TOKEN_EOF, &(*this->source)[this->index], 0, this->line ));
}