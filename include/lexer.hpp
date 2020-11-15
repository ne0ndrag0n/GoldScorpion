#pragma once
#include "token.hpp"
#include "result_type.hpp"
#include <queue>

namespace GoldScorpion {

	Result< std::queue< Token > > getTokens( const std::string& body );

}