#ifndef DIANNEX_TRANSLATION_H
#define DIANNEX_TRANSLATION_H

#include "Parser.h"
#include "Context.h"
#include "Instruction.h"

#include <fstream>

namespace diannex
{
    class Translation
    {
    public:
        static bool GeneratePublicFile(std::ofstream& s, CompileContext* ctx);
    private:
        Translation();
    };
}

#endif // DIANNEX_TRANSLATION_H