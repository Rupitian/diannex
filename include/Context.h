#ifndef DIANNEX_CONTEXT_H
#define DIANNEX_CONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <variant>
#include <algorithm>

#include "Instruction.h"
#include "Token.h"
#include "Project.h"
#include "ParseResult.h"

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
        std::unordered_set<std::string> files;
        std::vector<std::pair<std::string, std::vector<Token>>> tokenList;
        std::vector<std::pair<std::string, ParseResult*>> parseList;
        std::unordered_map<std::string, std::vector<int>> sceneBytecode;
        std::unordered_map<std::string, std::vector<int>> functionBytecode;
        std::unordered_set<std::string> definitions;
        std::unordered_map<std::string, std::pair<std::variant<int, std::string>, int>> definitionBytecode;
        std::vector<Instruction> bytecode;
        std::vector<std::string> internalStrings;
        std::vector<std::string> symbolStack;
        std::vector<std::string> localStack;
        std::vector<int> localCountStack;
        std::vector<LoopContext> loopStack;
        int translationStringIndex = 0;
        std::vector<TranslationInfo> translationInfo;
        bool generatingFunction = false;

        ~CompileContext();

        int string(std::string str);
    };
}
	
#endif // DIANNEX_CONTEXT_H
