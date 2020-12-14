#include "generator.hpp"
#include "variant_visitor.hpp"
#include "memory_tracker.hpp"
#include "arch/m68k/instruction.hpp"
#include "token.hpp"
#include <cstdio>
#include <exception>
#include <unordered_map>

namespace GoldScorpion {

	enum class ExpressionDataType { INVALID, U8, U16, U32, S8, S16, S32, STRING };

	static ExpressionDataType getType( const Expression& node, Assembly& assembly );
	static void generate( const Expression& node, Assembly& assembly );

	static const std::unordered_map< std::string, ExpressionDataType > types = {
		{ "u8", ExpressionDataType::U8 },
		{ "u16", ExpressionDataType::U16 },
		{ "u32", ExpressionDataType::U32 },
		{ "s8", ExpressionDataType::S8 },
		{ "s16", ExpressionDataType::S16 },
		{ "s32", ExpressionDataType::S32 },
		{ "string", ExpressionDataType::STRING }
	};

	static char getTypeComparison( ExpressionDataType type ) {
		switch( type ) {
			case ExpressionDataType::INVALID:
			default:
				return 0;
			case ExpressionDataType::U8:
			case ExpressionDataType::S8:
				return 1;
			case ExpressionDataType::U16:
			case ExpressionDataType::S16:
				return 2;
			case ExpressionDataType::U32:
			case ExpressionDataType::S32:
			case ExpressionDataType::STRING:
				return 3;
		}
	}

	static bool isSigned( ExpressionDataType type ) {
		switch( type ) {
			case ExpressionDataType::S8:
			case ExpressionDataType::S16:
			case ExpressionDataType::S32:
				return true;
			default:
				return false;
		}
	}

	static bool isOneSigned( ExpressionDataType a, ExpressionDataType b ) {
		return ( isSigned( a ) && !isSigned( b ) ) ||
			( !isSigned( a ) && isSigned( b ) );
	}

	static ExpressionDataType scrubSigned( ExpressionDataType type ) {
		switch( type ) {
			case ExpressionDataType::S8:
				return ExpressionDataType::U8;
			case ExpressionDataType::S16:
				return ExpressionDataType::U16;
			case ExpressionDataType::S32:
				return ExpressionDataType::U32;
			default:
				return type;
		}
	}

	static m68k::OperatorSize typeToWordSize( ExpressionDataType type ) {
		switch( type ) {
			case ExpressionDataType::U8:
			case ExpressionDataType::S8:
				return m68k::OperatorSize::BYTE;
			case ExpressionDataType::U16:
			case ExpressionDataType::S16:
				return m68k::OperatorSize::WORD;
			case ExpressionDataType::U32:
			case ExpressionDataType::S32:
			default:
				return m68k::OperatorSize::LONG;
		}
	}

	static ExpressionDataType getLiteralType( long literal ) {
		// Negative values mean a signed value is required
		if( literal < 0 ) {

			if( literal >= -127 ) {
				return ExpressionDataType::S8;
			}

			if( literal >= -32767 ) {
				return ExpressionDataType::S16;
			}

			return ExpressionDataType::S32;

		} else {
			
			if( literal <= 255 ) {
				return ExpressionDataType::U8;
			}

			if( literal <= 65535 ) {
				return ExpressionDataType::U16;
			}

			return ExpressionDataType::U32;
		}
	}

	static ExpressionDataType getIdentifierType( const std::string& typeId ) {
		auto it = types.find( typeId );
		if( it != types.end() ) {
			return it->second;
		}

		return ExpressionDataType::INVALID;
	}

	static long expectLong( const Token& token, const std::string& error ) {
		if( token.value ) {
			if( auto longValue = std::get_if< long >( &*( token.value ) ) ) {
				return *longValue;
			}
		}

		throw std::runtime_error( error );
	}

	static std::string expectString( const Token& token, const std::string& error ) {
		if( token.value ) {
			if( auto stringValue = std::get_if< std::string >( &*( token.value ) ) ) {
				return *stringValue;
			}
		}

		throw std::runtime_error( error );
	}

	static Token expectToken( const Primary& primary, const std::string& error ) {
		if( auto token = std::get_if< Token >( &primary.value ) ) {
			return *token;
		}

		throw std::runtime_error( error );
	}

	static ExpressionDataType getType( const Primary& node, Assembly& assembly ) {
		ExpressionDataType result;

		// We can directly determine the primary value for any Token variant
		// Otherwise, we need to go deeper
		std::visit( overloaded {
			[ &result, &assembly ]( const Token& token ) {
				// The only two types of token expected here are TOKEN_LITERAL_INTEGER, TOKEN_LITERAL_STRING, and TOKEN_IDENTIFIER
				// If TOKEN_IDENTIFIER is provided, the underlying type must not be of a custom type
				switch( token.type ) {
					case TokenType::TOKEN_LITERAL_INTEGER: {
						result = getLiteralType( expectLong( token, "Expected: long type for literal integer token" ) );
						break;
					}
					case TokenType::TOKEN_LITERAL_STRING: {
						result = ExpressionDataType::STRING;
						break;
					}
					case TokenType::TOKEN_IDENTIFIER: {
						std::string id = expectString( token, "Expected: string type for identifier token" );
						auto memoryQuery = assembly.memory.find( id );
						if( !memoryQuery ) {
							throw std::runtime_error( std::string( "Undefined identifier: " ) + id );
						}

						result = getIdentifierType( std::visit( overloaded {
							[]( const GlobalMemoryElement& element ) { return element.value.typeId; },
							[]( const StackMemoryElement& element ) { return element.value.typeId; }
						}, *memoryQuery ) );
						break;
					}
					default:
						throw std::runtime_error( "Expected: integer, string, or identifier as expression operand" );
				}
			},
			[ &result, &assembly ]( const std::unique_ptr< Expression >& expression ) {
				result = getType( *expression, assembly );
			}
		}, node.value );

		return result;
	}

	static ExpressionDataType getType( const BinaryExpression& node, Assembly& assembly ) {
		// The type of a BinaryExpression is the larger of the two children

		ExpressionDataType lhs = getType( *node.lhsValue, assembly );
		ExpressionDataType rhs = getType( *node.rhsValue, assembly );

		// If either lhs or rhs return an invalid comparison
		if( lhs == ExpressionDataType::INVALID || rhs == ExpressionDataType::INVALID ) {
			return ExpressionDataType::INVALID;
		}

		// Otherwise the data type of the BinaryExpression is the larger of lhs, rhs
		if( getTypeComparison( rhs ) >= getTypeComparison( lhs ) ) {
			return isOneSigned( lhs, rhs ) ? scrubSigned( rhs ) : rhs;
		} else {
			return isOneSigned( lhs, rhs ) ? scrubSigned( lhs ) : lhs;
		}
	}

	static ExpressionDataType getType( const Expression& node, Assembly& assembly ) {

		if( auto binaryExpression = std::get_if< std::unique_ptr< BinaryExpression > >( &node.value ) ) {
 			return getType( **binaryExpression, assembly );
		}

		if( auto primaryExpression = std::get_if< std::unique_ptr< Primary > >( &node.value ) ) {
			return getType( **primaryExpression, assembly );
		}

		// Many node types not yet implemented
		return ExpressionDataType::INVALID;
	}

	static void generate( const Primary& node, Assembly& assembly ) {
		std::visit( overloaded {
			[ &assembly, &node ]( const Token& token ) {

				if( token.type == TokenType::TOKEN_LITERAL_INTEGER ) {
					// Generate immediate move instruction onto stack
					assembly.instructions.push_back(
						m68k::Instruction {
							m68k::Operator::MOVE,
							typeToWordSize( getType( node, assembly ) ),
							m68k::Operand { 0, m68k::OperandType::IMMEDIATE, 0, expectLong( token, "Internal compiler error" ) },
							m68k::Operand { -1, m68k::OperandType::REGISTER_sp_INDIRECT, 0, 0 }
						}
					);
				} else {
					throw std::runtime_error( "Expected: TOKEN_LITERAL_INTEGER to generate Primary code" );
				}

			},
			[ &assembly ]( const std::unique_ptr< Expression >& expression ) {
				generate( *expression, assembly );
			}
		}, node.value );
	}

	static void generate( const BinaryExpression& node, Assembly& assembly ) {
		// Get type of left and right hand sides
		// The largest of the two types is used to generate code
		ExpressionDataType type = getType( node, assembly );
		m68k::OperatorSize wordSize = typeToWordSize( type );

		// Expressions are evaluated right to left
		// All operations work on stack
		// Elision step will take care of redundant assembly
		generate( *node.rhsValue, assembly );
		generate( *node.lhsValue, assembly );

		// The values in the stack right now should be literals
		// Move the LHS into d0 and pop stack
		assembly.instructions.push_back(
			m68k::Instruction {
				m68k::Operator::MOVE,
				wordSize,
				m68k::Operand{ 0, m68k::OperandType::REGISTER_sp_INDIRECT, 1, 0 },
				m68k::Operand{ 0, m68k::OperandType::REGISTER_d0, 0, 0 }
			}
		);

		// Now, depending on the operator given, apply the RHS
		switch( expectToken( *node.op, "Expected: Token as BinaryExpression operator" ).type ) {
			case TokenType::TOKEN_PLUS:
				assembly.instructions.push_back(
					m68k::Instruction {
						m68k::Operator::ADD,
						wordSize,
						m68k::Operand { 0, m68k::OperandType::REGISTER_sp_INDIRECT, 1, 0 },
						m68k::Operand { 0, m68k::OperandType::REGISTER_d0, 0, 0 }
					}
				);
				break;
			case TokenType::TOKEN_MINUS:
				assembly.instructions.push_back(
					m68k::Instruction {
						m68k::Operator::SUBTRACT,
						wordSize,
						m68k::Operand { 0, m68k::OperandType::REGISTER_sp_INDIRECT, 1, 0 },
						m68k::Operand { 0, m68k::OperandType::REGISTER_d0, 0, 0 }
					}
				);
				break;
			case TokenType::TOKEN_ASTERISK:				
				assembly.instructions.push_back(
					m68k::Instruction {
						isSigned( type ) ? m68k::Operator::MULTIPLY_SIGNED : m68k::Operator::MULTIPLY_UNSIGNED,
						wordSize,
						m68k::Operand { 0, m68k::OperandType::REGISTER_sp_INDIRECT, 1, 0 },
						m68k::Operand { 0, m68k::OperandType::REGISTER_d0, 0, 0 }
					}
				);
				break;
			case TokenType::TOKEN_FORWARD_SLASH:
				assembly.instructions.push_back(
					m68k::Instruction {
						isSigned( type ) ? m68k::Operator::DIVIDE_SIGNED : m68k::Operator::DIVIDE_UNSIGNED,
						wordSize,
						m68k::Operand { 0, m68k::OperandType::REGISTER_sp_INDIRECT, 1, 0 },
						m68k::Operand { 0, m68k::OperandType::REGISTER_d0, 0, 0 }
					}
				);
				break;
			default:
				throw std::runtime_error( "Expected: ., +, -, *, or / operator" );
		}
	}

	static void generate( const Expression& node, Assembly& assembly ) {

		if( auto binaryExpression = std::get_if< std::unique_ptr< BinaryExpression > >( &node.value ) ) {
			generate( **binaryExpression, assembly );
		}

	}

	Result< Assembly > generate( const Program& program ) {
		return "Not implemented";
	}

}