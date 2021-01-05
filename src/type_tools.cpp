#include "type_tools.hpp"
#include "tree_tools.hpp"
#include "variant_visitor.hpp"
#include "error.hpp"
#include "utility.hpp"

namespace GoldScorpion {

    static std::string getLiteralType( long literal ) {
		// Negative values mean a signed value is required
		if( literal < 0 ) {

			if( literal >= -127 ) {
				return "s8";
			}

			if( literal >= -32767 ) {
				return "s16";
			}

			return "s32";

		} else {

			if( literal <= 255 ) {
				return "u8";
			}

			if( literal <= 65535 ) {
				return "u16";
			}

			return "u32";
		}
	}

	static long expectLong( const Token& token ) {
		if( token.value ) {
			if( auto longValue = std::get_if< long >( &*( token.value ) ) ) {
				return *longValue;
			}
		}

		Error{ "Internal compiler error", token }.throwException();
		return 0;
	}

	static std::string expectString( const Token& token ) {
		if( token.value ) {
			if( auto stringValue = std::get_if< std::string >( &*( token.value ) ) ) {
				return *stringValue;
			}
		}

		Error{ "Internal compiler error", token }.throwException();
		return "";
	}

    static char getTypeComparison( const std::string& id ) {
        if( id == "u8" || id == "s8" ) {
            return 0;
        }

        if( id == "u16" || id == "s16" ) {
            return 1;
        }

        if( id == "u32" || id == "s32" || id == "string" ) {
            return 2;
        }

        return 3;
    }

    static bool isSigned( const std::string& id ) {
        return id == "s8" || id == "s16" || id == "s32";
    }

    static bool isOneSigned( const std::string& a, const std::string& b ) {
        return ( isSigned( a ) && !isSigned( b ) ) || ( !isSigned( a ) && isSigned( b ) );
    }

    static std::string scrubSigned( const std::string& id ) {
        if( id == "s8" ) { return "u8"; }
        if( id == "s16" ) { return "u16"; }
        if( id == "s32" ) { return "u32"; }

        return "<undefined>";
    }

    std::optional< TokenType > typeIdToTokenType( const std::string& id ) {
        if( id == "u8" ) { return TokenType::TOKEN_U8; }
        if( id == "u16" ) { return TokenType::TOKEN_U16; }
        if( id == "u32" ) { return TokenType::TOKEN_U32; }
        if( id == "s8" ) { return TokenType::TOKEN_S8; }
        if( id == "s16" ) { return TokenType::TOKEN_S16; }
        if( id == "s32" ) { return TokenType::TOKEN_S32; }
        if( id == "string" ) { return TokenType::TOKEN_STRING; }

        return {};
    }

    std::optional< std::string > tokenTypeToTypeId( const TokenType type ) {
        switch( type ) {
            default:
                return {};
            case TokenType::TOKEN_U8:
                return "u8";
            case TokenType::TOKEN_U16:
                return "u16";
            case TokenType::TOKEN_U32:
                return "u32";
            case TokenType::TOKEN_S8:
                return "s8";
            case TokenType::TOKEN_S16:
                return "s16";
            case TokenType::TOKEN_S32:
                return "s32";
            case TokenType::TOKEN_STRING:
                return "string";
        }
    }

    std::optional< std::string > tokenToTypeId( const Token& token ) {
        // Attempt to pull a type from the TokenType
        std::optional< std::string > typeId = tokenTypeToTypeId( token.type );
        if( typeId ) {
            return typeId;
        }

        // If that wasn't possible, then let's try to extract from an identifier w/string
        if( token.type == TokenType::TOKEN_IDENTIFIER && token.value ) {
            if( auto stringValue = std::get_if< std::string >( &*token.value ) ) {
                return *stringValue;
            }
        }

        return {};
    }

    std::string unwrapTypeId( const MemoryDataType& type ) {
		return std::visit( overloaded {
			[]( const FunctionType& element ) { Error{ "Internal compiler error (attempted simple unwrap of function type)", {} }.throwException(); return std::string( "" ); },
			[]( const ValueType& element ) { return element.id; }
		}, type );
    }

    std::string typeToString( const MemoryDataType& type ) {
        if( typeIsFunction( type ) ) {
            std::string typeString = "function.";

            const FunctionType& function = std::get< FunctionType >( type );
            if( function.udtId ) {
                typeString += *function.udtId + ".";
            }

           typeString += "(";

            for( unsigned int i = 0; i != function.arguments.size(); i++ ) {
                typeString += function.arguments[ i ].typeId;
                if( i != function.arguments.size() - 1 ) {
                    typeString += ",";
                }
            }

            typeString += ").";

            if( function.returnTypeId ) {
                typeString += *function.returnTypeId;
            }

            return typeString;
        } else {
            return unwrapTypeId( type );
        }
    }

    /**
     * "Comparable" types refer to whether or not two types are BOTH FunctionType or BOTH ValueType
     */
    bool typesComparable( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        // Function types and value types are incomparable
        if( ( typeIsFunction( lhs ) && typeIsFunction( rhs ) ) || ( typeIsValue( lhs ) && typeIsValue( rhs ) ) ) {
            return true;
        }

        return false;
    }

    bool typeIsValue( const MemoryDataType& type ) {
        return std::holds_alternative< ValueType >( type );
    }

    bool typeIsFunction( const MemoryDataType& type ) {
        return std::holds_alternative< FunctionType >( type );
    }

    bool typeIsUdt( const MemoryDataType& type ) {
        if( typeIsFunction( type ) ) {
            return false;
        }

        return !typeIdToTokenType( unwrapTypeId( type ) );
    }

    bool typeIsInteger( const MemoryDataType& type ) {
        if( typeIsFunction( type ) ) {
            return false;
        }

        std::string typeId = unwrapTypeId( type );

        return typeId == "u8" || typeId == "u16" || typeId == "u32" || typeId == "s8" || typeId == "s16" || typeId == "s32";
    }

    bool typeIsString( const MemoryDataType& type ) {
        if( typeIsFunction( type ) ) {
            return false;
        }

        return unwrapTypeId( type ) == "string";
    }

    bool typesMatch( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        if( !typesComparable( lhs, rhs ) ) {
            return false;
        }

        if( typeIsFunction( lhs ) ) {
            // Types are function
            // Function types are equivalent if they:
            // - Share a UDT id or lack thereof
            // - Share equivalent arguments
            // - Share a return type
            const FunctionType& lhsFunction = std::get< FunctionType >( lhs );
            const FunctionType& rhsFunction = std::get< FunctionType >( rhs );

            if( lhsFunction.udtId != rhsFunction.udtId ) {
                return false;
            }

            if( lhsFunction.arguments.size() != rhsFunction.arguments.size() ) {
                return false;
            }

            for( unsigned int i = 0; i != lhsFunction.arguments.size(); i++ ) {
                if( lhsFunction.arguments[ i ].typeId != rhsFunction.arguments[ i ].typeId ) {
                    return false;
                }
            }

            if( lhsFunction.returnTypeId != rhsFunction.returnTypeId ) {
                return false;
            }

            // Passed all the checks!
            return true;
        } else {
            // Types are value
            return unwrapTypeId( lhs ) == unwrapTypeId( rhs );
        }
    }

    bool integerTypesMatch( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        return typeIsInteger( lhs ) && typeIsInteger( rhs );
    }

    bool assignmentCoercible( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        if( typeIsFunction( lhs ) || typeIsFunction( rhs ) ) {
            return false;
        }

        return unwrapTypeId( lhs ) == "string" && typeIsInteger( rhs );
    }

    bool coercibleToString( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        if( typeIsFunction( lhs ) || typeIsFunction( rhs ) ) {
            return false;
        }

        return ( unwrapTypeId( lhs ) == "string" || unwrapTypeId( rhs ) == "string" ) && ( typeIsInteger( lhs ) || typeIsInteger( rhs ) );
    }

    std::optional< long > getPrimitiveTypeSize( const MemoryDataType& type ) {
        if( typeIsUdt( type ) || typeIsFunction( type ) ) {
            // Cannot report the size of a UDT or a functional type using this method
            return {};
        }

        std::string typeId = unwrapTypeId( type );

        if( typeId == "u8" || typeId == "s8" ) { return 1; }
        if( typeId == "u16" || typeId == "s16" ) { return 2; }
        if( typeId == "u32" || typeId == "s32" || typeId == "string" ) { return 4; }

        return {};
    }

    std::optional< long > getUdtTypeSize( const MemoryDataType& type, const MemoryTracker& memory ) {
        if( !typeIsUdt( type ) ) {
            return {};
        }

        std::string typeId = unwrapTypeId( type );

        auto query = memory.findUdt( typeId );
        if( !query ) {
            return {};
        }

        // Add all subtypes
        long totalSize = 0;
        for( const UdtField& field : query->fields ) {
            if( typeIsUdt( field.type ) ) {
                std::optional< long > size = getUdtTypeSize( field.type, memory );
                if( !size ) {
                    return {};
                }

                totalSize += *size;
            } else {
                std::optional< long > size = getPrimitiveTypeSize( field.type );
                if( !size ) {
                    return {};
                }

                totalSize += *size;
            }
        }

        if( !totalSize ) {
            return {};
        }

        return totalSize;
    }

    MemoryDataType promotePrimitiveTypes( const MemoryDataType& lhs, const MemoryDataType& rhs ) {
        // Cannot promote udt or function types
        if( !typeIsInteger( lhs ) || !typeIsInteger( rhs ) ) {
            Error{ "Internal compiler error (attempted to promote noninteger type)", {} }.throwException();
        }

        std::string lhsTypeId = unwrapTypeId( lhs );
        std::string rhsTypeId = unwrapTypeId( rhs );

        if( getTypeComparison( rhsTypeId ) >= getTypeComparison( lhsTypeId ) ) {
            return ValueType{ isOneSigned( lhsTypeId, rhsTypeId ) ? scrubSigned( rhsTypeId ) : rhsTypeId };
        } else {
            return ValueType{ isOneSigned( lhsTypeId, rhsTypeId ) ? scrubSigned( lhsTypeId ) : lhsTypeId };
        }
    }

    TypeResult getType( const Primary& node, MemoryTracker& memory ) {
        TypeResult result;

        std::visit(
            overloaded {
                [ &memory, &result ]( const Token& token ){
                    switch( token.type ) {
                        case TokenType::TOKEN_LITERAL_INTEGER: {
                            result = TypeResult::good( ValueType{ getLiteralType( expectLong( token ) ) } );
                            return;
                        }
                        case TokenType::TOKEN_LITERAL_STRING: {
                            result = TypeResult::good( ValueType{ "string" } );
                            return;
                        }
                        case TokenType::TOKEN_THIS: {
                            // Type of "this" token is obtainable from the pointer on the stack
                            auto thisQuery = memory.find( "this" );
                            if( !thisQuery ) {
                                Error{ "Internal compiler error (unable to determine type of \"this\" token)", token }.throwException();
                            }

                            result = TypeResult::good( MemoryTracker::unwrapValue( *thisQuery ).type );
                            return;
                        }
                        case TokenType::TOKEN_IDENTIFIER: {
                            // Look up identifier in memory
                            std::string id = expectString( token );
                            auto memoryQuery = memory.find( id );
                            if( !memoryQuery ) {
                                result = TypeResult::err( "Undefined variable: " + id );
                                return;
                            }

                            // Get its type id, and if it is a udt, attach the udt id
                            MemoryDataType type = MemoryTracker::unwrapValue( *memoryQuery ).type;
                            if( typeIsUdt( type ) ) {
                                // Udt
                                if( !memory.findUdt( unwrapTypeId( type ) ) ) {
                                    Error{ "Internal compiler error (UDT specified as ValueType but UDT not found)", token }.throwException();
                                    return;
                                }
                            }

                            result = TypeResult::good( type );
                            return;
                        }
                        default: {
                            Error{ "Internal compiler error", token }.throwException();
                        }
                    }
                },
                [ &memory, &result ]( const std::unique_ptr< Expression >& expression ){
                    result = getType( *expression, memory );
                }
            },
            node.value
        );

        return result;
    }

    TypeResult getType( const CallExpression& node, MemoryTracker& memory ) {
        // Get the return type of the expression
        TypeResult expressionType = getType( *node.identifier, memory );
        if( !expressionType ) {
            return TypeResult::err( "Could not deduce type of identifier in CallExpression: " + expressionType.getError() );
        }

        // The expression must ultimately boil down to a function
        if( !typeIsFunction( *expressionType ) ) {
            return TypeResult::err( "Unable to call non-function type " + typeToString( *expressionType ) );
        }

        // The return type of the function is the type of this CallExpression
        const FunctionType& function = std::get< FunctionType >( *expressionType );
        if( !function.returnTypeId ) {
            return TypeResult::err( "Cannot call function with no return type" );
        }

        return TypeResult::good( ValueType{ *function.returnTypeId } );
    }

    TypeResult getType( const BinaryExpression& node, MemoryTracker& memory ) {

        TypeResult lhs = getType( *node.lhsValue, memory );
        TypeResult rhs = getType( *node.rhsValue, memory );

        // If operator is dot then type of the expression is the field on the left hand side
        TokenType tokenOp;
        if( auto token = std::get_if< Token >( &node.op->value ) ) {
            tokenOp = token->type;
        } else {
            Error{ "Internal compiler error (BinaryExpression op does not contain Token variant)", {} }.throwException();
        }

        switch( tokenOp ) {
            case TokenType::TOKEN_DOT: {
                // The type of a dot operation is the field, specified on the RHS, of the UDT on the LHS.
                auto rhsIdentifier = getIdentifierName( *node.rhsValue );
                if( !rhsIdentifier ) {
                    return TypeResult::err( "Right-hand side of dot operator must contain single identifier" );
                }

                if( !lhs ) {
                    return lhs;
                }

                auto lhsUdt = memory.findUdt( unwrapTypeId( *lhs ) );
                if( !lhsUdt ) {
                    return TypeResult::err( "Undeclared user-defined type" );
                }

                auto rhsUdtField = memory.findUdtField( lhsUdt->id, *rhsIdentifier );
                if( !rhsUdtField ) {
                    return TypeResult::err( "User-defined type " + lhsUdt->id + " does not have field " + *rhsIdentifier );
                }

                return TypeResult::good( rhsUdtField->type );
            }
            default: {
                // All other operators require both sides to have a well-defined type
                if( !lhs ) { return lhs; }
                if( !rhs ) { return rhs; }

                // For all other operators, if left hand side is a UDT, then RHS must be as well.
                if( typeIsUdt( *lhs ) || typeIsFunction( *lhs ) ) {
                    if( typesMatch( *lhs, *rhs ) ) {
                        return lhs;
                    } else {
                        return TypeResult::err( "Type mismatch" );
                    }
                }

                // Otherwise the data type of the BinaryExpression is the larger of lhs, rhs
                return TypeResult::good( promotePrimitiveTypes( *lhs, *rhs ) );
            }
        }
    }

    TypeResult getType( const Expression& node, MemoryTracker& memory ) {
		if( auto binaryExpression = std::get_if< std::unique_ptr< BinaryExpression > >( &node.value ) ) {
 			return getType( **binaryExpression, memory );
		}

		if( auto primaryExpression = std::get_if< std::unique_ptr< Primary > >( &node.value ) ) {
			return getType( **primaryExpression, memory );
		}

        if( auto callExpression = std::get_if< std::unique_ptr< CallExpression > >( &node.value ) ) {
            return getType( **callExpression, memory );
        }

		// Many node types not yet implemented
        return TypeResult::err( "Expression subtype not implemented" );
    }

}
