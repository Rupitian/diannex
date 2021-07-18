#ifndef DIANNEX_LEXER_H
#define DIANNEX_LEXER_H

#include <cstdint>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>

#include "Project.h"
#include "Utility.h"
#include "Context.h"
#include "Token.h"

namespace diannex
{
    // Stringifies a token. Can return nullptr if it is a certain Error-type token.
    const char* tokenToString(Token t);

    class Lexer
    {
    public:
        static void LexString(const std::string& in, CompileContext* ctx, std::vector<Token>& out, uint32_t startLine = 1, uint16_t startColumn = 1, std::unordered_set<std::string>* macros = nullptr);
    private:
        Lexer();
    };
}

#endif // DIANNEX_LEXER_H