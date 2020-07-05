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

    static int translationInfo(CompileContext* ctx, std::string text, bool isComment = false)
    {
        int res = ctx->translationStringIndex;
        if (ctx->project->options.translationOutput.empty())
        {
            if (!isComment)
            {
                ctx->translationInfo.push_back({ "", false, text });
                ctx->translationStringIndex++;
            }
        }
        else if (ctx->project->options.translationPrivate)
        {
            if (!isComment)
            {
                ctx->translationInfo.push_back({ expandSymbol(ctx), false, text });
                ctx->translationStringIndex++;
            } else
                ctx->translationInfo.push_back({ expandSymbol(ctx), true, text });
        }
        else if (!isComment)
        {
            ctx->translationInfo.push_back({ "", false, text });
            ctx->translationStringIndex++;
        }
        return res;
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

    static void patch(int ind, int targ, CompileContext* ctx)
    {
        ctx->bytecode.at(ind).arg = targ - ind;
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
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::freeloc, ctx->localStack.size()));
        }
    }

    static void pushLoopContext(CompileContext* ctx, std::vector<Instruction> cleanup = {})
    {
        ctx->loopStack.push_back({ std::vector<int>(), std::vector<int>(), cleanup });
    }

    static void popLoopContext(int continueInd, CompileContext* ctx, BytecodeResult* res = nullptr)
    {
        {
            if (continueInd == -1 && ctx->loopStack.back().continuePatch.size() != 0)
            {
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
                ctx->generatingFunction = false;

                GenerateSceneBlock(n, ctx, res);

                if (pos == ctx->bytecode.size())
                    ctx->sceneBytecode.insert(std::make_pair(symbol, -1));
                else
                {
                    ctx->bytecode.emplace_back(Instruction::Opcode::exit);
                    ctx->sceneBytecode.insert(std::make_pair(symbol, pos));
                }

                ctx->symbolStack.pop_back();
                break;
            }
            case Node::NodeType::Function:
            {
                NodeFunc* func = ((NodeFunc*)n);
                ctx->symbolStack.push_back(func->name);
                const std::string& symbol = expandSymbol(ctx);
                if (ctx->functionBytecode.count(symbol))
                    res->errors.push_back({ BytecodeError::ErrorType::FunctionAlreadyExists, 0, 0, symbol.c_str() });
                int pos = ctx->bytecode.size();
                ctx->generatingFunction = true;

                pushLocalContext(ctx);
                ctx->localCountStack.back() = func->args.size();
                for (auto it = func->args.begin(); it != func->args.end(); ++it)
                    ctx->localStack.push_back(it->content);
                GenerateSceneBlock(n, ctx, res);
                popLocalContext(ctx);

                if (pos == ctx->bytecode.size())
                    ctx->functionBytecode.insert(std::make_pair(symbol, -1));
                else
                {
                    ctx->bytecode.emplace_back(Instruction::Opcode::exit);
                    ctx->functionBytecode.insert(std::make_pair(symbol, pos));
                }

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
                            ctx->definitionBytecode.insert(std::make_pair(symbol + '.' + def->key, std::make_pair(translationInfo(ctx, def->value), hasExpr ? pos : -1)));
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
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarloc, localId));
            }

            int i = 0;
            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
            }

            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(statement->type == Node::NodeType::Increment ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);

            for (; i > 0; i--)
                ctx->bytecode.emplace_back(Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarloc, localId));
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
                    res->errors.push_back({ BytecodeError::ErrorType::LocalVariableAlreadyExists, assign->token.line, assign->token.column, var->content.c_str() });
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
                        ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarglb, ctx->string(var->content)));
                    else
                        ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarloc, localId));

                    // Array accesses
                    for (int i = 0; i < var->nodes.size(); i++)
                    {
                        GenerateExpression(var->nodes.at(i), ctx, res);
                        if (i + 1 < var->nodes.size() || notEquals)
                        {
                            ctx->bytecode.emplace_back(Instruction::Opcode::dup2);
                            ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
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
                        ctx->bytecode.emplace_back(Instruction::Opcode::add);
                        break;
                    case TokenType::MinusEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::sub);
                        break;
                    case TokenType::MultiplyEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::mul);
                        break;
                    case TokenType::DivideEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::div);
                        break;
                    case TokenType::ModEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::mod);
                        break;
                    case TokenType::BitwiseAndEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::bitand);
                        break;
                    case TokenType::BitwiseOrEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::bitor);
                        break;
                    case TokenType::BitwiseXorEquals:
                        ctx->bytecode.emplace_back(Instruction::Opcode::bitxor);
                        break;
                    }
                }

                // Array sets
                if (arr)
                {
                    for (int i = 0; i < var->nodes.size(); i++)
                        ctx->bytecode.emplace_back(Instruction::Opcode::setarrind);
                }

                if (localId == -1)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarglb, ctx->string(var->content)));
                else
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarloc, localId));
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
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushbs, ctx->string(sc->token.content)));
                else
                {
                    for (int i = sc->nodes.size() - 1; i > 0; i--)
                        GenerateExpression(sc->nodes.at(i), ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushbints, ctx->string(sc->token.content), sc->nodes.size()));
                }
                break;
            case TokenType::MarkedString:
                if (sc->nodes.size() == 1)
                {
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushs, translationInfo(ctx, sc->token.content)));
                }
                else
                {
                    for (int i = sc->nodes.size() - 1; i > 0; i--)
                        GenerateExpression(sc->nodes.at(i), ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushints, translationInfo(ctx, sc->token.content), sc->nodes.size()));
                }
                break;
            }
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::PATCH_CALL, ctx->string("char")));
            ctx->bytecode.emplace_back(Instruction::Opcode::pop);
            pushLocalContext(ctx);
            GenerateSceneStatement(sc->nodes.at(0), ctx, res);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::SceneFunction:
        {
            for (auto it = statement->nodes.rbegin(); it != statement->nodes.rend(); ++it)
                GenerateExpression(*it, ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::PATCH_CALL, ctx->string(((NodeContent*)statement)->content)));
            ctx->bytecode.emplace_back(Instruction::Opcode::pop);
            break;
        }
        case Node::NodeType::TextRun:
        case Node::NodeType::ChoiceText:
        {
            NodeText* tr = (NodeText*)statement;
            if (tr->excludeTranslation)
            {
                if (tr->nodes.size() == 0)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushbs, ctx->string(tr->content)));
                else
                {
                    for (auto it = tr->nodes.rbegin(); it != tr->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushbints, ctx->string(tr->content), tr->nodes.size()));
                }
            }
            else
            {
                if (tr->nodes.size() == 0)
                {
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushs, translationInfo(ctx, tr->content)));
                }
                else
                {
                    for (auto it = tr->nodes.rbegin(); it != tr->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushints, translationInfo(ctx, tr->content), tr->nodes.size()));
                }
            }
            if (statement->type == Node::NodeType::TextRun)
                ctx->bytecode.emplace_back(Instruction::Opcode::textrun);
            break;
        }
        case Node::NodeType::Choice:
        {
            ctx->bytecode.emplace_back(Instruction::Opcode::choicebeg);

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
                    ctx->bytecode.emplace_back(Instruction::Opcode::pushu);
                else
                    GenerateSceneStatement(t, ctx, res);

                // Chance
                GenerateExpression(statement->nodes.at(i++), ctx, res);

                // Require
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

            ctx->bytecode.emplace_back(Instruction::Opcode::choicesel);

            // Actual branch paths
            int j = 0;
            std::vector<int> jumps;
            for (int i = 4; i < statement->nodes.size(); i += 4)
            {
                patch(choices.at(j++), ctx);
                pushLocalContext(ctx);
                GenerateSceneStatement(statement->nodes.at(i), ctx, res);
                popLocalContext(ctx);
                jumps.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            }

            // End location of the choice
            for (int i = 0; i < jumps.size(); i++)
                patch(jumps.at(i), ctx);
            break;
        }
        case Node::NodeType::Choose:
        {
            std::vector<int> choices;
            for (int i = 0; i < statement->nodes.size(); i++)
            {
                // Chance
                GenerateExpression(statement->nodes.at(i++), ctx, res);

                // Require
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

            ctx->bytecode.emplace_back(Instruction::Opcode::choosesel);

            // Actual branch paths
            int j = 0;
            std::vector<int> jumps;
            for (int i = 2; i < statement->nodes.size(); i += 3)
            {
                patch(choices.at(j++), ctx);
                pushLocalContext(ctx);
                GenerateSceneStatement(statement->nodes.at(i), ctx, res);
                popLocalContext(ctx);
                jumps.push_back(patchInstruction(Instruction::Opcode::j, ctx));
            }

            // End location of the choice
            for (int i = 0; i < jumps.size(); i++)
                patch(jumps.at(i), ctx);
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
            int cond = ctx->bytecode.size();
            GenerateExpression(statement->nodes.at(0), ctx, res);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLoopContext(ctx);
            pushLocalContext(ctx);
            GenerateSceneStatement(statement->nodes.at(1), ctx, res);
            popLocalContext(ctx);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::j, cond - ctx->bytecode.size()));
            popLoopContext(cond, ctx);
            patch(fail, ctx);
            break;
        }
        case Node::NodeType::For:
        {
            pushLocalContext(ctx);
            GenerateSceneStatement(statement->nodes.at(0), ctx, res);
            int cond = ctx->bytecode.size();
            GenerateExpression(statement->nodes.at(1), ctx, res);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLoopContext(ctx);
            GenerateSceneStatement(statement->nodes.at(3), ctx, res);
            int cont = ctx->bytecode.size();
            GenerateSceneStatement(statement->nodes.at(2), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::j, cond - ctx->bytecode.size()));
            patch(fail, ctx);
            popLoopContext(cont, ctx);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Do:
        {
            int top = ctx->bytecode.size();
            pushLocalContext(ctx);
            pushLoopContext(ctx);
            GenerateSceneStatement(statement->nodes.at(0), ctx, res);
            int cont = ctx->bytecode.size();
            GenerateExpression(statement->nodes.at(1), ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::jt, top - ctx->bytecode.size()));
            popLoopContext(cont, ctx);
            popLocalContext(ctx);
            break;
        }
        case Node::NodeType::Repeat:
        {
            GenerateExpression(statement->nodes.at(0), ctx, res);
            int top = ctx->bytecode.size();
            ctx->bytecode.emplace_back(Instruction::Opcode::dup);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 0));
            ctx->bytecode.emplace_back(Instruction::Opcode::cmpgt);
            int fail = patchInstruction(Instruction::Opcode::jf, ctx);
            pushLocalContext(ctx);
            pushLoopContext(ctx, { {Instruction::Opcode::pop} });
            GenerateSceneStatement(statement->nodes.at(1), ctx, res);
            int cont = ctx->bytecode.size();
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(Instruction::Opcode::sub);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::j, top - ctx->bytecode.size()));
            patch(fail, ctx);
            popLoopContext(cont, ctx);
            ctx->bytecode.emplace_back(Instruction::Opcode::pop);
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

                    ctx->bytecode.emplace_back(Instruction::Opcode::dup);
                    GenerateExpression(curr->nodes.at(0), ctx, res);
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmpeq);
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
            } else
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
                popLoopContext(ctx->bytecode.size(), ctx);
                ctx->bytecode.emplace_back(Instruction::Opcode::pop);
                ctx->loopStack.back().continuePatch.push_back(patchInstruction(Instruction::Opcode::j, ctx));
                patch(end, ctx);
            } else
                popLoopContext(-1, ctx, res);
            if (allFail != -1)
                patch(allFail, ctx);
            ctx->bytecode.emplace_back(Instruction::Opcode::pop);
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
                    cleanupInstructions.push_back(Instruction::make_int(Instruction::Opcode::freeloc, i));
                }

                cleanup = cleanupInstructions.size() != 0;
            }

            // Expression to return
            if (statement->nodes.size() == 1)
            {

                GenerateExpression(statement->nodes.at(0), ctx, res);
                if (cleanup)
                {
                    ctx->bytecode.emplace_back(Instruction::Opcode::save);
                    ctx->bytecode.emplace_back(Instruction::Opcode::pop);
                }
            }

            // Emit cleaning up instructions
            if (cleanup)
                ctx->bytecode.insert(ctx->bytecode.end(), cleanupInstructions.begin(), cleanupInstructions.end());

            // Actually return
            if (statement->nodes.size() == 0)
                ctx->bytecode.emplace_back(Instruction::Opcode::exit);
            else
            {
                if (cleanup)
                    ctx->bytecode.emplace_back(Instruction::Opcode::load);
                ctx->bytecode.emplace_back(Instruction::Opcode::ret);
            }
            break;
        }
        case Node::NodeType::MarkedComment:
            translationInfo(ctx, ((NodeContent*)statement)->content, true);
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
                int jump;

                while (i < expr->nodes.size())
                {
                    if (isAnd)
                        jump = patchInstruction(Instruction::Opcode::jf, ctx);
                    else
                        jump = patchInstruction(Instruction::Opcode::jt, ctx);
                    GenerateExpression(expr->nodes.at(i++), ctx, res);
                }

                int end = patchInstruction(Instruction::Opcode::j, ctx);
                patch(jump, ctx);
                if (isAnd)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 0));
                else
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 1));
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
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmpeq);
                    break;
                case TokenType::CompareGT:
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmpgt);
                    break;
                case TokenType::CompareGTE:
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmpgte);
                    break;
                case TokenType::CompareLT:
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmplt);
                    break;
                case TokenType::CompareLTE:
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmplte);
                    break;
                case TokenType::CompareNEQ:
                    ctx->bytecode.emplace_back(Instruction::Opcode::cmpneq);
                    break;
                case TokenType::BitwiseOr:
                    ctx->bytecode.emplace_back(Instruction::Opcode::bitor);
                    break;
                case TokenType::BitwiseAnd:
                    ctx->bytecode.emplace_back(Instruction::Opcode::bitand);
                    break;
                case TokenType::BitwiseXor:
                    ctx->bytecode.emplace_back(Instruction::Opcode::bitxor);
                    break;
                case TokenType::BitwiseLShift:
                    ctx->bytecode.emplace_back(Instruction::Opcode::bitls);
                    break;
                case TokenType::BitwiseRShift:
                    ctx->bytecode.emplace_back(Instruction::Opcode::bitrs);
                    break;
                case TokenType::Plus:
                    ctx->bytecode.emplace_back(Instruction::Opcode::add);
                    break;
                case TokenType::Minus:
                    ctx->bytecode.emplace_back(Instruction::Opcode::sub);
                    break;
                case TokenType::Multiply:
                    ctx->bytecode.emplace_back(Instruction::Opcode::mul);
                    break;
                case TokenType::Divide:
                    ctx->bytecode.emplace_back(Instruction::Opcode::div);
                    break;
                case TokenType::Mod:
                    ctx->bytecode.emplace_back(Instruction::Opcode::mod);
                    break;
                case TokenType::Power:
                    ctx->bytecode.emplace_back(Instruction::Opcode::pow);
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
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, std::stoi(constant->token.content)));
                else
                    ctx->bytecode.push_back(Instruction::make_double(Instruction::Opcode::pushd, std::stod(constant->token.content)));
                break;
            case TokenType::Percentage:
                if (constant->token.content.find('.') == std::string::npos)
                    ctx->bytecode.push_back(Instruction::make_double(Instruction::Opcode::pushd, std::stoi(constant->token.content) / 100.0));
                else
                    ctx->bytecode.push_back(Instruction::make_double(Instruction::Opcode::pushd, std::stod(constant->token.content) / 100.0));
                break;
            case TokenType::String: // todo: add default setting to project file?
            case TokenType::ExcludeString:
                if (constant->nodes.size() == 0)
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushbs, ctx->string(constant->token.content)));
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushbints, ctx->string(constant->token.content), constant->nodes.size()));
                }
                break;
            case TokenType::MarkedString:
                if (constant->nodes.size() == 0)
                {
                    ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushs, translationInfo(ctx, constant->token.content)));
                }
                else
                {
                    for (auto it = constant->nodes.rbegin(); it != constant->nodes.rend(); ++it)
                        GenerateExpression(*it, ctx, res);
                    ctx->bytecode.push_back(Instruction::make_int2(Instruction::Opcode::pushints, translationInfo(ctx, constant->token.content), constant->nodes.size()));
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
        case Node::NodeType::Variable:
        {
            NodeContent* var = (NodeContent*)expr;
            auto it = std::find(ctx->localStack.begin(), ctx->localStack.end(), var->content);
            if (it == ctx->localStack.end())
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarloc, std::distance(ctx->localStack.begin(), it)));

            // Array accesses
            while (i < expr->nodes.size())
            {
                GenerateExpression(expr->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
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
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarloc, localId));
            }

            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
            }

            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(expr->type == Node::NodeType::ExprPreIncrement ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);
            if (i == 0)
                ctx->bytecode.emplace_back(Instruction::Opcode::dup);
            else
                ctx->bytecode.emplace_back(Instruction::Opcode::save);

            for (int j = 0; j < i; j++)
                ctx->bytecode.emplace_back(Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarloc, localId));

            if (i != 0)
                ctx->bytecode.emplace_back(Instruction::Opcode::load);
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
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarglb, ctx->string(var->content)));
            }
            else
            {
                localId = std::distance(ctx->localStack.begin(), it);
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushvarloc, localId));
            }

            while (i < var->nodes.size())
            {
                GenerateExpression(var->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(Instruction::Opcode::dup2);
                ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
            }

            if (i == 0)
                ctx->bytecode.emplace_back(Instruction::Opcode::dup);
            else
                ctx->bytecode.emplace_back(Instruction::Opcode::save);

            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::pushi, 1));
            ctx->bytecode.emplace_back(expr->type == Node::NodeType::ExprPostIncrement ?
                                        Instruction::Opcode::add :
                                        Instruction::Opcode::sub);

            for (int j = 0; j < i; j++)
                ctx->bytecode.emplace_back(Instruction::Opcode::setarrind);

            if (localId == -1)
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarglb, ctx->string(var->content)));
            else
                ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::setvarloc, localId));

            if (i != 0)
                ctx->bytecode.emplace_back(Instruction::Opcode::load);
            break;
        }
        case Node::NodeType::ExprAccessArray:
        {
            GenerateExpression(expr->nodes.at(i++), ctx, res);
            while (i < expr->nodes.size())
            {
                GenerateExpression(expr->nodes.at(i++), ctx, res);
                ctx->bytecode.emplace_back(Instruction::Opcode::pusharrind);
            }
            break;
        }
        case Node::NodeType::SceneFunction:
        {
            for (auto it = expr->nodes.rbegin(); it != expr->nodes.rend(); ++it)
                GenerateExpression(*it, ctx, res);
            ctx->bytecode.push_back(Instruction::make_int(Instruction::Opcode::PATCH_CALL, ctx->string(((NodeContent*)expr)->content)));
            break;
        }
        }
    }
}