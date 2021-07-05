#ifndef DIANNEX_BYTECODE_H
#define DIANNEX_BYTECODE_H

#include "Parser.h"
#include "Context.h"
#include "Instruction.h"

namespace diannex
{
    struct BytecodeError
    {
        enum ErrorType
        {
            SceneAlreadyExists,
            FunctionAlreadyExists,
            DefinitionBlockAlreadyExists,
            LocalVariableAlreadyExists,
            ContinueOutsideOfLoop,
            BreakOutsideOfLoop,
            StatementsBeforeSwitchCase,

            UnexpectedError
        };

        ErrorType type;
        uint32_t line;
        uint16_t column;
        std::string info1;
    };

    struct BytecodeResult
    {
        std::vector<BytecodeError> errors;
    };
    
    class Bytecode
    {
    public:
        static BytecodeResult* Generate(ParseResult* parsed, CompileContext* ctx);
        static void GenerateBlock(Node* block, CompileContext* ctx, BytecodeResult* res);
        static void GenerateSceneBlock(Node* block, CompileContext* ctx, BytecodeResult* res);
        static void GenerateSceneStatement(Node* statement, CompileContext* ctx, BytecodeResult* res);
        static void GenerateBasicAssign(NodeContent* variable, CompileContext* ctx, BytecodeResult* res);
        
        static void GenerateExpression(Node* expr, CompileContext* ctx, BytecodeResult* res);
    private:
        Bytecode();
    };
}

#endif // DIANNEX_BYTECODE_H