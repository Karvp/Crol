#pragma once

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "token.h"

using namespace std;

#define isDigit(c) ('0' <= c && c <= '9')
#define isAlpha(c) (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_')

class Lexer {

protected:
	vector<Token> tokens;

public:
	Lexer(const string& src) : source(src) {
		this->index = 0;
		this->line = 1;
		this->length = source.length();
	}

	bool start() {
		try {
			this->lex();
			return true;
		}
		catch (exception) {
			this->tokens.clear();
			return false;
		}
	}

	const decltype(tokens)& getTokens() {
		return tokens;
	}

protected:
	const string& source;
	int line;
	size_t index, length;

	map<string, TokenType> keywords{
		{"false", TOKEN_FALSE},
		{"true", TOKEN_TRUE},
		{"null", TOKEN_NULL},
		{"var", TOKEN_VAR},
		{"func", TOKEN_FUNC},
		{"return", TOKEN_RETURN},
		{"continue", TOKEN_CONTINUE},
		{"break", TOKEN_BREAK},
		{"if", TOKEN_IF},
		{"elif", TOKEN_ELIF},
		{"else", TOKEN_ELSE},
		{"while", TOKEN_WHILE},
		{"for", TOKEN_FOR},
		{"this", TOKEN_THIS},
		{"class", TOKEN_CLASS},
		{"instanceof", TOKEN_INSTANCEOF},
		{"print", TOKEN_PRINT},
		{"and", TOKEN_AND},
		{"or", TOKEN_OR},
		{"try", TOKEN_TRY},
		{"catch", TOKEN_CATCH},
		{"raise", TOKEN_RAISE},
		{"new", TOKEN_NEW}
	};

	exception error(const char* msg) {
		printf("LexerError at '%c': %s at line %d\n", this->source[this->index], msg, this->line);
		return exception(msg);
	}

	void pushToken(Token token) {
		this->tokens.push_back(token);
	}

	void addToken(TokenType type, size_t start, int length) {
		this->pushToken({ type, &this->source[start], length, this->line });
	}

	void addToken(TokenType type, int length) {
		this->pushToken({ type, &this->source[this->index], length, this->line });
	}

	void addString(char quote) {
		++this->index;
		size_t start = this->index;

		while (this->index < this->length && this->source[this->index] != quote) {
			switch (this->source[this->index]) {
			case '\\':
				++this->index;
				break;
			case '\n':
				++this->line;
				break;
			}
			++this->index;
		}

		if (this->index < this->length) {
			this->addToken(TOKEN_STRING, start, this->index - start);
		}
		else {
			throw this->error("Expected a quote to end a string");
		}
	}

	void addNumber() {
		size_t start = this->index;

		while (this->index < this->length && isDigit(this->source[this->index])) {
			++this->index;
		}

		if (this->index < this->length && this->source[this->index] == '.') {
			++this->index;
			if (this->index == this->length || !isDigit(this->source[this->index])) {
				throw this->error("Expected digit");
			}

			while (this->index < this->length && isDigit(this->source[this->index])) {
				++this->index;
			}
		}

		this->addToken(TOKEN_NUMBER, start, this->index - start);
	}

	void addIdentifier() {
		size_t start = this->index++;

		while (this->index < this->length && (isAlpha(this->source[this->index]) || isDigit(this->source[this->index]))) {
			++this->index;
		}

		size_t length = this->index - start;

		string lexeme(&this->source[start], length);

		if (keywords.count(lexeme)) {
			this->addToken(keywords[lexeme], start, length);
		}
		else {
			this->addToken(TOKEN_IDENTIFIER, start, length);
		}
	}

	void lex();
};