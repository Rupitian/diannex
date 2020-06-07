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
        std::unique_ptr<Node> block(Node::ParseGroupBlock(&parser, false));
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
    {
        this->type = type;
        this->nodes = std::vector<Node*>();
    }

    Node::~Node()
    {
        for (Node* s : nodes)
            delete s;
    }

    NodeContent::NodeContent(std::string content, NodeType type) : Node(type)
    {
        this->content = content;
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
                    break;
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

    NodeTextRun::NodeTextRun(std::string content, bool excludeTranslation) : NodeContent(content, NodeType::TextRun)
    {
        this->excludeTranslation = excludeTranslation;
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

    NodeDefinition::NodeDefinition(std::string key, std::string value, bool excludeValueTranslation) : Node(NodeType::Definition)
    {
        this->key = key;
        this->value = value;
        this->excludeValueTranslation = excludeValueTranslation;
    }
}