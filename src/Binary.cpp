#include "Binary.h"

#include <algorithm>
#include <random>
#include <libs/miniz/miniz.h>
#include <set>

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

    static int instructionOffset(int index, CompileContext* ctx)
    {
        if (index == -1)
            return -1;
        return ctx->bytecode.at(index).offset;
    }

    bool Binary::Write(BinaryWriter* bw, CompileContext* ctx)
    {
        bw->WriteBytes("DNX", 3);
        bw->WriteUInt8(4); // Version

        // Flags
        bool compressed = ctx->project->options.compression,
             internalTranslationFile = !ctx->project->options.translationPublic;
        bw->WriteUInt8((uint8_t)compressed | ((uint8_t)internalTranslationFile << 1));

        BinaryMemoryWriter bmw;

        // Scene metadata
        uint32_t begin = bmw.GetSize();
        bmw.WriteUInt32(0);
        bmw.WriteUInt32(ctx->sceneBytecode.size());
        for (auto it = ctx->sceneBytecode.begin(); it != ctx->sceneBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            // Bytecode indices (scene bytecode and flag expressions)
            int size = it->second.size();
            bmw.WriteUInt16(size);
            for (int i = 0; i < size; i++)
                bmw.WriteInt32(instructionOffset(it->second.at(i), ctx));
        }
        bmw.SizePatch(begin);

        // Function metadata
        begin = bmw.GetSize();
        bmw.WriteUInt32(0);
        bmw.WriteUInt32(ctx->functionBytecode.size());
        for (auto it = ctx->functionBytecode.begin(); it != ctx->functionBytecode.end(); ++it)
        {
            // Symbol
            bmw.WriteUInt32(ctx->string(it->first));

            // Bytecode indices (function bytecode and flag expressions)
            int size = it->second.size();
            bmw.WriteUInt16(size);
            for (int i = 0; i < size; i++)
                bmw.WriteInt32(instructionOffset(it->second.at(i), ctx));
        }
        bmw.SizePatch(begin);

        // Definition metadata
        begin = bmw.GetSize();
        bmw.WriteUInt32(0);
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
            bmw.WriteInt32(instructionOffset(p.second, ctx));
        }
        bmw.SizePatch(begin);

        std::set<int> externalFunctions{};
        int externalFunctionIndex = 0;

        // Bytecode
        bmw.WriteUInt32(ctx->offset);
        for (auto it = ctx->bytecode.begin(); it != ctx->bytecode.end(); ++it)
        {
            if (it->opcode == Instruction::Opcode::PATCH_CALL)
            {
                std::string funcName = it->vec->at(0);

                // Start with local levels, go to higher levels
                for (int i = it->vec->size() - 1; i >= 1; --i)
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

                // Check top-level context last
                if (it->opcode != Instruction::Opcode::call)
                {
                    delete it->vec;

                    if (auto func = ctx->functionBytecode.find(funcName); func != ctx->functionBytecode.end())
                    {
                        // This is in the top level
                        it->opcode = Instruction::Opcode::call;
                        int32_t temp = it->count;
                        it->arg = std::distance(ctx->functionBytecode.begin(), func);
                        it->arg2 = temp;
                    }
                    else
                    {
                        // Unable to find at any level, so must be externally-defined
                        it->opcode = Instruction::Opcode::callext;
                        int32_t temp = it->count;
                        int str = ctx->string(funcName);
                        externalFunctions.insert(str);
                        it->arg = str;
                        it->arg2 = temp;
                    }
                }
            }
            it->Serialize(&bmw);
        }

        // Internal string table
        begin = bmw.GetSize();
        bmw.WriteUInt32(0);
        bmw.WriteUInt32(ctx->internalStrings.size());
        for (auto it = ctx->internalStrings.begin(); it != ctx->internalStrings.end(); ++it)
            bmw.WriteString(*it);
        bmw.SizePatch(begin);

        // Internal translation file (if applicable)
        if (internalTranslationFile)
        {
            uint32_t count = 0;
            for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
                if (!it->isComment)
                    count++;

            begin = bmw.GetSize();
            bmw.WriteUInt32(0);
            bmw.WriteUInt32(count);
            for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
                if (!it->isComment)
                    bmw.WriteString(it->text);
            bmw.SizePatch(begin);
        }

        // External function list
        begin = bmw.GetSize();
        bmw.WriteUInt32(0);
        bmw.WriteUInt32(externalFunctions.size());
        for (auto it = externalFunctions.begin(); it != externalFunctions.end(); ++it)
            bmw.WriteUInt32(*it);
        bmw.SizePatch(begin);

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