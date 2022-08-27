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

        static void ConvertPrivateToPublic(std::ifstream& in, std::ofstream& out);
        static void ConvertPublicToPrivate(std::ifstream& in, std::ifstream& inMatch, std::ofstream& out);

        static void UpgradeFileToNewer(std::ifstream& in, bool isInputPrivate, std::ifstream& inNewer, std::ofstream& out);

        static std::string SanitizeString(const std::string& str);
    private:
        Translation();
    };
}

#endif // DIANNEX_TRANSLATION_H