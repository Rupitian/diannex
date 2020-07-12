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
        static void GeneratePublicFile(std::ofstream& s, CompileContext* ctx);
        static void GeneratePrivateFile(std::ofstream& s, CompileContext* ctx);
        static std::string SanitizeString(const std::string& str);
    private:
        Translation();
    };
}

#endif // DIANNEX_TRANSLATION_H