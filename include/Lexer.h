#ifndef DIANNEX_LEXER_H
#define DIANNEX_LEXER_H

#include <cstdint>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>

#include "Project.h"
#include "Utility.h"
#include "Context.h"

namespace diannex
{
    enum TokenType
    {
        Identifier, // a-z, A-Z, other language chars, _ (and 0-9 or . after first char)
        Number, // 0-9 first chars, optional . followed by more 0-9
        Percentage, // %
        String, // " followed by content, ending with unescaped " (no backslash preceding it) (also some other escape codes)
        MarkedString, // @" and then continue like String
        ExcludeString, // !" and then continue like String

        GroupKeyword, // These are reserved identifiers, documented in below enum
        MainKeyword,
        MainSubKeyword,
        ModifierKeyword,

        OpenParen, // (
        CloseParen, // )
        OpenCurly, // {
        CloseCurly, // }
        OpenBrack, // [
        CloseBrack, // ]
        Semicolon, // ;
        Colon, // :
        Comma, // ,
        Ternary, // ?

        VariableStart, // $

        Newline, // Used contextually as a semicolon at the end of statements

        Equals, // =
        Plus, // +
        Increment, // ++
        PlusEquals, // +=
        Minus, // -
        Decrement, // --
        MinusEquals, // -=
        Multiply, // *
        Power, // **
        MultiplyEquals, // *=
        Divide, // /
        DivideEquals, // /=
        Mod, // %
        ModEquals, // %=
        Not, // !

        CompareEQ, // ==
        CompareGT, // >
        CompareLT, // <
        CompareGTE, // >=
        CompareLTE, // <=
        CompareNEQ, // != 

        LogicalAnd, // &&
        LogicalOr, // ||

        BitwiseLShift, // <<
        BitwiseRShift, // >>
        BitwiseAnd, // &
        BitwiseAndEquals, // &=
        BitwiseOr, // |
        BitwiseOrEquals, // |=
        BitwiseXor, // ^
        BitwiseXorEquals, // ^=
        BitwiseNegate, // ~

        Directive, // #

        MarkedComment, // "//*" or "/**" followed by comment, latter closed with normal "*/"

        Error, // If no token matches for some reason OR an error value in the parser
        ErrorString, // If there's an error token, but we want to give the token string in the message
        ErrorUnenclosedString, // If there's a string with no end
    };

    enum KeywordType
    {
        None,

        // Group scope (highest level)
        Namespace,
        Scene,
        Def,
        Func,

        // Main scope (scene/function-scope)
        Choice,
        Choose,
        If,
        Else,
        While,
        For,
        Do,
        Repeat,
        Switch,
        Continue,
        Break,
        Return,
        Case,
        Default,

        // Choice/choose scope
        Require,

        // Modifiers (in either scope)
        Local,
        Global,

        // Directive keywords
        Include,
        IfDef,
        IfNDef,
        EndIf
    };

    struct Token
    {
        TokenType type;
        uint32_t line;
        uint16_t column;
        KeywordType keywordType;
        std::string content; // unused if KeywordType is known

        Token(TokenType type, uint32_t line, uint16_t column);
        Token(TokenType type, uint32_t line, uint16_t column, KeywordType keywordType);
        Token(TokenType type, uint32_t line, uint16_t column, std::string content);
    };

    const char* tokenToString(Token t);

    class Lexer
    {
    public:
        static void LexString(const std::string& in, CompileContext& ctx, std::vector<Token>& out);
    private:
        Lexer();
    };
}

#endif // DIANNEX_LEXER_H