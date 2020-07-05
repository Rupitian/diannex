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
        bool compressed = false, // TODO retrieve values from project options,
             internalTranslationFile = true; // and implement cases other than these in this function
        bw->WriteUInt8((uint8_t)compressed | ((uint8_t)internalTranslationFile << 1));

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
        bmw.WriteUInt32(ctx->translationInfo.size());
        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
            bmw.WriteString(it->text);

        uint32_t size = bmw.GetSize();
        if (compressed)
        {
            std::vector<uint8_t> out;
            uint32_t compSize = Compress(bmw.GetBuffer(), size, out);
            if (compSize == 0)
                return false;
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