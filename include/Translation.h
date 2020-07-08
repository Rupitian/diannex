#ifndef DIANNEX_TRANSLATION_H
#define DIANNEX_TRANSLATION_H

#include "Parser.h"
#include "Context.h"
#include "Instruction.h"

namespace diannex
{
    class Translation
    {
    public:
        static bool GeneratePublicFile(CompileContext* ctx);
    private:
        Translation();
    };
}

#endif // DIANNEX_TRANSLATION_H