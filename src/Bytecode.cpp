#include "Bytecode.h"

#include <sstream>

namespace diannex
{
    static std::string expandSymbol(CompileContext* ctx, int offsetSize = 0)
    {
        std::stringstream ss(std::ios_base::app | std::ios_base::out);
        int size = ctx->symbolStack.size() - offsetSize;
        for (int i = 0; i < size; i++)
        {
            ss << ctx->symbolStack.at(i);
            if (i + 1 != size)
                ss << '.';
        }
        return ss.str();
    }

    static int translationInfo(CompileContext* ctx, std::string text, StringData* stringData, bool isComment = false)
    {
        int res = ctx->translationStringIndex;
        if (ctx->project->options.translationPrivate && !ctx->project->options.translationPrivateOutDir.empty())
        {
            if (!isComment)
            {
                ctx->translationInfo.push_back({ expandSymbol(ctx), false, text, stringData ? stringData->localizedStringId : -1});
                ctx->translationStringIndex++;
            } 
            else
                ctx->translationInfo.push_back({ expandSymbol(ctx), true, text, -1 });
        }
        else if (!isComment)
        {
            ctx->translationInfo.push_back({ "", false, text, stringData ? stringData->localizedStringId : -1 });
            ctx->translationStringIndex++;
        }

        if (!isComment && ctx->project->options.addStringIds && stringData)
        {
            // Add string info when necessary
            if (stringData->localizedStringId == -1)
            {
                auto& vec = ctx->stringIdPositions[ctx->currentFile];
                auto& pair = std::make_pair(stringData->endOfStringPos, ++ctx->maxStringId);
                vec.insert(std::upper_bound(vec.begin(), vec.end(), pair), pair);
            }
        }

        return res;
    }

    static int patchInstruction(Instruction::Opcode opcode, CompileContext* ctx)
    {
        ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, opcode, 0));
        return ctx->bytecode.size() - 1;
    }

    static void patch(int ind, CompileContext* ctx)
    {
        Instruction& instr = ctx->bytecode.at(ind);
        instr.arg = ctx->offset - (instr.offset + 5 /* relative to the end of the instruction */);
    }

    static void patch(int ind, int targ, CompileContext* ctx)
    {
        Instruction& instr = ctx->bytecode.at(ind);
        if (targ == ctx->bytecode.size())
        {
            instr.arg = ctx->offset - (instr.offset + 5 /* relative to the end of the instruction */);
            return;
        }
        Instruction& targInstr = ctx->bytecode.at(targ);
        instr.arg = targInstr.offset - (instr.offset + 5 /* relative to the end of the instruction */);
    }

    static void pushLocalContext(CompileContext* ctx)
    {
        ctx->localCountStack.push_back(0);
    }

    static void popLocalContext(CompileContext* ctx)
    {
        int c = ctx->localCountStack.back();
        ctx->localCountStack.pop_back();
        for (int i = 0; i < c; i++)
        {
            ctx->localStack.pop_back();
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::freeloc, ctx->localStack.size()));
        }
    }

    static void popLocalContextForLoop(CompileContext* ctx, LoopContext& loop)
    {
        int id = ctx->localStack.size() - 1;
        for (int j = ctx->localCountStack.size() - 1; j >= loop.localCountStackIndex; j--)
        {
            int c = ctx->localCountStack.at(j);
            for (int i = 0; i < c; i++)
            {
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::freeloc, id));
                id--;
            }
        }
    }

    static void pushLoopContext(CompileContext* ctx, std::vector<Instruction::Opcode> cleanup = {})
    {
        ctx->loopStack.push_back({ std::vector<int>(), std::vector<int>(), cleanup, (int)ctx->localCountStack.size() - 1 });
    }

    static void popLoopContext(int continueInd, CompileContext* ctx, BytecodeResult* res = nullptr)
    {
        {
            if (continueInd == -1 && ctx->loopStack.back().continuePatch.size() != 0)
            {
                if (res != nullptr)
                    res->errors.push_back({ BytecodeError::ErrorType::ContinueOutsideOfLoop, 0, 0 });
            }
            else
            {
                auto& vec = ctx->loopStack.back().continuePatch;
                for (auto it = vec.begin(); it != vec.end(); ++it)
                    patch(*it, continueInd, ctx);
            }
        }
        {
            auto& vec = ctx->loopStack.back().endLoopPatch;
            for (auto it = vec.begin(); it != vec.end(); ++it)
                patch(*it, ctx);
        }
        ctx->loopStack.pop_back();
    }

    static void popLoopContextContinue(int continueInd, CompileContext* ctx, BytecodeResult* res = nullptr)
    {
        if (continueInd == -1 && ctx->loopStack.back().continuePatch.size() != 0)
        {
            if (res != nullptr)
                res->errors.push_back({ BytecodeError::ErrorType::ContinueOutsideOfLoop, 0, 0 });
        }
        else
        {
            auto& vec = ctx->loopStack.back().continuePatch;
            for (auto it = vec.begin(); it != vec.end(); ++it)
                patch(*it, continueInd, ctx);
        }
    }

    static void patchCall(int32_t count, std::string str, CompileContext* ctx, BytecodeResult* res)
    {
        uint16_t size = ctx->symbolStack.size();
        std::vector<std::string>* vec = new std::vector<std::string>();
        vec->push_back(str);
        for (int i = 0; i < size - 1; i++)
            vec->push_back(expandSymbol(ctx, (size - 1) - i));
        ctx->bytecode.push_back(Instruction::make_patch_call(&ctx->offset, count, vec));
    }

    BytecodeResult* Bytecode::Generate(ParseResult* parsed, CompileContext* ctx)
    {
        BytecodeResult* res = new BytecodeResult;
        GenerateBlock(parsed->baseNode, ctx, res);
        return res;
    }

    template<typename T>
    static void generateFlagExpressions(T* node, const std::string& symbol, std::vector<int>& bytecodeIndices, CompileContext* ctx, BytecodeResult* res)
    {
        for (auto it = node->flags.begin(); it != node->flags.end(); ++it)
        {
            NodeContent* flag = *it;

            bytecodeIndices.push_back(ctx->bytecode.size());
            Bytecode::GenerateExpression(flag->nodes.at(0), ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);

            bytecodeIndices.push_back(ctx->bytecode.size());
            if (flag->nodes.size() == 2)
            {
                Bytecode::GenerateExpression(flag->nodes.at(1), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
            }
            else
            {
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushbs, ctx->string(symbol + "_" + flag->content)));
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
            }
        }
    }

    void Bytecode::GenerateBlock(Node* block, CompileContext* ctx, BytecodeResult* res)
    {
        for (Node* n : block->nodes)
        {
            switch (n->type)
            {
            case Node::NodeType::MarkedComment:
                translationInfo(ctx, ((NodeContent*)n)->content, nullptr, true);
                break;
            case Node::NodeType::Namespace:
                ctx->symbolStack.push_back(((NodeContent*)n)->content);
                GenerateBlock(n, ctx, res);
                ctx->symbolStack.pop_back();
                break;
            case Node::NodeType::Scene:
            {
                NodeScene* ns = ((NodeScene*)n);
                ctx->symbolStack.push_back(ns->content);
                const std::string& symbol = expandSymbol(ctx);
                if (ctx->sceneBytecode.count(symbol))
                    res->errors.push_back({ BytecodeError::ErrorType::SceneAlreadyExists, ns->token.line, ns->token.column, std::string(symbol) });
               
                int pos = ctx->bytecode.size();
                ctx->generatingFunction = false;

                // Define flag locals
                pushLocalContext(ctx);
                ctx->localCountStack.back() = ns->flags.size();
                for (auto it = ns->flags.begin(); it != ns->flags.end(); ++it)
                    ctx->localStack.push_back((*it)->content);

                GenerateSceneBlock(n, ctx, res);

                popLocalContext(ctx);

                std::vector<int> bytecodeIndices;
                if (pos == ctx->bytecode.size())
                    bytecodeIndices.push_back(-1); // No bytecode
                else
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
                    bytecodeIndices.push_back(pos);
                }

                // Also deal with flag expressions here
                generateFlagExpressions(ns, symbol, bytecodeIndices, ctx, res);

                ctx->sceneBytecode.insert(std::make_pair(symbol, bytecodeIndices));

                ctx->symbolStack.pop_back();
                break;
            }
            case Node::NodeType::Function:
            {
                NodeFunc* func = ((NodeFunc*)n);
                ctx->symbolStack.push_back(func->name);
                const std::string& symbol = expandSymbol(ctx);
                if (ctx->functionBytecode.count(symbol))
                    res->errors.push_back({ BytecodeError::ErrorType::FunctionAlreadyExists, func->token.line, func->token.column, std::string(symbol) });
                int pos = ctx->bytecode.size();
                ctx->generatingFunction = true;

                // Define flag locals and then arguments
                pushLocalContext(ctx);
                ctx->localCountStack.back() = func->flags.size() + func->args.size();
                for (auto it = func->flags.begin(); it != func->flags.end(); ++it)
                    ctx->localStack.push_back((*it)->content);
                for (auto it = func->args.begin(); it != func->args.end(); ++it)
                    ctx->localStack.push_back(it->content);

                GenerateSceneBlock(n, ctx, res);

                popLocalContext(ctx);
                
                std::vector<int> bytecodeIndices;
                if (pos == ctx->bytecode.size())
                    bytecodeIndices.push_back(-1); // No bytecode
                else
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
                    bytecodeIndices.push_back(pos);
                }

                // Also deal with flag expressions here
                generateFlagExpressions(func, symbol, bytecodeIndices, ctx, res);

                ctx->functionBytecode.insert(std::make_pair(symbol, bytecodeIndices));

                ctx->symbolStack.pop_back();
                break;
            }
            case Node::NodeType::Definitions:
            {
                NodeContent* nc = ((NodeContent*)n);
                ctx->symbolStack.push_back(nc->content);
                const std::string& symbol = expandSymbol(ctx);

                // Iterate over all of the definitions, and generate proper info
                for (Node* subNode : n->nodes)
                {
                    if (subNode->type == Node::NodeType::MarkedComment)
                    {
                        translationInfo(ctx, ((NodeContent*)subNode)->content, nullptr, true);
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
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
                            hasExpr = true;
                        }
                        else
                            hasExpr = false;
                        const std::string& name = symbol + '.' + def->key;
                        if (def->excludeValueTranslation)
                        {
                            if (!ctx->definitionBytecode.insert(std::make_pair(name, std::make_pair(def->value, hasExpr ? pos : -1))).second)
                                res->errors.push_back({ BytecodeError::ErrorType::DefinitionAlreadyExists, nc->token.line, nc->token.column, name });
                        }
                        else
                        {
                            if (!ctx->definitionBytecode.insert(std::make_pair(name, std::make_pair(translationInfo(ctx, def->value, def->stringData.get()), hasExpr ? pos : -1))).second)
                                res->errors.push_back({ BytecodeError::ErrorType::DefinitionAlreadyExists, nc->token.line, nc->token.column, name });
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
        pushLocalContext(ctx);

        for (Node* n : block->nodes)
        {
            GenerateSceneStatement(n, ctx, res);
        }

        popLocalContext(ctx);
    }

    void Bytecode::GenerateBasicAssign(NodeContent* var, CompileContext* ctx, BytecodeResult* res)
    {
        int localId = -1;
        auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
        if (it != ctx->localStack.end())
            localId = std::distance(ctx->localStack.begin(), it);

        bool arr = var->nodes.size() != 0;
        if (arr)
        {
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::save);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, localId));

            // Array accesses
            for (int i = 0; i < var->nodes.size(); i++)
            {
                GenerateExpression(var->nodes.at(i), ctx, res);
                if (i + 1 < var->nodes.size())
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup2);
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
                }
            }

            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::load);

            // Array sets
            for (int i = 0; i < var->nodes.size(); i++)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::setarrind);
        }

        // Final set
        if (localId == -1)
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarglb, ctx->string(var->content)));
        else
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarloc, localId));
    }

    void Bytecode::GenerateSceneStatement(Node* statement, CompileContext* ctx, BytecodeResult* res)
    {
        switch (statement->type)
        {
        case Node::NodeType::SceneBlock:
            GenerateSceneBlock(statement, ctx, res);
            break;
        case Node::NodeType::Increment:
        case Node::NodeType::Decrement:
        {
            NodeContent* var = (NodeContent*)(statement->nodes.at(0));
            auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
            int localId;
            if (it == ctx->localStack.end())
            {
                localId = -1;
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, localId));
            }

            int i = 0;
            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
            }

            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(&ctx->offset, statement->type == Node::NodeType::Increment ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);

            for (; i > 0; i--)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarloc, localId));
            break;
        }
        case Node::NodeType::Assign:
        {
            NodeTokenModifier* assign = (NodeTokenModifier*)statement;

            NodeContent* var = (NodeContent*)assign->nodes.at(0);

            int localId = -1;
            if (assign->modifier == KeywordType::Local)
            {
                ctx->localCountStack.back()++;
                if (std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content) != ctx->localStack.end())
                    res->errors.push_back({ BytecodeError::ErrorType::LocalVariableAlreadyExists, assign->token.line, assign->token.column, std::string(var->content) });
                localId = ctx->localStack.size();
                ctx->localStack.push_back(var->content);
            }
            else
            {
                auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
                if (it != ctx->localStack.end())
                    localId = std::distance(ctx->localStack.begin(), it);
            }

            if (assign->token.type != TokenType::Semicolon)
            {
                bool arr = var->nodes.size() != 0;
                bool notEquals = assign->token.type != TokenType::Equals;
                if (arr || notEquals)
                {
                    if (localId == -1)
                        ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
                    else
                        ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, localId));

                    // Array accesses
                    for (int i = 0; i < var->nodes.size(); i++)
                    {
                        GenerateExpression(var->nodes.at(i), ctx, res);
                        if (i + 1 < var->nodes.size() || notEquals)
                        {
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup2);
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
                        }
                    }
                }

                GenerateExpression(assign->nodes.at(1), ctx, res);

                // Deal with non-equal sign operations
                if (notEquals)
                {
                    switch (assign->token.type)
                    {
                    case TokenType::PlusEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::add);
                        break;
                    case TokenType::MinusEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::sub);
                        break;
                    case TokenType::MultiplyEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::mul);
                        break;
                    case TokenType::DivideEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::div);
                        break;
                    case TokenType::ModEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::mod);
                        break;
                    case TokenType::BitwiseAndEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::_bitand);
                        break;
                    case TokenType::BitwiseOrEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::_bitor);
                        break;
                    case TokenType::BitwiseXorEquals:
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::bitxor);
                        break;
                    }
                }

                // Array sets
                if (arr)
                {
                    for (int i = 0; i < var->nodes.size(); i++)
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::setarrind);
                }

                if (localId == -1)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarglb, ctx->string(var->content)));
                else
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarloc, localId));
            }
            break;
        }
        case Node::NodeType::ShorthandChar:
        {
            NodeToken* sc = (NodeToken*)statement;
            switch (sc->token.type)
            {
            case TokenType::String: // todo: add default setting to project file?
            case TokenType::ExcludeString:
            case TokenType::Identifier:
                if (sc->nodes.size() == 1)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushbs, ctx->string(sc->token.content)));
                else
                {
                    for (int i = sc->nodes.size() - 1; i > 0; i--)
                        GenerateExpression(sc->nodes.at(i), ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushbints, ctx->string(sc->token.content), sc->nodes.size()));
                }
                break;
            case TokenType::MarkedString:
                if (sc->nodes.size() == 1)
                {
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushs, translationInfo(ctx, sc->token.content, sc->token.stringData.get())));
                }
                else
                {
                    for (int i = sc->nodes.size() - 1; i > 0; i--)
                        GenerateExpression(sc->nodes.at(i), ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushints, translationInfo(ctx, sc->token.content, sc->token.stringData.get()), sc->nodes.size()));
                }
                break;
            }
            patchCall(1, "char", ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            pushLocalContext(ctx);
            GenerateSceneStatement(sc->nodes.at(0), ctx, res);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::SceneFunction:
        {
            for (auto it = statement->nodes.rbegin(); it != statement->nodes.rend(); ++it)
                GenerateExpression(*it, ctx, res);
            patchCall(statement->nodes.size(), ((NodeContent*)statement)->content, ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            break;
        }
        case Node::NodeType::TextRun:
        case Node::NodeType::ChoiceText:
        {
            NodeText* tr = (NodeText*)statement;
            if (tr->excludeTranslation)
            {
                if (tr->nodes.size() == 0)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushbs, ctx->string(tr->content)));
                else
                {
                    for (auto it = tr->nodes.rbegin(); it != tr->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushbints, ctx->string(tr->content), tr->nodes.size()));
                }
            }
            else
            {
                if (tr->nodes.size() == 0)
                {
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushs, translationInfo(ctx, tr->content, tr->stringData.get())));
                }
                else
                {
                    for (auto it = tr->nodes.rbegin(); it != tr->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushints, translationInfo(ctx, tr->content, tr->stringData.get()), tr->nodes.size()));
                }
            }
            if (statement->type == Node::NodeType::TextRun)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::textrun);
            break;
        }
        case Node::NodeType::Choice:
        {
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::choicebeg);

            // Statement before
            pushLocalContext(ctx);
            GenerateSceneStatement(statement->nodes.at(0), ctx, res);
            popLocalContext(ctx);

            std::vector<int> choices;
            for (int i = 1; i < statement->nodes.size(); i++)
            {
                // Text
                Node* t = statement->nodes.at(i++);
                if (t->type == Node::NodeType::None)
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pushu);
                else
                    GenerateSceneStatement(t, ctx, res);

                // Chance
                GenerateExpression(statement->nodes.at(i++), ctx, res);

                // Require + add option
                Node* r = statement->nodes.at(i++);
                if (r->type == Node::NodeType::None)
                {
                    choices.push_back(patchInstruction(Instruction::Opcode::choiceadd, ctx));
                }
                else
                {
                    GenerateExpression(r, ctx, res);
                    choices.push_back(patchInstruction(Instruction::Opcode::choiceaddt, ctx));
                }
            }

            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::choicesel);

            // Actual branch paths
            int j = 0;
            std::vector<int> jumps;
            int size = statement->nodes.size();
            for (int i = 4; i < size; i += 4)
            {
                patch(choices.at(j++), ctx);
                pushLocalContext(ctx);
                GenerateSceneStatement(statement->nodes.at(i), ctx, res);
                popLocalContext(ctx);
                if (i + 4 < size)
                    jumps.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            }

            // End location of the choice
            for (auto it = jumps.begin(); it != jumps.end(); ++it)
                patch(*it, ctx);
            break;
        }
        case Node::NodeType::Choose:
        {
            std::vector<int> choices;
            for (int i = 0; i < statement->nodes.size(); i++)
            {
                // Chance
                GenerateExpression(statement->nodes.at(i++), ctx, res);

                // Require + add option
                Node* r = statement->nodes.at(i++);
                if (r->type == Node::NodeType::None)
                {
                    choices.push_back(patchInstruction(Instruction::Opcode::chooseadd, ctx));
                }
                else
                {
                    GenerateExpression(r, ctx, res);
                    choices.push_back(patchInstruction(Instruction::Opcode::chooseaddt, ctx));
                }
            }

            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::choosesel);

            // Actual branch paths
            int j = 0;
            std::vector<int> jumps;
            int size = statement->nodes.size();
            for (int i = 2; i < size; i += 3)
            {
                patch(choices.at(j++), ctx);
                pushLocalContext(ctx);
                GenerateSceneStatement(statement->nodes.at(i), ctx, res);
                popLocalContext(ctx);
                if (i + 3 < size)
                    jumps.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            }

            // End location of the choose
            for (auto it = jumps.begin(); it != jumps.end(); ++it)
                patch(*it, ctx);
            break;
        }
        case Node::NodeType::If:
        {
            GenerateExpression(statement->nodes.at(0), ctx, res);
            int jumpFalse = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLocalContext(ctx);
            GenerateSceneStatement(statement->nodes.at(1), ctx, res);
            popLocalContext(ctx);
            if (statement->nodes.size() == 3)
            {
                int jump = patchInstruction(Instruction::Opcode::j, ctx);
                patch(jumpFalse, ctx);
                pushLocalContext(ctx);
                GenerateSceneStatement(statement->nodes.at(2), ctx, res);
                popLocalContext(ctx);
                patch(jump, ctx);
            }
            else
                patch(jumpFalse, ctx);
            break;
        }
        case Node::NodeType::While:
        {
            pushLocalContext(ctx);
            int cond = ctx->offset;
            GenerateExpression(statement->nodes.at(0), ctx, res);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLoopContext(ctx);
            GenerateSceneStatement(statement->nodes.at(1), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::j, cond - (ctx->offset + 5)));
            popLoopContext(cond, ctx);
            patch(fail, ctx);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::For:
        {
            pushLocalContext(ctx);
            GenerateSceneStatement(statement->nodes.at(0), ctx, res);
            int cond = ctx->offset;
            GenerateExpression(statement->nodes.at(1), ctx, res);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLoopContext(ctx);
            GenerateSceneStatement(statement->nodes.at(3), ctx, res);
            int cont = ctx->bytecode.size();
            GenerateSceneStatement(statement->nodes.at(2), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::j, cond - (ctx->offset + 5)));
            patch(fail, ctx);
            popLoopContext(cont, ctx);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Do:
        {
            pushLocalContext(ctx);
            int top = ctx->offset;
            pushLoopContext(ctx);
            GenerateSceneStatement(statement->nodes.at(0), ctx, res);
            int cont = ctx->bytecode.size();
            GenerateExpression(statement->nodes.at(1), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::jt, top - (ctx->offset + 5)));
            popLoopContext(cont, ctx);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Repeat:
        {
            GenerateExpression(statement->nodes.at(0), ctx, res);
            int top = ctx->offset;
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 0));
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpgt);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLocalContext(ctx);
            pushLoopContext(ctx, { Instruction::Opcode::pop });
            GenerateSceneStatement(statement->nodes.at(1), ctx, res);
            int cont = ctx->bytecode.size();
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::sub);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::j, top - (ctx->offset + 5)));
            patch(fail, ctx);
            popLoopContext(cont, ctx);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Switch:
        {
            LoopContext* enclosing = nullptr;
            if (ctx->loopStack.size() != 0)
                enclosing = &ctx->loopStack.back();

            GenerateExpression(statement->nodes.at(0), ctx, res);
            pushLocalContext(ctx);
            pushLoopContext(ctx, { {Instruction::Opcode::pop} });

            std::vector<std::pair<int /* patch */, int /* index in nodes */>> cases;

            int defaultInd = -1;
            int defaultInsertLoc = -1;

            // Find indices of cases, write branches
            bool foundCase = false;
            for (int i = 1; i < statement->nodes.size(); i++)
            {
                Node* curr = statement->nodes.at(i);
                switch (curr->type)
                {
                case Node::NodeType::SwitchCase:
                    foundCase = true;

                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                    GenerateExpression(curr->nodes.at(0), ctx, res);
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                    cases.emplace_back(std::make_pair(patchInstruction(Instruction::Opcode::jt, ctx), i));
                    break;
                case Node::NodeType::SwitchDefault:
                    foundCase = true;
                    
                    defaultInd = i;
                    defaultInsertLoc = cases.size();
                    break;
                default:
                    if (!foundCase)
                    {
                        Token& t = ((NodeToken*)statement)->token;
                        res->errors.push_back({ BytecodeError::ErrorType::StatementsBeforeSwitchCase, t.line, t.column });
                    }
                    break;
                }
            }

            // todo? check for duplicates?

            int allFail;
            if (defaultInd != -1)
            {
                allFail = -1;
                cases.insert(cases.begin() + defaultInsertLoc, std::make_pair(patchInstruction(Instruction::Opcode::j, ctx), defaultInd));
            } 
            else
                allFail = patchInstruction(Instruction::Opcode::j, ctx);

            // Write statements
            for (auto it = cases.begin(); it != cases.end(); ++it)
            {
                auto next = std::next(it);
                int end = (next == cases.end()) ? statement->nodes.size() : next->second;
                patch(it->first, ctx);
                for (int i = it->second + 1; i < end; i++)
                    GenerateSceneStatement(statement->nodes.at(i), ctx, res);
            }

            if (enclosing && ctx->loopStack.back().continuePatch.size() != 0)
            {
                int end = patchInstruction(Instruction::Opcode::j, ctx);
                popLoopContextContinue(ctx->bytecode.size(), ctx);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
                int newContinue = patchInstruction(Instruction::Opcode::j, ctx);
                popLoopContext(-1, ctx, res);
                popLocalContextForLoop(ctx, ctx->loopStack.back());
                ctx->loopStack.back().continuePatch.push_back(newContinue);
                patch(end, ctx);
            } 
            else
                popLoopContext(-1, ctx, res);
            if (allFail != -1)
                patch(allFail, ctx);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::SwitchSimple:
        {
            LoopContext* enclosing = nullptr;
            if (ctx->loopStack.size() != 0)
                enclosing = &ctx->loopStack.back();

            GenerateExpression(statement->nodes.at(0), ctx, res);
            pushLocalContext(ctx);
            pushLoopContext(ctx, { {Instruction::Opcode::pop} });

            std::vector<int> jumps;

            Node* defaultNode = nullptr;
            int defaultInd = -1;

            // Find indices of cases, write branches
            for (int i = 1; i < statement->nodes.size(); i += 2)
            {
                Node* curr = statement->nodes.at(i);
                if (curr->type == Node::NodeType::SwitchDefault)
                {
                    defaultInd = i + 1;
                    defaultNode = statement->nodes.at(defaultInd);
                }
                else if (curr->type == Node::NodeType::ExprRange)
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);

                    GenerateExpression(curr->nodes.at(0), ctx, res); // lower range
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpgte);
                    int jumpToNext = patchInstruction(Instruction::Opcode::jf, ctx);
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);

                    GenerateExpression(curr->nodes.at(1), ctx, res); // upper range
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmplte);

                    jumps.push_back(patchInstruction(Instruction::Opcode::jt, ctx));

                    patch(jumpToNext, ctx);
                }
                else
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                    GenerateExpression(curr, ctx, res);
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                    jumps.push_back(patchInstruction(Instruction::Opcode::jt, ctx));
                }
            }

            // Default statement if all conditions fail
            if (defaultNode != nullptr)
                GenerateSceneStatement(defaultNode, ctx, res);

            std::vector<int> toEnd{}; 
            toEnd.push_back(patchInstruction(Instruction::Opcode::j, ctx));

            // Actual statements
            int counter = 0;
            for (int i = 2; i < statement->nodes.size(); i += 2)
            {
                if (i == defaultInd)
                    continue;

                patch(jumps.at(counter), ctx);

                GenerateSceneStatement(statement->nodes.at(i), ctx, res);
                toEnd.emplace_back(patchInstruction(Instruction::Opcode::j, ctx));

                counter++;
            }

            if (enclosing && ctx->loopStack.back().continuePatch.size() != 0)
            {
                int end = patchInstruction(Instruction::Opcode::j, ctx);
                popLoopContextContinue(ctx->bytecode.size(), ctx);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
                int newContinue = patchInstruction(Instruction::Opcode::j, ctx);
                popLoopContext(-1, ctx, res);
                popLocalContextForLoop(ctx, ctx->loopStack.back());
                ctx->loopStack.back().continuePatch.push_back(newContinue);
                patch(end, ctx);
            }
            else
                popLoopContext(-1, ctx, res);
            for (auto it = toEnd.begin(); it != toEnd.end(); ++it)
                patch(*it, ctx);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Continue:
        {
            if (ctx->loopStack.size() == 0)
            {
                Token t = ((NodeToken*)statement)->token;
                res->errors.push_back({ BytecodeError::ErrorType::ContinueOutsideOfLoop, t.line, t.column });
                break;
            }

            popLocalContextForLoop(ctx, ctx->loopStack.back());
            ctx->loopStack.back().continuePatch.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            break;
        }
        case Node::NodeType::Break:
        {
            if (ctx->loopStack.size() == 0)
            {
                Token t = ((NodeToken*)statement)->token;
                res->errors.push_back({ BytecodeError::ErrorType::BreakOutsideOfLoop, t.line, t.column });
                break;
            }

            popLocalContextForLoop(ctx, ctx->loopStack.back());
            ctx->loopStack.back().endLoopPatch.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            break;
        }
        case Node::NodeType::Return:
        {
            bool cleanup = ctx->loopStack.size() != 0 || ctx->localStack.size() != 0;
            std::vector<Instruction> cleanupInstructions;

            if (cleanup)
            {
                // Make instructions for cleaning up, and check if it's even necessary
                for (auto it = ctx->loopStack.rbegin(); it != ctx->loopStack.rend(); ++it)
                {
                    cleanupInstructions.insert(cleanupInstructions.end(), 
                                               (*it).returnCleanup.begin(), (*it).returnCleanup.end());
                }

                for (int i = ctx->localStack.size() - 1; i >= 0; i--)
                {
                    cleanupInstructions.push_back(Instruction::make_int(nullptr, Instruction::Opcode::freeloc, i));
                }

                cleanup = cleanupInstructions.size() != 0;
            }

            // Expression to return
            if (statement->nodes.size() == 1)
            {
                GenerateExpression(statement->nodes.at(0), ctx, res);
                if (cleanup)
                {
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::save);
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
                }
            }

            // Emit cleaning up instructions
            if (cleanup)
            {
                for (auto it = cleanupInstructions.begin(); it != cleanupInstructions.end(); ++it)
                {
                    it->offset = ctx->offset;
                    if (it->opcode == Instruction::Opcode::freeloc)
                        ctx->offset += 5;
                    else
                        ctx->offset += 1;
                    ctx->bytecode.push_back(*it);
                }
            }

            // Actually return
            if (statement->nodes.size() == 0)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::exit);
            else
            {
                if (cleanup)
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::load);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::ret);
            }
            break;
        }
        case Node::NodeType::Sequence:
        {
            int top = ctx->offset;
            GenerateExpression(statement->nodes.at(0), ctx, res);
            pushLocalContext(ctx);
            pushLoopContext(ctx, { {Instruction::Opcode::pop} });

            std::vector<std::vector<int>> jumps{};

            for (int j = 1; j < statement->nodes.size(); j++)
            {
                Node* subNode = statement->nodes.at(j);

                for (int i = 0; i < subNode->nodes.size(); i += 2)
                {
                    Node* curr = subNode->nodes.at(i);

                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                    if (curr->type == Node::NodeType::ExprRange)
                    {
                        int farther = -1;
                        if (i + 2 >= subNode->nodes.size())
                        {
                            // There's nothing after this entry
                            GenerateExpression(curr->nodes.at(1), ctx, res); // upper range
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                            farther = patchInstruction(Instruction::Opcode::jt, ctx);
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                        }

                        GenerateExpression(curr->nodes.at(0), ctx, res); // lower range
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpgte);
                        int jumpToNext = patchInstruction(Instruction::Opcode::jf, ctx);
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);

                        GenerateExpression(curr->nodes.at(1), ctx, res); // upper range
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmplte);

                        jumps.push_back({ patchInstruction(Instruction::Opcode::jt, ctx), farther });

                        patch(jumpToNext, ctx);
                    }
                    else
                    {
                        GenerateExpression(curr, ctx, res);
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                        jumps.push_back({ patchInstruction(Instruction::Opcode::jt, ctx) });
                    }
                }
            }

            std::vector<int> toEnd{};
            toEnd.emplace_back(patchInstruction(Instruction::Opcode::j, ctx));

            int counter = 0;
            for (int j = 1; j < statement->nodes.size(); j++)
            {
                Node* subNode = statement->nodes.at(j);

                for (int i = 1; i < subNode->nodes.size(); i += 2)
                {
                    std::vector<int> currJumps = jumps.at(counter);
                    patch(currJumps.at(0), ctx);

                    if (i + 1 < subNode->nodes.size())
                    {
                        if (currJumps.size() == 2)
                        {
                            // farther is -1, since it's not used
                            // if maximum value of range, jump to next, otherwise increment
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                            GenerateExpression(subNode->nodes.at(i - 1)->nodes.at(1), ctx, res);
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                            int notEqual = patchInstruction(Instruction::Opcode::jf, ctx);

                            // Jump to next
                            Node* next = subNode->nodes.at(i + 1);
                            if (next->type == Node::NodeType::ExprRange)
                                GenerateExpression(next->nodes.at(0), ctx, res); // lower range
                            else
                                GenerateExpression(next, ctx, res);
                            int equal = patchInstruction(Instruction::Opcode::j, ctx);

                            patch(notEqual, ctx);

                            // Otherwise, increment
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
                            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::add);

                            patch(equal, ctx);

                            GenerateBasicAssign((NodeContent*)statement->nodes.at(0), ctx, res);
                        }
                        else
                        {
                            // Assign to next
                            Node* next = subNode->nodes.at(i + 1);
                            if (next->type == Node::NodeType::ExprRange)
                                GenerateExpression(next->nodes.at(0), ctx, res); // lower range
                            else
                                GenerateExpression(next, ctx, res);
                            GenerateBasicAssign((NodeContent*)statement->nodes.at(0), ctx, res);
                        }
                    }
                    else if (currJumps.size() == 2)
                    {
                        // Increment
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
                        ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
                        ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::add);
                        GenerateBasicAssign((NodeContent*)statement->nodes.at(0), ctx, res);

                        patch(currJumps.at(1), ctx); // Patch the farther jump here; there's nothing after this range
                    }

                    // Actual statement and branch away
                    GenerateSceneStatement(subNode->nodes.at(i), ctx, res);
                    toEnd.emplace_back(patchInstruction(Instruction::Opcode::j, ctx));

                    counter++;
                }
            }

            if (ctx->loopStack.back().continuePatch.size() != 0)
            {
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::j, top - (ctx->offset + 5)));
                popLoopContext(ctx->bytecode.size() - 2, ctx, res);
            }
            else
                popLoopContext(-1, ctx, res);
            for (auto it = toEnd.begin(); it != toEnd.end(); ++it)
                patch(*it, ctx);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pop);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::MarkedComment:
            translationInfo(ctx, ((NodeContent*)statement)->content, nullptr, true);
            break;
        }
    }

    void Bytecode::GenerateExpression(Node* expr, CompileContext* ctx, BytecodeResult* res)
    {
        int i = 0; // Index within subnodes (used in different ways)

        switch (expr->type)
        {
        case Node::NodeType::ExprTernary:
        {
            // Condition
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            int patch1 = patchInstruction(Instruction::Opcode::jf, ctx);

            // Result 1
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            int patch2 = patchInstruction(Instruction::Opcode::j, ctx);

            patch(patch1, ctx);

            // Result 2
            GenerateExpression(expr->nodes.at(i++), ctx, res);

            patch(patch2, ctx);
            break;
        }
        case Node::NodeType::ExprBinary:
        {
            NodeToken* binary = (NodeToken*)expr;

            // Put left value onto stack
            GenerateExpression(expr->nodes.at(i++), ctx, res);

            bool isAnd = binary->token.type == TokenType::LogicalAnd;
            if (isAnd || binary->token.type == TokenType::LogicalOr)
            {
                // Handle short circuit operators (logical and/or)
                int jump = -1;

                while (i < expr->nodes.size())
                {
                    if (isAnd)
                        jump = patchInstruction(Instruction::Opcode::jf, ctx);
                    else
                        jump = patchInstruction(Instruction::Opcode::jt, ctx);
                    GenerateExpression(expr->nodes.at(i++), ctx, res);
                }

                if (jump == -1)
                {
                    res->errors.push_back({ BytecodeError::ErrorType::UnexpectedError, 0, 0 });
                    break;
                }

                int end = patchInstruction(Instruction::Opcode::j, ctx);
                patch(jump, ctx);
                if (isAnd)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 0));
                else
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
                patch(end, ctx);
            }
            else
            {
                // Push right value to stack
                GenerateExpression(expr->nodes.at(i++), ctx, res);

                // Perform operation
                switch (binary->token.type)
                {
                case TokenType::CompareEQ:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpeq);
                    break;
                case TokenType::CompareGT:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpgt);
                    break;
                case TokenType::CompareGTE:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpgte);
                    break;
                case TokenType::CompareLT:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmplt);
                    break;
                case TokenType::CompareLTE:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmplte);
                    break;
                case TokenType::CompareNEQ:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::cmpneq);
                    break;
                case TokenType::BitwiseOr:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::_bitor);
                    break;
                case TokenType::BitwiseAnd:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::_bitand);
                    break;
                case TokenType::BitwiseXor:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::bitxor);
                    break;
                case TokenType::BitwiseLShift:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::bitls);
                    break;
                case TokenType::BitwiseRShift:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::bitrs);
                    break;
                case TokenType::Plus:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::add);
                    break;
                case TokenType::Minus:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::sub);
                    break;
                case TokenType::Multiply:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::mul);
                    break;
                case TokenType::Divide:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::div);
                    break;
                case TokenType::Mod:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::mod);
                    break;
                case TokenType::Power:
                    ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pow);
                    break;
                }
            }
            break;
        }
        case Node::NodeType::ExprConstant:
        {
            NodeToken* constant = (NodeToken*)expr;
            switch (constant->token.type)
            {
            case TokenType::Number:
                if (constant->token.content.find('.') == std::string::npos)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, std::stoi(constant->token.content)));
                else
                    ctx->bytecode.push_back(Instruction::make_double(&ctx->offset, Instruction::Opcode::pushd, std::stod(constant->token.content)));
                break;
            case TokenType::Percentage:
                if (constant->token.content.find('.') == std::string::npos)
                    ctx->bytecode.push_back(Instruction::make_double(&ctx->offset, Instruction::Opcode::pushd, std::stoi(constant->token.content) / 100.0));
                else
                    ctx->bytecode.push_back(Instruction::make_double(&ctx->offset, Instruction::Opcode::pushd, std::stod(constant->token.content) / 100.0));
                break;
            case TokenType::String: // todo: add default setting to project file?
            case TokenType::ExcludeString:
                if (constant->nodes.size() == 0)
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushbs, ctx->string(constant->token.content)));
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushbints, ctx->string(constant->token.content), constant->nodes.size()));
                }
                break;
            case TokenType::MarkedString:
                if (constant->nodes.size() == 0)
                {
                    ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushs, translationInfo(ctx, constant->token.content, constant->token.stringData.get())));
                }
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(&ctx->offset, Instruction::Opcode::pushints, translationInfo(ctx, constant->token.content, constant->token.stringData.get()), constant->nodes.size()));
                }
                break;
            case TokenType::Undefined:
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pushu);
                break;
            }
            break;
        }
        case Node::NodeType::ExprNot:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::inv);
            break;
        }
        case Node::NodeType::ExprNegate:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::neg);
            break;
        }
        case Node::NodeType::ExprBitwiseNegate:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::bitneg);
            break;
        }
        case Node::NodeType::ExprArray:
        {
            for (int j = 0; j < expr->nodes.size(); j++)
                GenerateExpression(expr->nodes.at(i++), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::makearr, expr->nodes.size()));
            break;
        }
        case Node::NodeType::Variable:
        {
            NodeContent* var = (NodeContent*)expr;
            auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
            if (it == ctx->localStack.end())
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, std::distance(ctx->localStack.begin(), it)));

            // Array accesses
            while (i < expr->nodes.size())
            {
                GenerateExpression(expr->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
            }
            break;
        }
        case Node::NodeType::ExprPreIncrement:
        case Node::NodeType::ExprPreDecrement:
        {
            NodeContent* var = (NodeContent*)(expr->nodes.at(0));
            auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
            int localId;
            if (it == ctx->localStack.end())
            {
                localId = -1;
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, localId));
            }

            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
            }

            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(&ctx->offset, expr->type == Node::NodeType::ExprPreIncrement ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);
            if (i == 0)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
            else
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::save);

            for (int j = 0; j < i; j++)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarloc, localId));

            if (i != 0)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::load);
            break;
        }
        case Node::NodeType::ExprPostIncrement:
        case Node::NodeType::ExprPostDecrement:
        {
            NodeContent* var = (NodeContent*)(expr->nodes.at(0));
            auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
            int localId;
            if (it == ctx->localStack.end())
            {
                localId = -1;
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushvarloc, localId));
            }

            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
            }

            if (i == 0)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::dup);
            else
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::save);

            ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(&ctx->offset, expr->type == Node::NodeType::ExprPostIncrement ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);

            for (int j = 0; j < i; j++)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(&ctx->offset, Instruction::Opcode::setvarloc, localId));

            if (i != 0)
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::load);
            break;
        }
        case Node::NodeType::ExprAccessArray:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            while (i < expr->nodes.size())
            {
                GenerateExpression(expr->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(&ctx->offset, Instruction::Opcode::pusharrind);
            }
            break;
        }
        case Node::NodeType::SceneFunction:
        {
            for (auto it = expr->nodes.rbegin(); it != expr->nodes.rend(); ++it)
                GenerateExpression(*it, ctx, res);
            patchCall(expr->nodes.size(), ((NodeContent*)expr)->content, ctx, res);
            break;
        }
        }
    }
}
