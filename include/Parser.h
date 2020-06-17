#ifndef DIANNEX_PARSER_H
#define DIANNEX_PARSER_H

#include "Lexer.h"

#include <memory>

namespace diannex
{
    struct ParseError
    {
        enum ErrorType
        {
            ExpectedTokenButGot,
            ExpectedTokenButEOF,
            UnexpectedToken,
            UnexpectedModifierFor,
            UnexpectedMarkedString,
            UnexpectedEOF,
            UnexpectedSwitchCase,
            UnexpectedSwitchDefault,
            ChooseWithoutStatement
        };

        ErrorType type;
        uint32_t line;
        uint16_t column;
        const char* info1;
        const char* info2;
    };

    struct ParseResult
    {
        std::shared_ptr<class Node> baseNode;
        std::vector<ParseError> errors;
    };

    class Parser
    {
    public:
        static ParseResult ParseTokens(std::vector<Token>* tokens);

        Parser(std::vector<Token>* tokens);

        void advance();
        void synchronize();
        void storePosition();
        void restorePosition();

        bool isMore();
        void skipNewlines();
        void skipSemicolons();
        bool isNextToken(TokenType type);

        Token peekToken();
        Token previousToken();
        Token ensureToken(TokenType type);
        Token ensureToken(TokenType type, TokenType type2);
        Token ensureToken(TokenType type, KeywordType keywordType);

        std::vector<ParseError> errors;
    private:
        Parser();

        std::vector<Token>* tokens;
        int tokenCount;
        int position;
        int storedPosition;
    };

    /*
        General-purpose node types
    */

    class Node
    {
    public:
        enum NodeType
        {
            // File-scope
            Block,
            MarkedComment,
            Namespace,
            Scene,
            Definitions,

            // Scene-scope
            SceneBlock,
            TextRun,
            Variable,
            Increment,
            Decrement,
            Assign,
            SceneFunction,
            ShorthandChar,
            SwitchCase,
            SwitchDefault,
            None,

            If,
            While,
            For,
            Do,
            Repeat,
            Switch,
            Continue,
            Break,
            Return,
            Choose,

            ExprConstant,
            ExprNot,
            ExprNegate,
            ExprBitwiseNegate,
            ExprBinary,
            ExprTernary,
            ExprArray,
            ExprPreIncrement,
            ExprPostIncrement,
            ExprPreDecrement,
            ExprPostDecrement,

            // Definitions-scope
            Definition
        };

        static Node* ParseGroupBlock(Parser* parser, bool isNamespace);
        static Node* ParseNamespaceBlock(Parser* parser, std::string name);
        static Node* ParseGroupStatement(Parser* parser, KeywordType modifier);

        static Node* ParseDefinitionBlock(Parser* parser, std::string name);
        static Node* ParseDefinitionStatement(Parser* parser);
        
        static Node* ParseSceneBlock(Parser* parser);
        static Node* ParseSceneBlock(Parser* parser, std::string name);
        static Node* ParseSceneStatement(Parser* parser, KeywordType modifier, bool inSwitch = false);
        static Node* ParseVariable(Parser* parser);
        static Node* ParseFunction(Parser* parser, bool parentheses = true);

        static Node* ParseExpression(Parser* parser);
        static Node* ParseConditional(Parser* parser);
        static Node* ParseOr(Parser* parser);
        static Node* ParseAnd(Parser* parser);
        static Node* ParseCompare(Parser* parser);
        static Node* ParseBitwise(Parser* parser);
        static Node* ParseBitShift(Parser* parser);
        static Node* ParseAddSub(Parser* parser);
        static Node* ParseMulDiv(Parser* parser);
        static Node* ParseExprLast(Parser* parser);

        Node(NodeType type);
        ~Node();

        NodeType type;
        std::vector<Node*> nodes;
    private:
        Node(const Node& node) = delete; 
        Node& operator=(const Node&) = delete;
    };

    class NodeContent : public Node
    {
    public:
        NodeContent(std::string content, NodeType type);

        std::string content;
    };

    /*
        Scene/function statements
    */

    class NodeTextRun : public NodeContent
    {
    public:
        NodeTextRun(std::string content, bool excludeTranslation);
        bool excludeTranslation;
    };

    class NodeToken : public Node
    {
    public:
        NodeToken(NodeType type, Token token);

        Token token;
    };

    class NodeTokenModifier : public Node
    {
    public:
        NodeTokenModifier(NodeType type, Token token, KeywordType modifier);

        Token token;
        KeywordType modifier;
    };


    /*
        Definitions statements
    */

    class NodeDefinition : public Node
    {
    public:
        NodeDefinition(std::string key, std::string value, bool excludeValueTranslation);

        std::string key;
        std::string value;
        bool excludeValueTranslation;
    };
}

#endif