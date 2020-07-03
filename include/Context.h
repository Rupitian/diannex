#ifndef DIANNEX_CONTEXT_H
#define DIANNEX_CONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <variant>

#include "Instruction.h"

namespace diannex
{
    struct TranslationInfo
    {
        std::string key;
        bool isComment;
        std::string text;
    };

    struct LoopContext
    {
        std::vector<int> continuePatch;
        std::vector<int> endLoopPatch;
        std::vector<Instruction> returnCleanup;
    };

    struct CompileContext
    {
        ProjectFormat* project;
        std::queue<std::string> queue;
        std::string currentFile;
        std::unordered_map<std::string, std::vector<struct Token>> tokenList;
        std::unordered_map<std::string, struct ParseResult*> parseList;
        std::unordered_map<std::string, int> sceneBytecode;
        std::unordered_map<std::string, int> functionBytecode;
        std::unordered_set<std::string> definitions;
        std::unordered_map<std::string, std::pair<std::variant<int, std::string>, int>> definitionBytecode;
        std::vector<Instruction> bytecode;
        std::vector<std::string> internalStrings;
        std::vector<std::string> symbolStrings;
        std::vector<std::string> symbolStack;
        std::vector<std::string> localStack;
        std::vector<int> localCountStack;
        std::vector<LoopContext> loopStack;
        int translationStringIndex = 0;
        std::vector<TranslationInfo> translationInfo;
        bool generatingFunction = false;

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