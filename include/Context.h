#ifndef DIANNEX_CONTEXT_H
#define DIANNEX_CONTEXT_H

#include <queue>
#include <string>

#include "Instruction.h"

namespace diannex
{
    struct CompileContext
    {
        ProjectFormat* project;
        std::queue<std::string> queue;
        std::string currentFile;
        std::unordered_map<std::string, std::vector<struct Token>> tokenList;
        std::unordered_map<std::string, struct ParseResult*> parseList;
        std::unordered_map<std::string, std::vector<Instruction>> bytecodeList;

        ~CompileContext()
        {
            for (auto it = parseList.begin(); it != parseList.end(); it++)
            {
                delete it->second;
            }
        }
    };
}
	
#endif // DIANNEX_CONTEXT_H