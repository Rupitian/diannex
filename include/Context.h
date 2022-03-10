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
        int32_t localizationStringId = -1;
    };

    struct LoopContext
    {
        std::vector<int> continuePatch;
        std::vector<int> endLoopPatch;
        std::vector<Instruction::Opcode> returnCleanup;
        int localCountStackIndex;
    };

    struct CompileContext
    {
        ProjectFormat* project;
#if DIANNEX_OLD_INCLUDE_ORDER
        std::queue<std::string> queue;
#else
        std::deque<std::string> queue;
#endif
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
        std::unordered_map<std::string, int> internalStringsMap;
        std::vector<std::string> symbolStack;
        std::vector<std::string> localStack;
        std::vector<int> localCountStack;
        std::vector<LoopContext> loopStack;
        int translationStringIndex = 0;
        std::vector<TranslationInfo> translationInfo;
        bool generatingFunction = false;
        int offset = 0;

        int32_t maxStringId = -1;
        std::unordered_map<std::string, std::vector<std::pair<uint32_t, int32_t>>> stringIdPositions;

        ~CompileContext();

        int string(const std::string& str);
    };
}
	
#endif // DIANNEX_CONTEXT_H
