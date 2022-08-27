#ifndef DIANNEX_BINARY_H
#define DIANNEX_BINARY_H

#include "Lexer.h" // Hack to fix circular references to Token
#include "Context.h"
#include "BinaryWriter.h"

#define DIANNEX_BINARY_VERSION 4
#define DIANNEX_BINARY_TRANSLATION_VERSION 0

namespace diannex
{
    class Binary
    {
    public:
        static uint32_t Compress(const char* srcBuff, uint32_t srcSize, std::vector<uint8_t>& out);
        static bool Write(BinaryWriter* bw, CompileContext* ctx);
        static bool WriteTranslationText(BinaryWriter* bw, const std::vector<std::string>& text);
    private:
        Binary();
    };
}

#endif // DIANNEX_BINARY_H