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
        bw->WriteUInt8(2); // Version

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

            // Bytecode indices (scene bytecode and flag expressions)
            int size = it->second.size();
            bmw.WriteUInt16(size);
            for (int i = 0; i < size; i++)
                bmw.WriteInt32(it->second.at(i));
        }

        // Function metadata
        bmw.WriteUInt32(ctx->functionBytecode.size());
        for (auto it = ctx->functionBytecode.begin(); it != ctx->functionBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            // Bytecode indices (function bytecode and flag expressions)
            int size = it->second.size();
            bmw.WriteUInt16(size);
            for (int i = 0; i < size; i++)
                bmw.WriteInt32(it->second.at(i));
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
                std::string funcName = it->vec->at(0);

                // Check top-level context first
                if (auto func = ctx->functionBytecode.find(funcName); func != ctx->functionBytecode.end())
                {
                    delete it->vec;

                    it->opcode = Instruction::Opcode::call;
                    int32_t temp = it->count;
                    it->arg = std::distance(ctx->functionBytecode.begin(), func);
                    it->arg2 = temp;
                }
                else
                {
                    // Check all other levels
                    for (int i = 1; i < it->vec->size(); i++)
                    {
                        auto func = ctx->functionBytecode.find(it->vec->at(i) + "." + funcName);
                        if (func != ctx->functionBytecode.end())
                        {
                            delete it->vec;

                            it->opcode = Instruction::Opcode::call;
                            int32_t temp = it->count;
                            it->arg = std::distance(ctx->functionBytecode.begin(), func);
                            it->arg2 = temp;
                            break;
                        }
                    }

                    if (it->opcode != Instruction::Opcode::call)
                    {
                        delete it->vec;

                        // Unable to find, so must be external
                        it->opcode = Instruction::Opcode::callext;
                        int32_t temp = it->count;
                        it->arg = ctx->string(funcName);
                        it->arg2 = temp;
                    }
                }
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
            uint32_t count = 0;
            for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
                if (!it->isComment)
                    count++;

            bmw.WriteUInt32(count);
            for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
                if (!it->isComment)
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