#ifndef DIANNEX_BYTECODE_H
#define DIANNEX_BYTECODE_H

#include "Parser.h"
#include "Context.h"
#include "Instruction.h"

namespace diannex
{

    class Bytecode
    {
    public:
        static void Generate(const ParseResult& in, CompileContext& ctx, std::vector<Instruction>& out);
    private:
        Bytecode();
    };
}

#endif // DIANNEX_BYTECODE_H