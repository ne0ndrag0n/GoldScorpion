#include "lexer.hpp"
#include "variant_visitor.hpp"
#include <unordered_map>
#include <vector>

namespace GoldScorpion {

	static const std::unordered_map< std::string, TokenType > TOKEN_MAP = {
		{ "def", TokenType::TOKEN_DEF },
		{ "as", TokenType::TOKEN_AS },
		{ "u8", TokenType::TOKEN_U8 },
		{ "u16", TokenType::TOKEN_U16 },
		{ "u32", TokenType::TOKEN_U32 },
		{ "s8", TokenType::TOKEN_S8 },
		{ "s16", TokenType::TOKEN_S16 },
		{ "s32", TokenType::TOKEN_S32 },
		{ "string", TokenType::TOKEN_STRING },
		{ "+", TokenType::TOKEN_PLUS },
		{ "-", TokenType::TOKEN_MINUS },
		{ "*", TokenType::TOKEN_ASTERISK },
		{ "/", TokenType::TOKEN_FORWARD_SLASH },
		{ ".", TokenType::TOKEN_DOT },
		{ "(", TokenType::TOKEN_LEFT_PAREN },
		{ ")", TokenType::TOKEN_RIGHT_PAREN },
		{ "=", TokenType::TOKEN_EQUALS },
		{ "==", TokenType::TOKEN_DOUBLE_EQUALS },
		{ "!=", TokenType::TOKEN_NOT_EQUALS },
		{ "not", TokenType::TOKEN_NOT },
		{ "then", TokenType::TOKEN_THEN },
		{ "function", TokenType::TOKEN_FUNCTION },
		{ ",", TokenType::TOKEN_COMMA },
		{ "end", TokenType::TOKEN_END },
		{ "type", TokenType::TOKEN_TYPE },
		{ "return", TokenType::TOKEN_RETURN },
		{ "import", TokenType::TOKEN_IMPORT },
		{ "[", TokenType::TOKEN_LEFT_BRACKET },
		{ "]", TokenType::TOKEN_RIGHT_BRACKET },
		{ "asm", TokenType::TOKEN_ASM },
		{ "this", TokenType::TOKEN_THIS },
		{ ">", TokenType::TOKEN_GREATER_THAN },
		{ "<", TokenType::TOKEN_LESS_THAN },
		{ ">=", TokenType::TOKEN_GREATER_THAN_EQUAL },
		{ "<=", TokenType::TOKEN_LESS_THAN_EQUAL },
		{ ">>", TokenType::TOKEN_SHIFT_RIGHT },
		{ "<<", TokenType::TOKEN_SHIFT_LEFT },
		{ "byref", TokenType::TOKEN_BYREF },
		{ "\"", TokenType::TOKEN_DOUBLE_QUOTE },
		{ "if", TokenType::TOKEN_IF },
		{ "for", TokenType::TOKEN_FOR },
		{ "while", TokenType::TOKEN_WHILE },
		{ "to", TokenType::TOKEN_TO },
		{ "every", TokenType::TOKEN_EVERY },
		{ "else", TokenType::TOKEN_ELSE },
		{ "break", TokenType::TOKEN_BREAK },
		{ "continue", TokenType::TOKEN_CONTINUE },
		{ "and", TokenType::TOKEN_AND },
		{ "or", TokenType::TOKEN_OR },
		{ "xor", TokenType::TOKEN_XOR },
		{ "super", TokenType::TOKEN_SUPER }
	};

	static Token interpretToken( std::string& segment ) {
		Token result;

		// Otherwise we must verify token is one of the following multipart tokens
		auto it = TOKEN_MAP.find( segment );
		if( it != TOKEN_MAP.end() ) {
			// Constructed segment is one of a reserved set of keywords or symbols
			result.type = it->second;
		} else {
			// Constructed segment is an identifier
			result.type = TokenType::TOKEN_IDENTIFIER;
			result.value = segment;
		}

		return result;
	}

	static bool isNumeric( char c ) {
		return c >= '0' && c <= '9';
	}

	static bool isAlpha( char c ) {
		return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
	}

	static bool isValidSymbol( char c ) {
		switch( c ) {
			case '+':
			case '-':
			case '*':
			case '/':
			case '.':
			case '(':
			case ')':
			case '[':
			case ']':
			case '=':
			case '>':
			case '<':
			case ',':
				return true;
			default:
				return false;
		}
	}

	Result< std::vector< Token > > getTokens( std::string body ) {
		// Append an extra character to force-flush the buffer
		body += '\t';

		struct Line {
			unsigned int line = 1;
			unsigned int column = 1;
		};

		std::vector< Token > tokens;

		std::string component;
		bool lineContinuation = false;

		// Three flags describing types of valid contiguous sequences.
		// A state is entered by its beginning state, and ends when either
		// whitespace, or a non-criteria character is encountered.
		// Then, when a state ends, snip and add the token.

		// - Begins with a number and consists entirely of numbers
		bool numericState = false;
		// - Begins with a symbol and consists entirely of symbols
		bool symbolicState = false;
		// - Begins with a letter and consists entirely letters and/or numbers
		bool alphanumericState = false;

		// These two states are special "run-on" states that process their contents
		// in a lexical-agnostic fashion.
		bool stringState = false;
		bool commentState = false;
		for( const char& character : body ) {

			if( stringState ) {
				// Newlines are invalid in string state
				if( character == '\n' ) {
					return "Unexpected newline encountered";
				}

				if( character == '"' ) {
					// Exit string state and append string literal token
					tokens.push_back( Token{ TokenType::TOKEN_LITERAL_STRING, component } );

					// Reset state
					component = "";
					stringState = false;
				} else {
					// If in string state, unconditionally append character to current component
					component += character;
				}

				continue;
			} else if( commentState ) {
				// Do nothing unless a newline is seen, in which case, unset comment state
				if( character == '\n' ) {
					commentState = false;
				}

				continue;
			} else if( numericState ) {

				if( isNumeric( character ) ) {
					component += character;
					continue;
				} else {
					tokens.push_back( Token{ TokenType::TOKEN_LITERAL_INTEGER, std::stol( component ) } );
					numericState = false;
					component = "";
				}

			} else if( symbolicState ) {

				if( isValidSymbol( character ) ) {
					component += character;
					continue;
				} else {
					tokens.push_back( interpretToken( component ) );
					symbolicState = false;
					component = "";
				}

			} else if( alphanumericState ) {

				if( isAlpha( character ) || isNumeric( character ) ) {
					component += character;
					continue;
				} else {
					tokens.push_back( interpretToken( component ) );
					alphanumericState = false;
					component = "";
				}

			}

			// Else, we need to enter a new state
			switch( character ) {
				case '#': {
					// Enter comment state, which will skip parsing until the next newline is seen
					commentState = true;
					continue;
				}
				case '\n': {
					if( lineContinuation ) {
						// If line continuation operator is present
						// Then eat the newline instead of adding it to the token stream
						lineContinuation = false;
					} else {
						tokens.push_back( Token{ TokenType::TOKEN_NEWLINE, {} } );
					}

					continue;
				}
				case ' ':
				case '\t':
				case '\r':
				case '\v':
				case '\f': {
					// Continue directly
					continue;
				}
				case '\\': {
					// When encountering the line-continuation operator, set the lineContinuation flag for when the next \n is encountered
					lineContinuation = true;
					continue;
				}
				case '"': {
					// Enter string state, which will stop ordinary parsing and simply append items to component
					stringState = true;
					continue;
				}
				default: {
					// Set state depending on token encountered
					if( isNumeric( character ) ) {
						numericState = true;
						component += character;
					} else if( isAlpha( character ) ) {
						alphanumericState = true;
						component += character;
					} else if( isValidSymbol( character ) ){
						symbolicState = true;
						component += character;
					} else {
						return std::string( "Unexpected character: " ) + character;
					}
				}
			}
		}

		return tokens;
	}
}