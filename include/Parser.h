#ifndef DIANNEX_PARSER_H
#define DIANNEX_PARSER_H

#include "Context.h"
#include "Lexer.h"
#include "ParseResult.h"

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
            ChooseWithoutStatement,
            ChoiceWithoutStatement,
            DuplicateFlagName
        };

        ErrorType type;
        uint32_t line;
        uint16_t column;
        const char* info1;
        const char* info2;
    };

    class Parser
    {
    public:
        static ParseResult* ParseTokens(CompileContext* ctx, std::vector<Token>* tokens);
        static ParseResult ParseTokensExpression(CompileContext* ctx, std::vector<Token>* tokens, uint32_t defaultLine, uint16_t defaultColumn);
        static std::string ProcessStringInterpolation(Parser* parser, Token& token, const std::string& input, std::vector<class Node*>* nodeList);

        Parser(CompileContext* ctx, std::vector<Token>* tokens);

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
        CompileContext* context;
        uint32_t defaultLine = 0;
        uint16_t defaultColumn = 0;
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
            Function,

            // Special
            Flag,

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
            ChoiceText,
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
            Choice,
            Sequence,
            Subsequence,
            SwitchSimple,

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
            ExprAccessArray,
            ExprRange,

            // Definitions-scope
            Definition
        };

        static Node* ParseGroupBlock(Parser* parser, bool isNamespace);
        static Node* ParseNamespaceBlock(Parser* parser, std::string name);
        static Node* ParseGroupStatement(Parser* parser, KeywordType modifier);

        static Node* ParseDefinitionBlock(Parser* parser, Token name);
        static Node* ParseDefinitionStatement(Parser* parser);

        static Node* ParseFunctionBlock(Parser* parser, Token name, KeywordType modifier);
        
        static Node* ParseSceneBlock(Parser* parser);
        static Node* ParseSceneBlock(Parser* parser, Token name);
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
        Node(const Node&) = delete; 
        Node& operator=(const Node&) = delete;
    };

    class NodeContent : public Node
    {
    public:
        NodeContent(Token token, NodeType type);
        NodeContent(std::string content, NodeType type);

        std::string content;
        Token token;
    private:
        NodeContent(const NodeContent&) = delete;
    };

    /*
        Scene/function statements
    */

    class NodeText : public NodeContent
    {
    public:
        NodeText(NodeType type, std::string content, bool excludeTranslation);
        NodeText(std::string content, bool excludeTranslation);
        bool excludeTranslation;
    private:
        NodeText(const NodeText&) = delete;
    };

    class NodeToken : public Node
    {
    public:
        NodeToken(NodeType type, Token token);

        Token token;
    private:
        NodeToken(const NodeToken&) = delete;
    };

    class NodeTokenModifier : public Node
    {
    public:
        NodeTokenModifier(NodeType type, Token token, KeywordType modifier);

        Token token;
        KeywordType modifier;
    private:
        NodeTokenModifier(const NodeTokenModifier&) = delete;
    };

    class NodeScene : public NodeContent
    {
    public:
        NodeScene(Token token);

        std::vector<NodeContent*> flags;
    private:
        NodeScene(const NodeScene&) = delete;
    };

    class NodeFunc : public Node
    {
    public:
        NodeFunc(Token token, KeywordType modifier);
        NodeFunc(std::string name, KeywordType modifier);

        std::string name;
        Token token;
        KeywordType modifier;
        std::vector<Token> args;
        std::vector<NodeContent*> flags;
    private:
        NodeFunc(const NodeFunc&) = delete;
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
    private:
        NodeDefinition(const NodeDefinition&) = delete;
    };
}

#endif