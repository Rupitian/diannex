#ifndef DIANNEX_PARSERESULT_H
#define DIANNEX_PARSERESULT_H

#include <vector>

namespace diannex
{
    struct ParseResult
    {
        class Node* baseNode;
        std::vector<struct ParseError> errors;
        bool doDelete = true;

        ~ParseResult();

        ParseResult(const ParseResult&) = delete;
    };
}

#endif