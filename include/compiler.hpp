#pragma once
#include "ast.hpp"
#include "result_type.hpp"
#include "symbol.hpp"
#include <string>
#include <set>

namespace GoldScorpion {

    struct CompilerSettings {
        std::set< std::string >& activeFiles;
        SymbolResolver& symbols;
        bool printLex = false;
        bool printAst = false;
    };

    Result< Program, std::string > fileToTree( const std::string& path, CompilerSettings settings );

    Result< Program, std::string > fileToProgram( const std::string& path, CompilerSettings settings );

    int compile( const std::string& parseFilename, bool printLex, bool printAst );

}