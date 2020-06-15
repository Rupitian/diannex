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
        this->position = 0;
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

    bool Parser::isMore()
    {
        return position < tokenCount;
    }

    void Parser::skipNewlines()
    {
        while (isMore() && peekToken().type == TokenType::Newline)
            advance();
    }

    bool Parser::isNextToken(TokenType type)
    {
        return tokens->at(position).type == type;
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

        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
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

        while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
        {
            res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
            parser->skipNewlines();
        }

        parser->ensureToken(TokenType::CloseCurly);

        return res;
    }

    Node* Node::ParseSceneStatement(Parser* parser, KeywordType modifier)
    {
        Token t = parser->peekToken();

        // First, skip semicolons
        if (t.type == TokenType::Semicolon)
        {
            do
            {
                parser->advance();
            } while (parser->isMore() && (t = parser->peekToken()).type == TokenType::Semicolon);
        }

        if (t.type == TokenType::VariableStart)
        {
            // todo parse variable assign (and other variants like +=), increment, or decrement
        }
        else
        {
            if (modifier != KeywordType::None)
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
            switch (t.type)
            {
            case TokenType::Identifier:
                parser->advance();
                if (parser->peekToken().type == TokenType::Colon)
                {
                    // todo parse shorthand char call
                }
                else
                {
                    // todo parse function call
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
                    Node* block = Node::ParseSceneBlock(parser);
                    Node* res = new Node(NodeType::If);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(block);
                    return res;
                }
                case KeywordType::Else:
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
                // todo
                break;
            case TokenType::Decrement:
                // todo
                break;
            case TokenType::ModifierKeyword:
                parser->advance();
                parser->skipNewlines();
                return Node::ParseSceneStatement(parser, t.keywordType);
            case TokenType::MarkedComment:
                parser->advance();
                return new NodeContent(t.content, NodeType::MarkedComment);
            default:
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
                parser->synchronize();
                break;
            }
        }
        return nullptr;
    }

    Node* Node::ParseExpression(Parser* parser)
    {
        parser->skipNewlines();
        return Node::ParseConditional(parser);
    }

    Node* Node::ParseConditional(Parser* parser)
    {
        Node* left = Node::ParseOr(parser);
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
                parser->advance();
                if (parser->isMore())
                {
                    t = parser->ensureToken(TokenType::Identifier);
                    if (t.type != TokenType::Error)
                    {
                        // todo call ParseSingleVar function here
                    }
                }
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedEOF });
                return nullptr;
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
                // todo
                return nullptr;
            }
            case TokenType::Increment:
            case TokenType::Decrement:
            {
                // todo
                return nullptr;
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