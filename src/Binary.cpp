#include "Binary.h"

#include <algorithm>
#include <random>
#include <libs/miniz/miniz.h>

namespace diannex
{
    uint32_t Binary::Compress(const char* srcBuff, uint32_t srcSize, std::vector<uint8_t>& out)
    {
        uLong outSize = compressBound(srcSize);
        out.resize(outSize);
        if (compress(&out[0], &outSize, (const unsigned char*)srcBuff, srcSize) != Z_OK)
            return 0;
        return outSize;
    }

    bool Binary::Write(BinaryWriter* bw, CompileContext* ctx)
    {
        bw->WriteBytes("DNX", 3);
        bw->WriteUInt8(0); // Version

        // Flags
        bool compressed = ctx->project->options.compression,
             internalTranslationFile = !ctx->project->options.translationPublic;
        bw->WriteUInt8((uint8_t)compressed | ((uint8_t)internalTranslationFile << 1));

        BinaryMemoryWriter bmw;

        // Scene metadata
        bmw.WriteUInt32(ctx->sceneBytecode.size());
        for (auto it = ctx->sceneBytecode.begin(); it != ctx->sceneBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            // Bytecode index
            bmw.WriteInt32(it->second);
        }

        // Function metadata
        bmw.WriteUInt32(ctx->functionBytecode.size());
        for (auto it = ctx->functionBytecode.begin(); it != ctx->functionBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            // Bytecode index
            bmw.WriteInt32(it->second);
        }

        // Definition metadata
        bmw.WriteUInt32(ctx->definitionBytecode.size());
        for (auto it = ctx->definitionBytecode.begin(); it != ctx->definitionBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            auto p = it->second;

            // String reference
            if (std::holds_alternative<int>(p.first))
                bmw.WriteUInt32(std::get<int>(p.first));
            else
                bmw.WriteUInt32(ctx->string(std::get<std::string>(p.first)) | (1 << 31));

            // Bytecode index
            bmw.WriteInt32(p.second);
        }

        // Bytecode
        bmw.WriteUInt32(ctx->bytecode.size());
        for (auto it = ctx->bytecode.begin(); it != ctx->bytecode.end(); ++it)
        {
            if (it->opcode == Instruction::Opcode::PATCH_CALL)
            {
                auto func = ctx->functionBytecode.find(ctx->internalStrings.at(it->arg));
                if (func != ctx->functionBytecode.end())
                {
                    it->opcode = Instruction::Opcode::call;
                    it->arg = std::distance(ctx->functionBytecode.begin(), func);
                }
                else
                    it->opcode = Instruction::Opcode::callext;
            }
            it->Serialize(&bmw);
        }

        // Internal string table
        bmw.WriteUInt32(ctx->internalStrings.size());
        for (auto it = ctx->internalStrings.begin(); it != ctx->internalStrings.end(); ++it)
            bmw.WriteString(*it);

        // Internal translation file (if applicable)
        if (internalTranslationFile)
        {
            bmw.WriteUInt32(ctx->translationInfo.size());
            for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
                bmw.WriteString(it->text);
        }

        uint32_t size = bmw.GetSize();
        if (compressed)
        {
            std::vector<uint8_t> out;
            uint32_t compSize = Compress(bmw.GetBuffer(), size, out);
            if (compSize == 0)
                return false;
            bw->WriteUInt32(size);
            bw->WriteUInt32(compSize);
            bw->WriteBytes((const char*)&out[0], compSize);
        }
        else
        {
            bw->WriteUInt32(size);
            bw->WriteBytes(bmw.GetBuffer(), size);
        }

        return true;
    }
}