#include "Parser.h"

#include <sstream>

namespace diannex
{
    /*
        Base parser
    */

    ParseResult* Parser::ParseTokens(CompileContext* ctx, std::vector<Token>* tokens)
    {
        Parser parser = Parser(ctx, tokens);
        parser.skipNewlines();
        return new ParseResult { Node::ParseGroupBlock(&parser, false), parser.errors };
    }

    ParseResult Parser::ParseTokensExpression(CompileContext* ctx, std::vector<Token>* tokens, uint32_t defaultLine, uint16_t defaultColumn)
    {
        Parser parser = Parser(ctx, tokens);
        parser.defaultLine = defaultLine;
        parser.defaultColumn = defaultColumn;
        parser.skipNewlines();
        return { Node::ParseExpression(&parser), parser.errors };
    }

    std::string Parser::ProcessStringInterpolation(Parser* parser, Token& token, const std::string& input, std::vector<class Node*>* nodeList)
    {
        std::string interpStr = parser->context->project->options.interpolationFlags.symbol;
        if (interpStr.empty())
            return input;
        char interpChar = interpStr.at(0);

        // Build the new string result as well as parse expressions
        std::stringstream ss(std::ios_base::app | std::ios_base::out);
        int pos = 0, len = input.length();
        int interpCount = 0;
        int line = token.line, col = token.column + ((token.type == TokenType::String) ? 1 : 2);
        while (pos < len)
        {
            char curr = input.at(pos);
            if (curr == interpChar && pos + 1 < len && input.at(pos + 1) == '{')
            {
                bool parse = false;
                if (pos != 0)
                {
                    if (input.at(pos - 1) != '\\')
                        parse = true;
                    else
                        ss << curr;
                }

                if (parse || pos == 0)
                {
                    // Extract the substring
                    pos += 2;
                    col += 2;
                    int count = 0;
                    int tempLine = line, tempCol = col;
                    int startPos = pos;
                    while (pos < len && input.at(pos) != '}')
                    {
                        pos++;
                        count++;
                        if (input.at(pos) == '\n')
                        {
                            tempLine++;
                            tempCol = 0;
                        }
                        else
                            tempCol++;
                    }
                    std::string exprStr = input.substr(startPos, count);

                    // Parse expression and add to nodes
                    std::vector<Token> tokens;
                    Lexer::LexString(exprStr, parser->context, tokens, line, col);
                    ParseResult parsed = Parser::ParseTokensExpression(parser->context, &tokens, line, col);
                    if (parsed.errors.size() != 0)
                        parser->errors.insert(parser->errors.end(), parsed.errors.begin(), parsed.errors.end());
                    else
                    {
                        nodeList->push_back(parsed.baseNode);
                        parsed.doDelete = false;
                    }

                    // Also add the proper string representation
                    ss << interpChar << "{" << interpCount++ << "}";
                    line = tempLine;
                    col = tempCol + 1;
                }
            }
            else
            {
                if (curr == '\n')
                {
                    line++;
                    col = 0;
                }
                else
                    col++;
                ss << curr;
            }
            pos++;
        }
        
        return ss.str();
    }

    Parser::Parser(CompileContext* ctx, std::vector<Token>* tokens)
        : context(ctx), tokens(tokens)
    {
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
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, defaultLine, defaultColumn, tokenToString(Token(type, 0, 0)) });
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
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, defaultLine, defaultColumn, tokenToString(Token(type, 0, 0)) });
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
            errors.push_back({ ParseError::ErrorType::ExpectedTokenButEOF, defaultLine, defaultColumn, tokenToString(Token(type, 0, 0, keywordType)) });
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

    NodeTextRun::NodeTextRun(std::string content, bool excludeTranslation)
        : NodeContent(content, NodeType::TextRun), excludeTranslation(excludeTranslation)
    {
    }

    NodeToken::NodeToken(NodeType type, Token token)
        : Node(type), token(token)
    {
    }

    NodeTokenModifier::NodeTokenModifier(NodeType type, Token token, KeywordType modifier)
        : Node(type), token(token), modifier(modifier)
    {
    }

    NodeFunc::NodeFunc(std::string name, KeywordType modifier)
        : Node(NodeType::Function), name(name), modifier(modifier)
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
                        return Node::ParseFunctionBlock(parser, name.content, modifier);
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

    Node* Node::ParseFunctionBlock(Parser* parser, std::string name, KeywordType modifier)
    {
        NodeFunc* res = new NodeFunc(name, modifier);

        // Parse arguments
        parser->ensureToken(TokenType::OpenParen);

        parser->skipNewlines();
        while (parser->isMore() && !parser->isNextToken(TokenType::CloseParen))
        {
            res->args.push_back(parser->ensureToken(TokenType::Identifier));
            parser->skipNewlines();
            if (parser->isNextToken(TokenType::Comma))
            {
                parser->advance();
                parser->skipNewlines();
            }
        }

        parser->ensureToken(TokenType::CloseParen);

        parser->skipNewlines();

        // Parse block
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

    Node* Node::ParseSceneStatement(Parser* parser, KeywordType modifier, bool inSwitch)
    {
        Token t = parser->peekToken();

        if (t.type == TokenType::VariableStart)
        {
            Node* variable = Node::ParseVariable(parser);
            parser->skipNewlines();
            t = parser->peekToken();
            switch (t.type)
            {
            case TokenType::Increment:
            {
                if (modifier != KeywordType::None)
                    parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
                Node* res = new Node(NodeType::Increment);
                parser->advance();
                res->nodes.push_back(variable);
                return res;
            }
            case TokenType::Decrement:
            {
                if (modifier != KeywordType::None)
                    parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
                Node* res = new Node(NodeType::Decrement);
                parser->advance();
                res->nodes.push_back(variable);
                return res;
            }
            case TokenType::PlusEquals:
            case TokenType::MinusEquals:
            case TokenType::MultiplyEquals:
            case TokenType::DivideEquals:
            case TokenType::ModEquals:
            case TokenType::BitwiseAndEquals:
            case TokenType::BitwiseOrEquals:
            case TokenType::BitwiseXorEquals:
                if (modifier != KeywordType::None)
                    parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
            case TokenType::Equals:
            {
                NodeTokenModifier* res = new NodeTokenModifier(NodeType::Assign, t, modifier);
                res->nodes.push_back(variable);

                parser->advance();
                res->nodes.push_back(Node::ParseExpression(parser));

                return res;
            }
            default:
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
                parser->synchronize();
                break;
            }
        }
        else
        {
            if (modifier != KeywordType::None)
                parser->errors.push_back({ ParseError::ErrorType::UnexpectedModifierFor, t.line, t.column, tokenToString(t) });
            switch (t.type)
            {
            case TokenType::Identifier:
                parser->storePosition();
                parser->advance();
                parser->skipNewlines();
                if (parser->isNextToken(TokenType::Colon))
                {
                    parser->advance();
                    parser->skipNewlines();
                    Node* res = new NodeToken(NodeType::ShorthandChar, t);
                    res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                    return res;
                }
                else
                {
                    parser->restorePosition();
                    return Node::ParseFunction(parser, false);
                }
                break;
            case TokenType::String:
            case TokenType::ExcludeString:
            case TokenType::MarkedString:
                parser->advance();
                parser->skipNewlines();
                if (parser->isNextToken(TokenType::Colon))
                {
                    parser->advance();
                    parser->skipNewlines();
                    NodeToken* res = new NodeToken(NodeType::ShorthandChar, t);
                    res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                    res->token.content = Parser::ProcessStringInterpolation(parser, t, t.content, &res->nodes);
                    return res;
                }
                else
                {
                    if (t.type == TokenType::MarkedString)
                        parser->errors.push_back({ ParseError::ErrorType::UnexpectedMarkedString, t.line, t.column });
                    NodeTextRun* res = new NodeTextRun(t.content, t.type == TokenType::ExcludeString);
                    res->content = Parser::ProcessStringInterpolation(parser, t, t.content, &res->nodes);
                    return res;
                }
                break;
            case TokenType::MainKeyword:
                switch (t.keywordType)
                {
                case KeywordType::Choice:
                {
                    parser->advance();
                    parser->skipNewlines();

                    Node* res = new Node(NodeType::Choice);

                    // Check for statement/text run
                    Token next = parser->peekToken();
                    switch (next.type)
                    {
                    case TokenType::String:
                    case TokenType::ExcludeString:
                    case TokenType::MarkedString:
                    {
                        if (t.type == TokenType::MarkedString)
                            parser->errors.push_back({ ParseError::ErrorType::UnexpectedMarkedString, t.line, t.column });
                        NodeTextRun* text = new NodeTextRun(next.content, next.type == TokenType::ExcludeString);
                        text->content = Parser::ProcessStringInterpolation(parser, t, t.content, &text->nodes);
                        res->nodes.push_back(text);
                        parser->advance();
                        parser->skipNewlines();
                        break;
                    }
                    case TokenType::CompareGT: // >
                        parser->advance();
                        parser->skipNewlines();
                        res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                        parser->skipNewlines();
                        break;
                    default:
                        res->nodes.push_back(new Node(NodeType::None));
                        break;
                    }

                    parser->ensureToken(TokenType::OpenCurly);
                    parser->skipNewlines();
                    while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
                    {
                        // Parse the choice text
                        switch (Token val = parser->peekToken(); val.type)
                        {
                        case TokenType::String:
                        case TokenType::MarkedString:
                        case TokenType::ExcludeString:
                        {
                            NodeToken* text = new NodeToken(NodeType::ChoiceText, val);
                            text->token.content = Parser::ProcessStringInterpolation(parser, val, val.content, &text->nodes);
                            res->nodes.push_back(text);
                            parser->advance();
                            break;
                        }
                        default:
                            res->nodes.push_back(new Node(NodeType::None));
                            break;
                        }

                        // Parse the chance
                        parser->skipNewlines();
                        switch (Token val = parser->peekToken(); val.type)
                        {
                        case TokenType::Number:
                        case TokenType::Percentage:
                            res->nodes.push_back(new NodeToken(NodeType::ExprConstant, val));
                            parser->advance();
                            break;
                        case TokenType::OpenParen:
                            res->nodes.push_back(Node::ParseExpression(parser));
                            break;
                        default:
                            res->nodes.push_back(new NodeToken(NodeType::ExprConstant, Token(TokenType::Number, 0, 0, "1")));
                            break;
                        }

                        // Parse the require
                        parser->skipNewlines();
                        t = parser->peekToken();
                        if (t.type == TokenType::MainSubKeyword && t.keywordType == KeywordType::Require)
                        {
                            parser->advance();
                            parser->skipNewlines();
                            res->nodes.push_back(Node::ParseExpression(parser));
                        }
                        else
                            res->nodes.push_back(new Node(NodeType::None));

                        // Parse the statement
                        res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                        parser->skipNewlines();
                    }
                    if (res->nodes.size() == 0)
                        parser->errors.push_back({ ParseError::ErrorType::ChoiceWithoutStatement, t.line, t.column });
                    parser->ensureToken(TokenType::CloseCurly);

                    return res;
                }
                case KeywordType::Choose:
                {
                    parser->advance();
                    parser->skipNewlines();

                    Node* res = new Node(NodeType::Choose);

                    parser->ensureToken(TokenType::OpenCurly);
                    parser->skipNewlines();
                    while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
                    {
                        // Parse the chance
                        switch (Token val = parser->peekToken(); val.type)
                        {
                        case TokenType::Number:
                        case TokenType::Percentage:
                            res->nodes.push_back(new NodeToken(NodeType::ExprConstant, val));
                            parser->advance();
                            break;
                        case TokenType::OpenParen:
                            res->nodes.push_back(Node::ParseExpression(parser));
                            break;
                        default:
                            res->nodes.push_back(new NodeToken(NodeType::ExprConstant, Token(TokenType::Number, 0, 0, "1")));
                            break;
                        }

                        // Parse the require
                        parser->skipNewlines();
                        t = parser->peekToken();
                        if (t.type == TokenType::MainSubKeyword && t.keywordType == KeywordType::Require)
                        {
                            parser->advance();
                            parser->skipNewlines();
                            res->nodes.push_back(Node::ParseExpression(parser));
                        }
                        else
                            res->nodes.push_back(new Node(NodeType::None));

                        // Parse the statement
                        res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None));
                        parser->skipNewlines();
                    }
                    if (res->nodes.size() == 0)
                        parser->errors.push_back({ ParseError::ErrorType::ChooseWithoutStatement, t.line, t.column });
                    parser->ensureToken(TokenType::CloseCurly);

                    return res;
                }
                case KeywordType::If:
                {
                    parser->advance();
                    parser->skipNewlines();
                    Node* condition = Node::ParseExpression(parser);

                    // Parse true branch
                    parser->skipNewlines();
                    Node* trueBranch = Node::ParseSceneStatement(parser, KeywordType::None);

                    Node* res = new Node(NodeType::If);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(trueBranch);

                    // Parse false branch (if present)
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
                case KeywordType::While:
                {
                    parser->advance();
                    parser->skipNewlines();
                    Node* condition = Node::ParseExpression(parser);

                    // Parse body
                    parser->skipNewlines();
                    Node* body = Node::ParseSceneStatement(parser, KeywordType::None);

                    Node* res = new Node(NodeType::While);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(body);

                    return res;
                }
                case KeywordType::For:
                {
                    parser->advance();
                    parser->skipNewlines();

                    parser->ensureToken(TokenType::OpenParen);
                    parser->skipNewlines();

                    // Initialize statement
                    Node* init = Node::ParseSceneStatement(parser, KeywordType::None);
                    parser->skipNewlines();
                    if (init->type != NodeType::None)
                    {
                        parser->ensureToken(TokenType::Semicolon);
                        parser->skipNewlines();
                    }

                    // Condition
                    Node* condition;
                    if (parser->isNextToken(TokenType::Semicolon))
                    {
                        condition = new Node(NodeType::None);
                        parser->advance();
                    }
                    else
                    {
                        condition = Node::ParseExpression(parser);
                        parser->skipNewlines();
                        parser->ensureToken(TokenType::Semicolon);
                    }
                    parser->skipNewlines();

                    // Loop statement
                    Node* loop = Node::ParseSceneStatement(parser, KeywordType::None);
                    parser->skipNewlines();

                    parser->ensureToken(TokenType::CloseParen);

                    // Loop body
                    parser->skipNewlines();
                    Node* body = Node::ParseSceneStatement(parser, KeywordType::None);

                    Node* res = new Node(NodeType::For);
                    res->nodes.push_back(init);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(loop);
                    res->nodes.push_back(body);
                    return res;
                }
                case KeywordType::Do:
                {
                    parser->advance();
                    parser->skipNewlines();

                    // Parse true branch
                    parser->skipNewlines();
                    Node* body = Node::ParseSceneStatement(parser, KeywordType::None);

                    // Parse the "while" keyword
                    parser->skipNewlines();
                    Token keyword = parser->ensureToken(TokenType::MainKeyword);
                    if (keyword.type != TokenType::Error && keyword.keywordType != KeywordType::While)
                        parser->errors.push_back({ ParseError::ErrorType::ExpectedTokenButGot, t.line, t.column,
                                                    tokenToString(Token(TokenType::MainKeyword, 0, 0, KeywordType::While)), tokenToString(keyword) });

                    parser->skipNewlines();
                    Node* condition = Node::ParseExpression(parser);

                    Node* res = new Node(NodeType::Do);
                    res->nodes.push_back(body);
                    res->nodes.push_back(condition);

                    return res;
                }
                case KeywordType::Repeat:
                {
                    parser->advance();
                    parser->skipNewlines();
                    Node* condition = Node::ParseExpression(parser);

                    // Parse body
                    parser->skipNewlines();
                    Node* body = Node::ParseSceneStatement(parser, KeywordType::None);

                    Node* res = new Node(NodeType::Repeat);
                    res->nodes.push_back(condition);
                    res->nodes.push_back(body);

                    return res;
                }
                case KeywordType::Switch:
                {
                    parser->advance();
                    parser->skipNewlines();
                    Node* value = Node::ParseExpression(parser);

                    Node* res = new Node(NodeType::Switch);
                    res->nodes.push_back(value);

                    parser->ensureToken(TokenType::OpenCurly); 
                    parser->skipNewlines();
                    parser->skipSemicolons();
                    while (parser->isMore() && !parser->isNextToken(TokenType::CloseCurly))
                    {
                        res->nodes.push_back(Node::ParseSceneStatement(parser, KeywordType::None, true));
                        parser->skipSemicolons();
                        parser->skipNewlines();
                    }
                    parser->ensureToken(TokenType::CloseCurly);

                    return res;
                }
                case KeywordType::Case:
                {
                    if (!inSwitch)
                        parser->errors.push_back({ ParseError::ErrorType::UnexpectedSwitchCase, t.line, t.column });
                    parser->advance();
                    parser->skipNewlines();
                    Node* res = new Node(NodeType::SwitchCase);
                    res->nodes.push_back(Node::ParseExpression(parser));
                    parser->ensureToken(TokenType::Colon);
                    return res;
                }
                case KeywordType::Default:
                {
                    if (!inSwitch)
                        parser->errors.push_back({ ParseError::ErrorType::UnexpectedSwitchDefault, t.line, t.column });
                    parser->advance();
                    parser->skipNewlines();
                    parser->ensureToken(TokenType::Colon);
                    return new Node(NodeType::SwitchDefault);
                }
                case KeywordType::Continue:
                {
                    parser->advance();
                    return new Node(NodeType::Continue);
                }
                case KeywordType::Break:
                {
                    parser->advance();
                    return new Node(NodeType::Break);
                }
                case KeywordType::Return:
                {
                    parser->advance();
                    Node* res = new Node(NodeType::Return);

                    // Parse expression to return (if present)
                    parser->skipNewlines();
                    if (parser->isMore())
                    {
                        if (TokenType tt = parser->peekToken().type; tt != TokenType::MainKeyword && tt != TokenType::Semicolon)
                            res->nodes.push_back(Node::ParseExpression(parser));
                    }
                    
                    return res;
                }
                default:
                    parser->errors.push_back({ ParseError::ErrorType::UnexpectedToken, t.line, t.column, tokenToString(t) });
                    parser->synchronize();
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

            // Array index parse
            parser->skipNewlines();
            while (parser->isMore() && parser->isNextToken(TokenType::OpenBrack))
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
                if (parser->isNextToken(TokenType::OpenParen))
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
                    if (!parser->isNextToken(TokenType::Comma))
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

            Token t = parser->isMore() ? parser->peekToken() : Token(TokenType::Error, 0, 0);
            if (parentheses)
            {
                // Parse normal functions with opening/closing parentheses
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
                // Parse command-syntax functions that close on a newline or semicolon
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

        // Array index parse
        parser->skipNewlines();
        if (parser->isMore() && parser->isNextToken(TokenType::OpenBrack))
        {
            Node* arrayRes = new Node(NodeType::ExprAccessArray);
            arrayRes->nodes.push_back(res);

            do 
            {
                parser->advance();
                arrayRes->nodes.push_back(Node::ParseExpression(parser));
                parser->skipNewlines();
                parser->ensureToken(TokenType::CloseBrack);
                parser->skipNewlines();
            } while (parser->isMore() && parser->isNextToken(TokenType::OpenBrack));

            return arrayRes;
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
                if (parser->isMore())
                {
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

                        if (!parser->isMore())
                            break;
                    }
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
                if (parser->isMore())
                {
                    t = parser->peekToken();
                    while (t.type == TokenType::BitwiseLShift ||
                           t.type == TokenType::BitwiseRShift)
                    {
                        parser->advance();

                        Node* next = new NodeToken(NodeType::ExprBinary, t);
                        next->nodes.push_back(res);
                        next->nodes.push_back(Node::ParseAddSub(parser));
                        res = next;

                        if (!parser->isMore())
                            break;
                    }
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
                if (parser->isMore())
                {
                    t = parser->peekToken();
                    while (t.type == TokenType::Plus ||
                           t.type == TokenType::Minus)
                    {
                        parser->advance();

                        Node* next = new NodeToken(NodeType::ExprBinary, t);
                        next->nodes.push_back(res);
                        next->nodes.push_back(Node::ParseMulDiv(parser));
                        res = next;

                        if (!parser->isMore())
                            break;
                    }
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
                if (parser->isMore())
                {
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

                        if (!parser->isMore())
                            break;
                    }
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
            case TokenType::Percentage:
                parser->advance();
                return new NodeToken(NodeType::ExprConstant, t);
            case TokenType::String:
            case TokenType::MarkedString:
            case TokenType::ExcludeString:
            {
                parser->advance();
                NodeToken* str = new NodeToken(NodeType::ExprConstant, t);
                str->token.content = Parser::ProcessStringInterpolation(parser, t, t.content, &str->nodes);
                return str;
            }
            case TokenType::VariableStart:
            {
                Node* val = Node::ParseVariable(parser);

                // Also check for postfix operations
                parser->skipNewlines();
                if (parser->isMore())
                {
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

        parser->errors.push_back({ ParseError::ErrorType::UnexpectedEOF, parser->defaultLine, parser->defaultColumn });
        return nullptr;
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
                {
                    NodeDefinition* def = new NodeDefinition(t.content, val.content, val.type != TokenType::String);
                    def->value = Parser::ProcessStringInterpolation(parser, val, val.content, &def->nodes);
                    return def;
                }
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