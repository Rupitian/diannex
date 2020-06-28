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

    static int patchInstruction(Instruction::Opcode opcode, CompileContext* ctx)
    {
        ctx->bytecode.emplace_back(opcode);
        return ctx->bytecode.size() - 1;
    }

    static void patch(int ind, CompileContext* ctx)
    {
        ctx->bytecode.at(ind).arg = ctx->bytecode.size() - ind;
    }

    static int string(std::string str, CompileContext* ctx)
    {
        int res = std::find(ctx->internalStrings.begin(), ctx->internalStrings.end(), str) - ctx->internalStrings.begin();
        if (res == ctx->internalStrings.size())
            ctx->internalStrings.push_back(str);
        return res;
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
            case Node::NodeType::Namespace:
                ctx->symbolStack.push_back(((NodeContent*)n)->content);
                GenerateBlock(n, ctx, res);
                ctx->symbolStack.pop_back();
                break;
            case Node::NodeType::Scene:
            {
                ctx->symbolStack.push_back(((NodeContent*)n)->content);
                const std::string& symbol = expandSymbol(ctx);
                if (ctx->sceneBytecode.count(symbol))
                    res->errors.push_back({ BytecodeError::ErrorType::SceneAlreadyExists, 0, 0, symbol.c_str() });
                int pos = ctx->bytecode.size();
                GenerateSceneBlock(n, ctx, res);
                ctx->sceneBytecode.insert(std::make_pair(symbol, (pos == ctx->bytecode.size()) ? -1 : pos));
                ctx->symbolStack.pop_back();
                break;
            }
            case Node::NodeType::Function:
            {
                ctx->symbolStack.push_back(((NodeContent*)n)->content);
                const std::string& symbol = expandSymbol(ctx);
                if (ctx->functionBytecode.count(symbol))
                    res->errors.push_back({ BytecodeError::ErrorType::FunctionAlreadyExists, 0, 0, symbol.c_str() });
                //GenerateFunctionBlock(symbol, n, ctx, res);
                // todo
                ctx->symbolStack.pop_back();
                break;
            }
            case Node::NodeType::Definitions:
            {
                ctx->symbolStack.push_back(((NodeContent*)n)->content);
                const std::string& symbol = expandSymbol(ctx);
                if (!ctx->definitions.insert(symbol).second)
                    res->errors.push_back({ BytecodeError::ErrorType::DefinitionBlockAlreadyExists, 0, 0, symbol.c_str() });

                // Iterate over all of the definitions, and generate proper info
                for (Node* subNode : n->nodes)
                {
                    if (subNode->type == Node::NodeType::MarkedComment)
                    {
                        translationInfo(ctx, ((NodeContent*)subNode)->content, true);
                    }
                    else
                    {
                        int pos = ctx->bytecode.size();
                        NodeDefinition* def = (NodeDefinition*)subNode;
                        for (auto it = def->nodes.rbegin(); it != def->nodes.rend(); ++it)
                            GenerateExpression(*it, ctx, res);
                        bool hasExpr;
                        if (pos != ctx->bytecode.size())
                        {
                            ctx->bytecode.emplace_back(Instruction::Opcode::exit);
                            hasExpr = true;
                        }
                        else
                            hasExpr = false;
                        if (def->excludeValueTranslation)
                        {
                            ctx->definitionBytecode.insert(std::make_pair(symbol + '.' + def->key, std::make_pair(def->value, hasExpr ? pos : -1)));
                        }
                        else
                        {
                            ctx->definitionBytecode.insert(std::make_pair(symbol + '.' + def->key, std::make_pair(std::nullopt, hasExpr ? pos : -1)));
                            translationInfo(ctx, def->value);
                        }
                    }
                }

                ctx->symbolStack.pop_back();
                break;
            }
            }
        }
    }

    void Bytecode::GenerateSceneBlock(Node* block, CompileContext* ctx, BytecodeResult* res)
    {
        for (Node* n : block->nodes)
        {
            switch (n->type)
            {
            case Node::NodeType::SceneBlock:
                GenerateSceneBlock(n, ctx, res);
                break;
            }
        }
    }

    void Bytecode::GenerateExpression(Node* expr, CompileContext* ctx, BytecodeResult* res)
    {
        int i = 0; // Index within subnodes, which needs to be tracked for the end

        switch (expr->type)
        {
        case Node::NodeType::ExprTernary:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            int patch1 = patchInstruction(Instruction::Opcode::jf, ctx);
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            int patch2 = patchInstruction(Instruction::Opcode::j, ctx);
            patch(patch1, ctx);
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            patch(patch2, ctx);
            break;
        }
        case Node::NodeType::ExprBinary:
        {
            break;
        }
        case Node::NodeType::ExprConstant:
        {
            NodeToken* constant = (NodeToken*)expr;
            switch (constant->token.type)
            {
            case TokenType::Number:
                if (constant->token.content.find('.') == std::string::npos)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, std::stoi(constant->token.content)));
                else
                    ctx->bytecode.push_back(Instruction::make_double(Instruction::Opcode::pushd, std::stod(constant->token.content)));
                break;
            case TokenType::Percentage:
                if (constant->token.content.find('.') == std::string::npos)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushd, std::stoi(constant->token.content) / 100.0));
                else
                    ctx->bytecode.push_back(Instruction::make_double(Instruction::Opcode::pushd, std::stod(constant->token.content) / 100.0));
                break;
            case TokenType::String: // todo: add default setting to project file?
            case TokenType::ExcludeString:
                if (constant->nodes.size() == 0)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushbs, string(constant->token.content, ctx)));
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushbints, string(constant->token.content, ctx), constant->nodes.size()));
                }
                break;
            case TokenType::MarkedString:
                if (constant->nodes.size() == 0)
                {
                    ctx->bytecode.emplace_back(Instruction::Opcode::pushs);
                    translationInfo(ctx, constant->token.content);
                }
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushbints, constant->nodes.size()));
                    translationInfo(ctx, constant->token.content);
                }
                break;
            }
            break;
        }
        case Node::NodeType::ExprNot:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(Instruction::Opcode::inv);
            break;
        }
        case Node::NodeType::ExprNegate:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(Instruction::Opcode::neg);
            break;
        }
        case Node::NodeType::ExprBitwiseNegate:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(Instruction::Opcode::bitneg);
            break;
        }
        case Node::NodeType::ExprArray:
        {
            for (int j = 0; j < expr->nodes.size(); j++)
                GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::makearr, expr->nodes.size()));
            break;
        }
        case Node::NodeType::ExprPreIncrement:
        {
            break;
        }
        case Node::NodeType::ExprPostIncrement:
        {
            break;
        }
        case Node::NodeType::ExprPreDecrement:
        {
            break;
        }
        case Node::NodeType::ExprPostDecrement:
        {
            break;
        }
        case Node::NodeType::ExprAccessArray:
        {
            break;
        }
        case Node::NodeType::SceneFunction:
        {
            break;
        }
        }

        // TODO array accesses here
    }
}