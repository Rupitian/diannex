#ifndef DIANNEX_CONTEXT_H
#define DIANNEX_CONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include <unordered_set>

#include "Instruction.h"

namespace diannex
{
    struct TranslationInfo
    {
        std::string key;
        bool isComment;
        std::string text;
    };

    struct CompileContext
    {
        ProjectFormat* project;
        std::queue<std::string> queue;
        std::string currentFile;
        std::unordered_map<std::string, std::vector<struct Token>> tokenList;
        std::unordered_map<std::string, struct ParseResult*> parseList;
        std::unordered_map<std::string, std::vector<Instruction>> sceneBytecode;
        std::unordered_map<std::string, std::vector<Instruction>> functionBytecode;
        std::unordered_set<std::string> definitions;
        std::unordered_map<std::string, std::pair<std::string, std::vector<Instruction>>> definitionBytecode;
        std::vector<std::string> symbolStack;
        std::vector<TranslationInfo> translationInfo;

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