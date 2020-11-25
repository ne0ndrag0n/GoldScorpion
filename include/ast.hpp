#pragma once
#include "token.hpp"
#include "variant_visitor.hpp"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <utility>

namespace GoldScorpion {

	template< typename ReturnType >
	struct GeneratedAstNode {
		std::vector< Token >::iterator nextIterator;
		std::unique_ptr< ReturnType > node;
	};

	template< typename ReturnType >
	using AstResult = std::optional< GeneratedAstNode< ReturnType > >;

	struct Primary {
		std::variant< Token, std::unique_ptr< struct Expression > > value;
	};

	struct CallExpression {
		std::unique_ptr< Expression > identifier;
		std::vector< std::unique_ptr< struct Expression > > arguments;
	};

	struct UnaryExpression {
		std::unique_ptr< Primary > op;
		std::unique_ptr< struct Expression > value;
	};

	struct BinaryExpression {
		std::unique_ptr< struct Expression > lhsValue;
		std::unique_ptr< Primary > op;
		std::unique_ptr< struct Expression > rhsValue;
	};

	struct AssignmentExpression {
		std::unique_ptr< Primary > identifier;
		std::unique_ptr< struct Expression > expression;
	};

	struct Expression {
		std::variant<
			std::unique_ptr< AssignmentExpression >,
			std::unique_ptr< BinaryExpression >,
			std::unique_ptr< UnaryExpression >,
			std::unique_ptr< CallExpression >,
			std::unique_ptr< Primary >
		> value;
	};

	struct Statement {
		std::unique_ptr< Expression > value;
	};

	struct Program {
		std::vector< std::unique_ptr< Statement > > statements;
	};
}