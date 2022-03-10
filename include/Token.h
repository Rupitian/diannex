#ifndef DIANNEX_TOKEN_H
#define DIANNEX_TOKEN_H

#include <string>
#include <memory>

namespace diannex
{
    enum class TokenType
    {
        Identifier, // a-z, A-Z, other language chars, _ (and 0-9 or . after first char)
        Number, // 0-9 first chars, optional . followed by more 0-9
        Percentage, // %
        String, // " followed by content, ending with unescaped " (no backslash preceding it) (also some other escape codes)
        MarkedString, // @" and then continue like String
        ExcludeString, // !" and then continue like String

        Undefined, // An Identifier called "undefined"

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
        Range, // ..

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

    enum class KeywordType
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
        Sequence,

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

    struct StringData
    {
        int32_t localizedStringId = -1;
        uint32_t endOfStringPos = 0;

        StringData(int32_t localizedStringId, uint32_t endOfStringPos);
    };

    struct Token
    {
        TokenType type = TokenType::Error;
        uint32_t line = 0;
        uint32_t column = 0;
        KeywordType keywordType = KeywordType::None;
        std::string content; // unused if KeywordType is known
        std::shared_ptr<StringData> stringData = nullptr;

        Token(TokenType type, uint32_t line, uint32_t column);
        Token(TokenType type, uint32_t line, uint32_t column, KeywordType keywordType);
        Token(TokenType type, uint32_t line, uint32_t column, std::string content);
        Token(TokenType type, uint32_t line, uint32_t column, std::string content, std::shared_ptr<StringData> stringData);
    };
}

#endif // DIANNEX_TOKEN_H