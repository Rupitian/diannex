#include "Bytecode.h"

#include <sstream>

namespace diannex
{
    static std::string expandSymbol(CompileContext* ctx)
    {
        std::stringstream ss(std::ios_base::app | std::ios_base::out);
        for (int i = 0; i < ctx->symbolStack.size(); i++)
        {
            ss << ctx->symbolStack.at(i);
            if (i + 1 != ctx->symbolStack.size())
                ss << '.';
        }
        return ss.str();
    }

    static void translationInfo(CompileContext* ctx, std::string text, bool isComment = false)
    {
        if (ctx->project->options.translationOutput.empty())
        {
            if (!isComment)
            {
                ctx->translationInfo.push_back({ "", false, text });
            }
        } else if (ctx->project->options.translationPrivate)
            ctx->translationInfo.push_back({ expandSymbol(ctx), isComment, text });
        else if (!isComment)
            ctx->translationInfo.push_back({ "", false, text });
    }

    BytecodeResult* Bytecode::Generate(ParseResult* parsed, CompileContext* ctx)
    {
        BytecodeResult* res = new BytecodeResult;
        GenerateBlock(parsed->baseNode, ctx, res);
        return res;
    }

    void Bytecode::GenerateBlock(Node* block, CompileContext* ctx, BytecodeResult* res)
    {
        for (Node* n : block->nodes)
        {
            switch (n->type)
            {
            case Node::NodeType::MarkedComment:
                translationInfo(ctx, ((NodeContent*)n)->content, true);
                break;
            }
        }
    }
}