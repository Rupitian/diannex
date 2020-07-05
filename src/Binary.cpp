#include "Binary.h"

namespace diannex
{
    void Binary::Write(BinaryWriter* bw, CompileContext* ctx)
    {
        bw->WriteBytes("DNX", 3);
        bw->WriteUInt8(0); // Version

        // Flags
        bool compressed = false, // TODO retrieve values from project options,
             shuffle = false,    // and implement cases other than these in this function
             internalTranslationFile = true;
        bw->WriteUInt8((uint8_t)compressed | ((uint8_t)shuffle << 1) | ((uint8_t)internalTranslationFile << 2)); // Version

        BinaryMemoryWriter bmw;

        // Scene metadata
        bmw.WriteUInt32(ctx->sceneBytecode.size());
        for (auto it = ctx->sceneBytecode.begin(); it != ctx->sceneBytecode.end(); ++it)
        {
            bmw.WriteUInt32(ctx->string(it->first));
            bmw.WriteUInt32(it->second);
        }

        // Function metadata
        bmw.WriteUInt32(ctx->functionBytecode.size());
        for (auto it = ctx->functionBytecode.begin(); it != ctx->functionBytecode.end(); ++it)
        {
            bmw.WriteUInt32(ctx->string(it->first));
            bmw.WriteUInt32(it->second);
        }

        // Definition metadata
        bmw.WriteUInt32(ctx->definitionBytecode.size());
        for (auto it = ctx->definitionBytecode.begin(); it != ctx->definitionBytecode.end(); ++it)
        {
            bmw.WriteUInt32(ctx->string(it->first));
            auto p = it->second;
            if (p.first.index() == 0)
                bmw.WriteUInt32(std::get<int>(p.first));
            else
                bmw.WriteUInt32(ctx->string(std::get<std::string>(p.first)) | (1 << 31));
            bmw.WriteUInt32(p.second);
        }

        // Bytecode
        bmw.WriteList(ctx->bytecode);

        // Internal string table
        bmw.WriteUInt32(ctx->internalStrings.size());
        for (auto it = ctx->internalStrings.begin(); it != ctx->internalStrings.end(); ++it)
            bmw.WriteString(*it);

        // Internal translation file (if applicable)
        bmw.WriteUInt32(ctx->translationInfo.size());
        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
            bmw.WriteString(it->text);

        uint32_t size = bmw.GetSize();
        bw->WriteUInt32(size);
        bw->WriteBytes(bmw.GetBuffer(), size);
    }
}