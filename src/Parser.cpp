#include "Parser.h"

namespace diannex
{
    /*
        Base parser
    */

    ParseResult Parser::ParseTokens(std::vector<Token>* tokens)
    {
        Parser parser = Parser(tokens);
        parser.skipNewlines();
        std::shared_ptr<Node> block(Node::ParseGroupBlock(&parser, false));
        return { block, parser.errors };
    }

    Parser::Parser(std::vector<Token>* tokens)
    {
        this->tokens = tokens;
        tokenCount = tokens->size();
        position = 0;
        storedPosition = 0;
        errors = std::vector<ParseError>();
    }

    void Parser::advance()
    {
        position++;
    }

    void Parser::synchronize()
    {
        advance();
        while (isMore())
        {
            TokenType type = peekToken().type;
            if (type == TokenType::Semicolon || type == TokenType::Identifier 
                || type == TokenType::ModifierKeyword || type == TokenType::MainKeyword
                || type == TokenType::GroupKeyword || type == TokenType::MainSubKeyword)
                break;
            advance();
        }
    }

    void Parser::storePosition()
    {
        storedPosition = position;
    }

    void Parser::restorePosition()
    {
        position = storedPosition;
    }

    bool Parser::isMore()
    {
        return position < tokenCount;
    }

    void Parser::skipNewlines()
    {
        while (isMore() && peekToken().type == TokenType::Newline)
            advance();
    }

    void Parser::skipSemicolons()
    {
        if (isMore())
        {
            if (Token t = peekToken(); t.type == TokenType::Semicolon)
            {
                do
                {
                    advance();
                    skipNewlines();
                } while (isMore() && (t = peekToken()).type == TokenType::Semicolon);
                skipNewlines();
            }
        }
    }

    bool Parser::isNextToken(TokenType type)
    {
        return tokens->at(position).type == type;
    }

    Token Parser::previousToken()
    {
        return tokens->at(position - 1);
    }

    Token Parser::peekToken()
    {
        return tokens->at(position);
    }

    Token Parser::ensureToken(TokenType type)
    {
        if (position == tokenCount)
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, 0, 0, tokenToString(Token(type, 0, 0)) });
            return Token(TokenType::Error, 0, 0);
        }

        Token t = tokens->at(position);
        advance();
        if (t.type == type)
            return t;
        else
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column, tokenToString(Token(type, 0, 0)), tokenToString(t) });
            return Token(TokenType::Error, 0, 0);
        }
    }

    Token Parser::ensureToken(TokenType type, TokenType type2)
    {
        if (position == tokenCount)
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, 0, 0, tokenToString(Token(type, 0, 0)) });
            return Token(TokenType::Error, 0, 0);
        }

        Token t = tokens->at(position);
        advance();
        if (t.type == type || t.type == type2)
            return t;
        else
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column, tokenToString(Token(type, 0, 0)), tokenToString(t) });
            return Token(TokenType::Error, 0, 0);
        }
    }

    Token Parser::ensureToken(TokenType type, KeywordType keywordType)
    {
        if (position == tokenCount)
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, 0, 0, tokenToString(Token(type, 0, 0, keywordType)) });
            return Token(TokenType::Error, 0, 0);
        }

        Token t = tokens->at(position);
        advance();
        if (t.type == type && t.keywordType == keywordType)
            return t;
        else
        {
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column,
                                tokenToString(Token(type, 0, 0, keywordType)), tokenToString(t) });
            return Token(TokenType::Error, 0, 0);
        }
    }

    /*
        General-purpose nodes
    */

    Node::Node(NodeType type)
        : type(type)
    {
        this->nodes = std::vector<Node*>();
    }

    Node::~Node()
    {
        for (Node* s : nodes)
            delete s;
    }

    NodeContent::NodeContent(std::string content, NodeType type) : Node(type), content(content)
    {
    }

    /*
        Group statements
    */

    Node* Node::ParseGroupBlock(Parser* parser, bool isNamespace)
    {
        Node* res = new Node(NodeType::Block);

        if (isNamespace)
            parser->ensureToken(TokenType::OpenCurly);

        parser->skipNewlines();

        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseGroupStatement(parser, KeywordType::None));
            parser->skipNewlines();
        }

        parser->skipNewlines();

        if (isNamespace)
            parser->ensureToken(TokenType::CloseCurly);

        return res;
    }

    Node* Node::ParseNamespaceBlock(Parser* parser, std::string name)
    {
        Node* res = Node::ParseGroupBlock(parser, true);
        res->type = NodeType::Namespace;
        return res;
    }

    Node* Node::ParseGroupStatement(Parser* parser, KeywordType modifier)
    {
        Token t = parser->peekToken();
        switch (t.type)
        {
        case TokenType::GroupKeyword:
            {
                parser->advance();
                parser->skipNewlines();
                Token name = parser->ensureToken(TokenType::Identifier);
                parser->skipNewlines();
                if (name.type != TokenType::Error)
                {
                    if (t.keywordType != KeywordType::Func)
                    {
                        if (modifier != KeywordType::None)
                            parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
                        switch (t.keywordType)
                        {
                        case KeywordType::Namespace:
                            return Node::ParseNamespaceBlock(parser, name.content);
                        case KeywordType::Scene:
                            return Node::ParseSceneBlock(parser, name.content);
                        case KeywordType::Def:
                            return Node::ParseDefinitionBlock(parser, name.content);
                        }
                    }
                    else
                    {
                        // Parse functions TODO
                    }
                }
                else
                {
                    parser->errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column,
                                                tokenToString(Token(TokenType::Identifier, 0, 0)), tokenToString(name) });
                    parser->synchronize();
                }
            }
            break;

        case TokenType::ModifierKeyword:
            parser->advance();
            parser->skipNewlines();
            return Node::ParseGroupStatement(parser, t.keywordType);

        case TokenType::MarkedComment:
            if (modifier != KeywordType::None)
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
            parser->advance();
            return new NodeContent(t.content, NodeType::MarkedComment);

        default:
            parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
            parser->synchronize();
            break;
        }

        return nullptr;
    }

    /*
        Scene/function statements
    */

    Node* Node::ParseSceneBlock(Parser* parser)
    {
        Node* res = new Node(NodeType::SceneBlock);

        parser->ensureToken(TokenType::OpenCurly);

        parser->skipNewlines();

        parser->skipSemicolons();
        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
            parser->skipSemicolons();
            parser->skipNewlines();
        }

        parser->ensureToken(TokenType::CloseCurly);

        return res;
    }

    Node* Node::ParseSceneBlock(Parser* parser, std::string name)
    {
        NodeContent* res = new NodeContent(name, NodeType::Scene);

        parser->ensureToken(TokenType::OpenCurly);

        parser->skipNewlines();

        parser->skipSemicolons();
        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
            parser->skipSemicolons();
            parser->skipNewlines();
        }

        parser->ensureToken(TokenType::CloseCurly);

        return res;
    }

    Node* Node::ParseSceneStatement(Parser* parser, KeywordType modifier)
    {
        Token t = parser->peekToken();

        if (t.type == TokenType::VariableStart)
        {
            parser->advance();
            parser->skipNewlines();
            // todo parse variable assign (and other variants like +=), increment, or decrement
        }
        else
        {
            if (modifier != KeywordType::None)
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
            switch (t.type)
            {
            case TokenType::Identifier:
                if (parser->peekToken().type == TokenType::Colon)
                {
                    parser->advance();
                    // todo parse shorthand char call
                }
                else
                {
                    return Node::ParseFunction(parser, false);
                }
                break;
            case TokenType::String:
            case TokenType::ExcludeString:
            case TokenType::MarkedString:
                parser->advance();
                parser->skipNewlines();
                if (parser->peekToken().type == TokenType::Colon)
                {
                    // todo parse shorthand char call
                }
                else
                {
                    if (t.type == TokenType::MarkedString)
                        parser->errors.push_back({ ParseError::ErrorType::UnexpectedMarkedString, t.line, t.column });
                    return new NodeTextRun(t.content, t.type == TokenType::ExcludeString);
                }
                break;
            case TokenType::MainKeyword:
                switch (t.keywordType)
                {
                case KeywordType::Choice:
                    break;
                case KeywordType::Choose:
                    break;
                case KeywordType::If:
                {
                    parser->advance();
                    Node* condition = Node::ParseExpression(parser);
                    parser->skipNewlines();
                    Node* trueBranch = Node::ParseSceneStatement(parser, KeywordType::None);
                    Node* res = new Node(NodeType::If);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(trueBranch);
                    parser->skipNewlines();
                    if (parser->isMore())
                    {
                        if (Token t = parser->peekToken(); t.type == TokenType::MainKeyword && t.keywordType == KeywordType::Else)
                        {
                            parser->advance();
                            res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                        }
                    }
                    return res;
                }
                case KeywordType::Else:
                    parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
                    parser->synchronize();
                    break;
                case KeywordType::While:
                    break;
                case KeywordType::For:
                    break;
                case KeywordType::Do:
                    break;
                case KeywordType::Repeat:
                    break;
                case KeywordType::Switch:
                    break;
                case KeywordType::Continue:
                    break;
                case KeywordType::Break:
                    break;
                case KeywordType::Return:
                    break;
                }
                break;
            case TokenType::Increment:
            {
                Node* res = new Node(NodeType::Increment);
                parser->advance();
                parser->skipNewlines();
                Node* val = Node::ParseVariable(parser);
                res->nodes.push_back(val);
                return res;
            }
            case TokenType::Decrement:
            {
                Node* res = new Node(NodeType::Decrement);
                parser->advance();
                parser->skipNewlines();
                Node* val = Node::ParseVariable(parser);
                res->nodes.push_back(val);
                return res;
            }
            case TokenType::ModifierKeyword:
                parser->advance();
                parser->skipNewlines();
                return Node::ParseSceneStatement(parser, t.keywordType);
            case TokenType::MarkedComment:
                parser->advance();
                return new NodeContent(t.content, NodeType::MarkedComment);
            case TokenType::OpenCurly:
                return Node::ParseSceneBlock(parser);
            case TokenType::Semicolon:
                parser->advance();
                return new Node(NodeType::None);
            default:
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
                parser->synchronize();
                break;
            }
        }
        return nullptr;
    }

    Node* Node::ParseVariable(Parser* parser)
    {
        parser->ensureToken(TokenType::VariableStart);
        Token name = parser->ensureToken(TokenType::Identifier);
        if (name.type != TokenType::Error)
        {
            NodeContent* res = new NodeContent(name.content, NodeType::Variable);

            parser->skipNewlines();
            while (parser->isMore() && parser->peekToken().type == TokenType::OpenBrack)
            {
                parser->advance();
                res->nodes.push_back(Node::ParseExpression(parser));
                parser->skipNewlines();
                parser->ensureToken(TokenType::CloseBrack);
                parser->skipNewlines();
            }

            return res;
        }
        return nullptr;
    }

    Node* Node::ParseFunction(Parser* parser, bool parentheses)
    {
        Token name = parser->ensureToken(TokenType::Identifier);
        if (name.type != TokenType::Error)
        {
            NodeContent* res = new NodeContent(name.content, NodeType::SceneFunction);

            if (parentheses)
            {
                parser->skipNewlines();
                parser->ensureToken(TokenType::OpenParen);
                parser->skipNewlines();
            }
            else
            {
                if (parser->peekToken().type == TokenType::OpenParen)
                {
                    // We have to check if this call is a command or not
                    // To do this, we check the balance of parentheses, and after the last one, if there is NOT a comma, it's a function
                    parser->storePosition();
                    parser->advance();
                    Token curr = parser->peekToken();
                    int depth = 1;
                    while (parser->isMore() && depth != 0 && curr.type != TokenType::Newline && curr.type != TokenType::Semicolon)
                    {
                        if (curr.type == TokenType::OpenParen)
                            depth++;
                        else if (curr.type == TokenType::CloseParen)
                            depth--;
                        parser->advance();
                        curr = parser->peekToken();
                    }
                    parser->skipNewlines();
                    if (parser->peekToken().type != TokenType::Comma)
                    {
                        parentheses = true;
                    }
                    parser->restorePosition();
                    if (parentheses)
                    {
                        parser->skipNewlines();
                        parser->ensureToken(TokenType::OpenParen);
                        parser->skipNewlines();
                    }
                }
            }

            if (parentheses)
            {
                Token t = parser->peekToken();
                while (parser->isMore() && t.type != TokenType::CloseParen)
                {
                    res->nodes.push_back(Node::ParseExpression(parser));
                    parser->skipNewlines();
                    if (parser->isMore())
                    {
                        t = parser->peekToken();
                        if (t.type != TokenType::CloseParen)
                        {
                            parser->advance();
                            parser->skipNewlines();
                            if (t.type != TokenType::Comma)
                            {
                                parser->errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column, tokenToString(Token(TokenType::Comma, 0, 0)), tokenToString(t) });
                                break;
                            }
                        }
                    }
                }
                parser->ensureToken(TokenType::CloseParen);
            }
            else
            {
                Token t = parser->peekToken();
                while (parser->isMore() && t.type != TokenType::Newline && t.type != TokenType::Semicolon)
                {
                    res->nodes.push_back(Node::ParseExpression(parser));
                    if (parser->isMore())
                    {
                        t = parser->peekToken();
                        if (t.type != TokenType::Newline && t.type != TokenType::Semicolon)
                        {
                            if (parser->previousToken().type == TokenType::Newline)
                                break;
                            parser->advance();
                            if (t.type != TokenType::Comma)
                            {
                                parser->errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column, tokenToString(Token(TokenType::Comma, 0, 0)), tokenToString(t) });
                                break;
                            }
                        }
                    }
                }
            }

            return res;
        }
        return nullptr;
    }

    Node* Node::ParseExpression(Parser* parser)
    {
        parser->skipNewlines();
        Node* res = Node::ParseConditional(parser);

        // Array parse
        parser->skipNewlines();
        while (parser->isMore() && parser->peekToken().type == TokenType::OpenBrack)
        {
            parser->advance();
            res->nodes.push_back(Node::ParseExpression(parser));
            parser->skipNewlines();
            parser->ensureToken(TokenType::CloseBrack);
            parser->skipNewlines();
        }

        return res;
    }

    Node* Node::ParseConditional(Parser* parser)
    {
        Node* left = Node::ParseOr(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::Ternary)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprTernary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseExpression(parser));
                parser->skipNewlines();
                parser->ensureToken(TokenType::Colon);
                res->nodes.push_back(Node::ParseExpression(parser));

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseOr(Parser* parser)
    {
        Node* left = Node::ParseAnd(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::LogicalOr)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseExpression(parser));

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseAnd(Parser* parser)
    {
        Node* left = Node::ParseCompare(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::LogicalAnd)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseExpression(parser));

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseCompare(Parser* parser)
    {
        Node* left = Node::ParseBitwise(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::CompareEQ ||
                t.type == TokenType::CompareGT ||
                t.type == TokenType::CompareGTE ||
                t.type == TokenType::CompareLT ||
                t.type == TokenType::CompareLTE ||
                t.type == TokenType::CompareNEQ)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseBitwise(parser));

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseBitwise(Parser* parser)
    {
        Node* left = Node::ParseBitShift(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::BitwiseOr ||
                t.type == TokenType::BitwiseAnd ||
                t.type == TokenType::BitwiseXor)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseBitShift(parser));

                // Check for additional operations with the same precedence
                parser->skipNewlines();
                t = parser->peekToken();
                while (t.type == TokenType::BitwiseOr ||
                    t.type == TokenType::BitwiseAnd ||
                    t.type == TokenType::BitwiseXor)
                {
                    parser->advance();

                    Node* next = new NodeToken(NodeType::ExprBinary, t);
                    next->nodes.push_back(res);
                    next->nodes.push_back(Node::ParseBitShift(parser));
                    res = next;
                }

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseBitShift(Parser* parser)
    {
        Node* left = Node::ParseAddSub(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::BitwiseLShift ||
                t.type == TokenType::BitwiseRShift)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseAddSub(parser));

                // Check for additional operations with the same precedence
                parser->skipNewlines();
                t = parser->peekToken();
                while (t.type == TokenType::BitwiseLShift ||
                       t.type == TokenType::BitwiseRShift)
                {
                    parser->advance();

                    Node* next = new NodeToken(NodeType::ExprBinary, t);
                    next->nodes.push_back(res);
                    next->nodes.push_back(Node::ParseAddSub(parser));
                    res = next;
                }

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseAddSub(Parser* parser)
    {
        Node* left = Node::ParseMulDiv(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::Plus ||
                t.type == TokenType::Minus)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseMulDiv(parser));

                // Check for additional operations with the same precedence
                parser->skipNewlines();
                t = parser->peekToken();
                while (t.type == TokenType::Plus ||
                    t.type == TokenType::Minus)
                {
                    parser->advance();

                    Node* next = new NodeToken(NodeType::ExprBinary, t);
                    next->nodes.push_back(res);
                    next->nodes.push_back(Node::ParseMulDiv(parser));
                    res = next;
                }

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseMulDiv(Parser* parser)
    {
        Node* left = Node::ParseExprLast(parser);
        parser->skipNewlines();
        if (parser->isMore())
        {
            Token t = parser->peekToken();
            if (t.type == TokenType::Multiply ||
                t.type == TokenType::Divide ||
                t.type == TokenType::Mod ||
                t.type == TokenType::Power)
            {
                parser->advance();

                Node* res = new NodeToken(NodeType::ExprBinary, t);
                res->nodes.push_back(left);
                res->nodes.push_back(Node::ParseExprLast(parser));

                // Check for additional operations with the same precedence
                parser->skipNewlines();
                t = parser->peekToken();
                while (t.type == TokenType::Multiply ||
                       t.type == TokenType::Divide ||
                       t.type == TokenType::Mod ||
                       t.type == TokenType::Power)
                {
                    parser->advance();

                    Node* next = new NodeToken(NodeType::ExprBinary, t);
                    next->nodes.push_back(res);
                    next->nodes.push_back(Node::ParseExprLast(parser));
                    res = next;
                }

                return res;
            }
        }
        return left;
    }

    Node* Node::ParseExprLast(Parser* parser)
    {
        parser->skipNewlines();

        if (parser->isMore())
        {
            Token t = parser->peekToken();
            switch (t.type)
            {
            case TokenType::Number:
            case TokenType::String:
            case TokenType::MarkedString:
            case TokenType::ExcludeString:
                parser->advance();
                return new NodeToken(NodeType::ExprConstant, t);
            case TokenType::VariableStart:
            {
                Node* val = Node::ParseVariable(parser);
                parser->skipNewlines();
                t = parser->peekToken();
                if (t.type == TokenType::Increment)
                {
                    parser->advance();
                    Node* res = new Node(NodeType::ExprPostIncrement);
                    res->nodes.push_back(val);
                    return res;
                }
                else if (t.type == TokenType::Decrement)
                {
                    parser->advance();
                    Node* res = new Node(NodeType::ExprPostDecrement);
                    res->nodes.push_back(val);
                    return res;
                }
                return val;
            }
            case TokenType::Not:
            {
                parser->advance();
                parser->skipNewlines();
                Node* expr = Node::ParseExprLast(parser);
                Node* res = new Node(NodeType::ExprNot);
                res->nodes.push_back(expr);
                return res;
            }
            case TokenType::Minus:
            {
                parser->advance();
                parser->skipNewlines();
                Node* expr = Node::ParseExprLast(parser);
                Node* res = new Node(NodeType::ExprNegate);
                res->nodes.push_back(expr);
                return res;
            }
            case TokenType::BitwiseNegate:
            {
                parser->advance();
                parser->skipNewlines();
                Node* expr = Node::ParseExprLast(parser);
                Node* res = new Node(NodeType::ExprBitwiseNegate);
                res->nodes.push_back(expr);
                return res;
            }
            case TokenType::OpenParen:
            {
                parser->advance();
                parser->skipNewlines();
                Node* expr = Node::ParseExpression(parser);
                parser->skipNewlines();
                parser->ensureToken(TokenType::CloseParen);
                return expr;
            }
            case TokenType::OpenBrack:
            {
                parser->advance();
                parser->skipNewlines();
                Node* res = new Node(NodeType::ExprArray);
                t = parser->peekToken();
                while (parser->isMore() && t.type != TokenType::CloseBrack)
                {
                    res->nodes.push_back(Node::ParseExpression(parser));
                    parser->skipNewlines();
                    if (parser->isMore())
                    {
                        t = parser->peekToken();
                        if (t.type != TokenType::CloseBrack)
                        {
                            parser->advance();
                            parser->skipNewlines();
                            if (t.type != TokenType::Comma)
                            {
                                parser->errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column, tokenToString(Token(TokenType::Comma, 0, 0)), tokenToString(t) });
                                break;
                            }
                        }
                    }
                }
                parser->ensureToken(TokenType::CloseBrack);
                return res;
            }
            case TokenType::Increment:
            {
                Node* res = new Node(NodeType::ExprPreIncrement);
                parser->advance();
                parser->skipNewlines();
                Node* val = Node::ParseVariable(parser);
                res->nodes.push_back(val);
                return res;
            }
            case TokenType::Decrement:
            {
                Node* res = new Node(NodeType::ExprPreDecrement);
                parser->advance();
                parser->skipNewlines();
                Node* val = Node::ParseVariable(parser);
                res->nodes.push_back(val);
                return res;
            }
            case TokenType::Identifier:
            {
                return Node::ParseFunction(parser);
            }
            }

            parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
            return nullptr;
        }

        parser->errors.push_back({ ParseError::ErrorType::UnexpectedEOF });
        return nullptr;
    }

    NodeTextRun::NodeTextRun(std::string content, bool excludeTranslation) 
        : NodeContent(content, NodeType::TextRun), excludeTranslation(excludeTranslation)
    {
    }

    NodeToken::NodeToken(NodeType type, Token token) 
        : Node(type), token(token)
    {
    }

    /*
        Definitions statements
    */

    Node* Node::ParseDefinitionBlock(Parser* parser, std::string name)
    {
        NodeContent* res = new NodeContent(name, NodeType::Definitions);

        parser->ensureToken(TokenType::OpenCurly);

        parser->skipNewlines();

        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseDefinitionStatement(parser));
            parser->skipNewlines();
        }

        parser->ensureToken(TokenType::CloseCurly);

        return res;
    }

    Node* Node::ParseDefinitionStatement(Parser* parser)
    {
        Token t = parser->peekToken();
        switch (t.type)
        {
        case TokenType::Identifier:
            parser->advance();
            parser->skipNewlines();
            if (parser->ensureToken(TokenType::Equals).type != TokenType::Error)
            {
                Token val = parser->ensureToken(TokenType::String, TokenType::ExcludeString);
                if (val.type != TokenType::Error)
                    return new NodeDefinition(t.content, val.content, val.type != TokenType::String);
            }
            break;
        case TokenType::MarkedComment:
            parser->advance();
            return new NodeContent(t.content, NodeType::MarkedComment);
        default:
            parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
            parser->synchronize();
            break;
        }

        return nullptr;
    }

    NodeDefinition::NodeDefinition(std::string key, std::string value, bool excludeValueTranslation) 
        : Node(NodeType::Definition), key(key), value(value), excludeValueTranslation(excludeValueTranslation)
    {
    }
}