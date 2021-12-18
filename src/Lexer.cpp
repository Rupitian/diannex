#include "Lexer.h"

#include <string>
#include <memory>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace diannex
{
    // Token constructors
    Token::Token(TokenType type, uint32_t line, uint16_t column)
        : type(type), line(line), column(column)
    {
    }
    Token::Token(TokenType type, uint32_t line, uint16_t column, KeywordType keywordType)
        : type(type), line(line), column(column), keywordType(keywordType)
    {
    }
    Token::Token(TokenType type, uint32_t line, uint16_t column, std::string content)
        : type(type), line(line), column(column), content(content)
    {
    }

    // Utility class for reading code strings easily
    class CodeReader
    {
    public:
        CodeReader(const std::string& code, uint32_t line, uint16_t column)
            : code(code), position(0), length(code.length()), line(line), column(column)
        {
            if ((uint8_t)code[0] == 0xEF && (uint8_t)code[1] == 0xBB && (uint8_t)code[2] == 0xBF)
                position += 3;
        }

        uint32_t position;
        uint32_t length;
        uint32_t line;
        uint16_t column;
        int16_t skip = -1;
        int16_t stack = 0;
        bool directiveFollowup = false;

        inline char peekChar()
        {
            return code[position];
        }

        inline char peekCharNext()
        {
            return code[position + 1];
        }

        inline char peekCharNext2()
        {
            return code[position + 2];
        }

        inline void advanceChar()
        {
            column++;
            position++;
        }

        inline void advanceChar(int count)
        {
            for (int i = 0; i < count; i++)
                advanceChar();
        }

        inline void backUpChar()
        {
            column--;
            position--;
        }

        inline char readChar()
        {
            column++;
            return code[position++];
        }

        inline bool matchChars(char c, char c2)
        {
            if (position + 1 >= length)
                return false;
            if (peekChar() != c)
                return false;
            return (peekCharNext() == c2);
        }

        inline bool matchChars(char c, char c2, char c3)
        {
            if (position + 2 >= length)
                return false;
            if (peekChar() != c)
                return false;
            if (peekCharNext() != c2)
                return false;
            return (peekCharNext2() == c3);
        }

        // Skips whitespace characters
        // Returns true if EOF is hit
        bool skipWhitespace(std::vector<Token>& out)
        {
            while (position < length)
            {
                char curr = peekChar();
                if (curr != ' ' && curr != '\t' && curr != '\r' && curr != '\v' && curr != '\f')
                {
                    if (curr != '\n')
                        return false;
                    else
                    {
                        out.emplace_back(TokenType::Newline, line, column);
                        line++;
                        column = 0;
                    }
                }
                advanceChar();
            }
            return true;
        }

        // Reads a comment if one exists
        // Returns true if recognized
        bool readComment(std::vector<Token>& out)
        {
            if (peekChar() == '/')
            {
                if (position + 1 < length)
                {
                    char n = peekCharNext();
                    if (n == '/')
                    {
                        if (position + 2 < length && peekCharNext2() == '!') // marked comment
                            return false;

                        // This is a normal single-line comment
                        advanceChar(2);

                        // Ignore all further text on this line
                        uint32_t thisLine = line;
                        skipWhitespace(out);
                        while (position < length && thisLine == line)
                        {
                            advanceChar();
                            skipWhitespace(out);
                        }

                        return true;
                    }
                    else if (n == '*')
                    {
                        if (position + 2 < length && peekCharNext2() == '!') // marked comment
                            return false;

                        // This is a normal multi-line comment
                        advanceChar(2);

                        // Ignore text until EOF or "*/"
                        while (position < length)
                        {
                            char c = readChar();
                            if (c == '*')
                            {
                                if (position + 1 < length && peekChar() == '/')
                                {
                                    advanceChar();
                                    return true;
                                }
                            }
                            else if (c == '\n')
                            {
                                line++;
                                column = 0;
                            }
                        }

                        return true;
                    }
                }
            }
            return false;
        }

        // Reads an identifier
        // Returns null if not valid
        std::optional<std::string> readIdentifier()
        {
            uint32_t base = position;

            if (position == length || !isValidIdentifierStart(readChar()))
                return {}; // invalid

            while (position < length)
            {
                char curr = peekChar();
                if (isValidIdentifierMid(curr))
                    advanceChar();
                else
                    break;
            }

            return code.substr(base, position - base);
        }

        void readNumber(char curr, const std::string& in, std::vector<Token>& out)
        {
            uint32_t startLine = line;
            uint16_t startCol = column;

            uint32_t base = position;

            if (curr == '-')
            {
                advanceChar();
                curr = peekChar();
            }

            bool foundSeparator = (curr == '.'), foundNumber = (curr >= '0' && curr <= '9'), isPercent = false;
            advanceChar();

            while (position < length)
            {
                curr = readChar();

                // Check for ".." (range)
                if (curr == '.' && position < length)
                {
                    if (readChar() == '.')
                    {
                        backUpChar();
                        backUpChar();
                        break;
                    }
                    else
                    {
                        backUpChar();
                    }
                }

                if (foundNumber && curr == '%')
                {
                    isPercent = true;
                    break;
                }
                else if (((curr != '.') && curr < '0' || curr > '9') || (foundSeparator && curr == '.'))
                {
                    backUpChar();
                    break;
                }
                else if (curr == '.')
                    foundSeparator = true;
                else
                    foundNumber = true;
            }

            if (isPercent)
                out.emplace_back(TokenType::Percentage, startLine, startCol, in.substr(base, position - base - 1));
            else
                out.emplace_back(TokenType::Number, startLine, startCol, in.substr(base, position - base));
        }
    private:
        std::string code;

        static inline bool isValidIdentifierStart(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0xC0;
        }

        static inline bool isValidIdentifierMid(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || (c >= '0' && c <= '9') || (unsigned char)c >= 0x80;
        }
    };

    static const std::unordered_map<std::string, Token> keywords =
    {
        { "namespace", Token(TokenType::GroupKeyword, 0, 0, KeywordType::Namespace) },
        { "scene", Token(TokenType::GroupKeyword, 0, 0, KeywordType::Scene) },
        { "def", Token(TokenType::GroupKeyword, 0, 0, KeywordType::Def) },
        { "func", Token(TokenType::GroupKeyword, 0, 0, KeywordType::Func) },

        { "choice", Token(TokenType::MainKeyword, 0, 0, KeywordType::Choice) },
        { "choose", Token(TokenType::MainKeyword, 0, 0, KeywordType::Choose) },
        { "if", Token(TokenType::MainKeyword, 0, 0, KeywordType::If) },
        { "else", Token(TokenType::MainKeyword, 0, 0, KeywordType::Else) },
        { "while", Token(TokenType::MainKeyword, 0, 0, KeywordType::While) },
        { "for", Token(TokenType::MainKeyword, 0, 0, KeywordType::For) },
        { "do", Token(TokenType::MainKeyword, 0, 0, KeywordType::Do) },
        { "repeat", Token(TokenType::MainKeyword, 0, 0, KeywordType::Repeat) },
        { "switch", Token(TokenType::MainKeyword, 0, 0, KeywordType::Switch) },
        { "continue", Token(TokenType::MainKeyword, 0, 0, KeywordType::Continue) },
        { "break", Token(TokenType::MainKeyword, 0, 0, KeywordType::Break) },
        { "return", Token(TokenType::MainKeyword, 0, 0, KeywordType::Return) },
        { "case", Token(TokenType::MainKeyword, 0, 0, KeywordType::Case) },
        { "default", Token(TokenType::MainKeyword, 0, 0, KeywordType::Default) },
        { "sequence", Token(TokenType::MainKeyword, 0, 0, KeywordType::Sequence) },

        { "require", Token(TokenType::MainSubKeyword, 0, 0, KeywordType::Require) },

        { "local", Token(TokenType::ModifierKeyword, 0, 0, KeywordType::Local) },
        { "global", Token(TokenType::ModifierKeyword, 0, 0, KeywordType::Global) },

        { "false", Token(TokenType::Number, 0, 0, "0") },
        { "true", Token(TokenType::Number, 0, 0, "1") },

        { "undefined", Token(TokenType::Undefined, 0, 0, "undefined") },
    };

    void Lexer::LexString(const std::string& in, CompileContext* ctx, std::vector<Token>& out, uint32_t startLine, uint16_t startColumn, std::unordered_set<std::string>* macros)
    {
        CodeReader cr = CodeReader(in, startLine, startColumn);

        std::vector<std::string> includes;

        while (cr.position < cr.length)
        {
            if (cr.skipWhitespace(out))
                break;

            // Directive checks when necessary
            if (cr.skip != -1)
            {
                if (cr.peekChar() == '#')
                {
                    if (cr.peekCharNext() == 'i')
                    {
                        if (
                                (cr.advanceChar(), cr.matchChars('i', 'f')) &&
                                (cr.advanceChar(2), cr.peekChar() == 'n' ?
                                                    (cr.advanceChar(), cr.matchChars('d', 'e', 'f')) :
                                                    cr.matchChars('d', 'e', 'f')))
                        {
                            cr.advanceChar(3);
                            char curr = cr.peekChar();
                            if (curr == ' ' || curr == '\t' || curr == '\r' || curr == '\v' || curr == '\f' || curr == '\n')
                            {
                                if (curr == '\n')
                                {
                                    cr.line++;
                                    cr.column = 0;
                                }
                                cr.stack++;
                            }
                        }
                    }
                    else if (cr.peekCharNext() == 'e')
                    {
                        if (
                                (cr.advanceChar(), cr.matchChars('e', 'n', 'd')) &&
                                (cr.advanceChar(3), cr.matchChars('i', 'f')))
                        {
                            cr.advanceChar(2);
                            char curr = cr.peekChar();
                            if (curr == ' ' || curr == '\t' || curr == '\r' || curr == '\v' || curr == '\f' || curr == '\n')
                            {
                                if (curr == '\n')
                                {
                                    cr.line++;
                                    cr.column = 0;
                                }
                                cr.stack--;
                                if (cr.stack == cr.skip)
                                {
                                    cr.skip = -1;
                                }
                            }
                        }
                    }
                }

                cr.advanceChar();
                continue;
            }

            if (cr.readComment(out))
                continue;
            if (cr.matchChars('/', '/', '!')) // Marked comment single-line
            {
                uint16_t col = cr.column;
                cr.advanceChar(3);

                // Get to the next newline/EOF
                uint32_t base = cr.position;
                while (cr.position < cr.length)
                {
                    if (cr.readChar() == '\n')
                    {
                        cr.backUpChar();
                        break;
                    }
                }

                out.emplace_back(TokenType::MarkedComment, cr.line, col, in.substr(base, cr.position - base));
            }
            else if (cr.matchChars('/', '*', '!')) // Marked comment multi-line
            {
                uint32_t line = cr.line;
                uint16_t col = cr.column;
                cr.advanceChar(3);

                // Go until EOF or "*/"
                uint32_t base = cr.position;
                bool foundEnd = false;
                while (cr.position < cr.length)
                {
                    char curr = cr.readChar();
                    if (curr == '*')
                    {
                        if (cr.position + 1 < cr.length && cr.peekChar() == '/')
                        {
                            cr.backUpChar();
                            foundEnd = true;
                            break;
                        }
                    }
                    else if (curr == '\n')
                    {
                        out.emplace_back(TokenType::Newline, cr.line, cr.column);
                        cr.line++;
                        cr.column = 1;
                    }
                }

                out.emplace_back(TokenType::MarkedComment, line, col, in.substr(base, cr.position - base));

                if (foundEnd)
                    cr.advanceChar(2);
            }
            else
            {
                char curr = cr.peekChar();
                if (curr == '#') // Directive
                {
                    uint32_t line = cr.line;
                    uint16_t col = cr.column;
                    cr.advanceChar();

                    // Read the directive type
                    cr.skipWhitespace(out);
                    if (auto identifier = cr.readIdentifier())
                    {
                        cr.directiveFollowup = true;
                        if (identifier->compare("include") == 0)
                        {
                            out.emplace_back(TokenType::Directive, line, col, KeywordType::Include);
                        }
                        else if (identifier->compare("ifdef") == 0)
                        {
                            out.emplace_back(TokenType::Directive, line, col, KeywordType::IfDef);
                        }
                        else if (identifier->compare("ifndef") == 0)
                        {
                            out.emplace_back(TokenType::Directive, line, col, KeywordType::IfNDef);
                        }
                        else if (identifier->compare("endif") == 0) 
                        {
                            out.emplace_back(TokenType::Directive, line, col, KeywordType::EndIf);
                        }
                        else 
                        {
                            cr.directiveFollowup = false;
                            out.emplace_back(TokenType::ErrorString, line, col, *identifier);
                        }
                    }
                    else
                    {
                        out.emplace_back(TokenType::Error, line, col);
                    }
                }
                else if ((curr >= '0' && curr <= '9') || curr == '.') // Number, percentage, or range
                {
                    bool isRange = false;
                    if (curr == '.' && cr.position + 1 < cr.length)
                    {
                        if (cr.peekCharNext() == '.')
                        {
                            isRange = true;
                        }
                    }

                    if (isRange)
                    {
                        out.emplace_back(TokenType::Range, cr.line, cr.column);
                        cr.advanceChar(2);
                    }
                    else
                        cr.readNumber(curr, in, out);
                }
                else if (curr == '"' || cr.matchChars('@', '"') || cr.matchChars('!', '"')) // Strings
                {
                    char type = curr;
                    uint32_t line = cr.line;
                    uint16_t col = cr.column;

                    cr.advanceChar(type == '"' ? 1 : 2);

                    // Parse string content
                    std::stringstream ss(std::ios_base::app | std::ios_base::out);
                    bool foundEnd = false;
                    while (cr.position < cr.length)
                    {
                        curr = cr.readChar();
                        if (curr == '\\')
                        {
                            if (cr.position + 1 < cr.length)
                            {
                                curr = cr.readChar();
                                switch (curr)
                                {
                                case 'a':
                                    ss << '\a';
                                    break;
                                case 'n':
                                    ss << '\n';
                                    break;
                                case 'r':
                                    ss << '\r';
                                    break;
                                case 't':
                                    ss << '\t';
                                    break;
                                case 'v':
                                    ss << '\v';
                                    break;
                                case 'f':
                                    ss << '\f';
                                    break;
                                case 'b':
                                    ss << '\b';
                                    break;
                                case '\n':
                                    break;
                                    // todo: unicode/hex/octal support?
                                default:
                                    ss << curr;
                                    break;
                                }
                            }
                            else
                                break; // error
                        }
                        else if (curr == '"')
                        {
                            foundEnd = true;
                            break;
                        }
                        else
                            ss << curr;
                    }

                    if (foundEnd)
                    {
                        if (type == '"')
                            out.emplace_back(TokenType::String, line, col, ss.str());
                        else if (type == '@')
                            out.emplace_back(TokenType::MarkedString, line, col, ss.str());
                        else // if (type == '!')
                            out.emplace_back(TokenType::ExcludeString, line, col, ss.str());
                    }
                    else
                        out.emplace_back(TokenType::ErrorUnenclosedString, line, col);
                }
                else
                {
                    uint32_t line = cr.line;
                    uint16_t col = cr.column;
                    switch (curr)
                    {
                    case '(':
                        out.emplace_back(TokenType::OpenParen, line, col);
                        break;
                    case ')':
                        out.emplace_back(TokenType::CloseParen, line, col);
                        break;
                    case '{':
                        out.emplace_back(TokenType::OpenCurly, line, col);
                        break;
                    case '}':
                        out.emplace_back(TokenType::CloseCurly, line, col);
                        break;
                    case '[':
                        out.emplace_back(TokenType::OpenBrack, line, col);
                        break;
                    case ']':
                        out.emplace_back(TokenType::CloseBrack, line, col);
                        break;
                    case ';':
                        out.emplace_back(TokenType::Semicolon, line, col);
                        break;
                    case ':':
                        out.emplace_back(TokenType::Colon, line, col);
                        break;
                    case ',':
                        out.emplace_back(TokenType::Comma, line, col);
                        break;
                    case '?':
                        out.emplace_back(TokenType::Ternary, line, col);
                        break;
                    case '$':
                        out.emplace_back(TokenType::VariableStart, line, col);
                        break;
                    case '=':
                        if (cr.position + 1 < cr.length && cr.peekCharNext() == '=')
                        {
                            cr.advanceChar();
                            out.emplace_back(TokenType::CompareEQ, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Equals, line, col);
                        break;
                    case '+':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext(); 
                            if (n == '+')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::Increment, line, col);
                            }
                            else if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::PlusEquals, line, col);
                            }
                            else
                                out.emplace_back(TokenType::Plus, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Plus, line, col);
                        break;
                    case '-':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '-')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::Decrement, line, col);
                            }
                            else if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::MinusEquals, line, col);
                            }
                            else if ((n >= '0' && n <= '9') || n == '.')
                            {
                                cr.readNumber(curr, in, out);
                                continue; // skip advanceChar() call
                            }
                            else
                                out.emplace_back(TokenType::Minus, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Minus, line, col);
                        break;
                    case '*':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '*')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::Power, line, col);
                            }
                            else if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::MultiplyEquals, line, col);
                            }
                            else
                                out.emplace_back(TokenType::Multiply, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Multiply, line, col);
                        break;
                    case '/':
                        if (cr.position + 1 < cr.length && cr.peekCharNext() == '=')
                        {
                            cr.advanceChar();
                            out.emplace_back(TokenType::DivideEquals, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Divide, line, col);
                        break;
                    case '%':
                        if (cr.position + 1 < cr.length && cr.peekCharNext() == '=')
                        {
                            cr.advanceChar();
                            out.emplace_back(TokenType::ModEquals, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Mod, line, col);
                        break;
                    case '!': // doesn't apply to string literals; that's a special string type
                        if (cr.position + 1 < cr.length && cr.peekCharNext() == '=')
                        {
                            cr.advanceChar();
                            out.emplace_back(TokenType::CompareNEQ, line, col);
                        }
                        else
                            out.emplace_back(TokenType::Not, line, col);
                        break;
                    case '>':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::CompareGTE, line, col);
                            }
                            else if (n == '>')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::BitwiseRShift, line, col);
                            }
                            else
                                out.emplace_back(TokenType::CompareGT, line, col);
                        }
                        else
                            out.emplace_back(TokenType::CompareGT, line, col);
                        break;
                    case '<':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::CompareLTE, line, col);
                            }
                            else if (n == '<')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::BitwiseLShift, line, col);
                            }
                            else
                                out.emplace_back(TokenType::CompareLT, line, col);
                        }
                        else
                            out.emplace_back(TokenType::CompareLT, line, col);
                        break;
                    case '&':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '&')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::LogicalAnd, line, col);
                            }
                            else if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::BitwiseAndEquals, line, col);
                            }
                            else
                                out.emplace_back(TokenType::BitwiseAnd, line, col);
                        }
                        else
                            out.emplace_back(TokenType::BitwiseAnd, line, col);
                        break;
                    case '|':
                        if (cr.position + 1 < cr.length)
                        {
                            char n = cr.peekCharNext();
                            if (n == '|')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::LogicalOr, line, col);
                            }
                            else if (n == '=')
                            {
                                cr.advanceChar();
                                out.emplace_back(TokenType::BitwiseOrEquals, line, col);
                            }
                            else
                                out.emplace_back(TokenType::BitwiseOr, line, col);
                        }
                        else
                            out.emplace_back(TokenType::BitwiseOr, line, col);
                        break;
                    case '^':
                        if (cr.position + 1 < cr.length && cr.peekCharNext() == '=')
                        {
                            cr.advanceChar();
                            out.emplace_back(TokenType::BitwiseXorEquals, line, col);
                        }
                        else
                            out.emplace_back(TokenType::BitwiseXor, line, col);
                        break;
                    case '~':
                        out.emplace_back(TokenType::BitwiseNegate, line, col);
                        break;
                    default: 
                        // Must be an identifier of some type, or it's invalid
                        if (auto identifier = cr.readIdentifier())
                        {
                            auto keyword = keywords.find(*identifier);
                            if (keyword != keywords.end())
                            {
                                // This is a built-in keyword; copy the token and assign line/column
                                Token token = keyword->second;
                                token.line = line;
                                token.column = col;
                                out.push_back(token);
                            }
                            else
                            {
                                auto macro = ctx->project->options.macros.find(*identifier);
                                if (macro != ctx->project->options.macros.end())
                                {
                                    // This is one of the project-defined macros; lex the macro in this context
                                    bool createSet = (macros == nullptr);
                                    if (createSet)
                                        macros = new std::unordered_set<std::string>();

                                    auto status = macros->insert(*identifier);
                                    if (status.second)
                                    {
                                        // This isn't present in the macro chain yet, so we're safe to parse
                                        LexString(macro->second, ctx, out, line, col, macros); // todo? maybe have a way to tell that line/col are inside a macro
                                    }
                                    else
                                    {
                                        // This is already present, signifying illegal recursive macro definitions
                                        out.emplace_back(TokenType::Error, line, col, "recursive_macro");
                                    }

                                    if (createSet)
                                    {
                                        delete macros;
                                        macros = nullptr;
                                    }
                                    else
                                        macros->erase(*identifier);
                                }
                                else
                                {
                                    // This is a regular identifier
                                    out.emplace_back(TokenType::Identifier, line, col, *identifier);
                                }
                            }
                        }
                        else
                        {
                            out.emplace_back(TokenType::Error, line, col);

                            // Ignore all further error tokens on this line
                            uint32_t line = cr.line;
                            while (cr.position < cr.length && cr.line == line)
                            {
                                cr.advanceChar();
                                cr.skipWhitespace(out);
                            }
                        }
                        continue; // Skip the advanceChar call
                    }
                    cr.advanceChar();
                }
            }

            if (cr.directiveFollowup)
            {
                Token t = out.back(); //  Get directive token
                out.pop_back();
                cr.directiveFollowup = false;

                if (t.keywordType == KeywordType::Include)
                {
                    if (cr.skipWhitespace(out))
                    {
                        out.emplace_back(TokenType::Error, cr.line, cr.column, "unexpected_eof");
                        break;
                    }

                    char curr = cr.peekChar();
                    uint32_t line = cr.line;
                    uint16_t col = cr.column;

                    if (curr != '"')
                    {
                        // Token wasn't a string like we expected, push an error token and try to continue
                        out.emplace_back(TokenType::Error, line, col);
                        continue;
                    }

                    cr.advanceChar();

                    std::stringstream ss(std::ios_base::app | std::ios_base::out);
                    bool foundEnd = false;
                    while (cr.position < cr.length)
                    {
                        curr = cr.readChar();
                        if (curr == '"')
                        {
                            foundEnd = true;
                            break;
                        }
                        ss << curr;
                    }

                    if (!foundEnd)
                    {
                        out.emplace_back(TokenType::ErrorUnenclosedString, line, col);
                        continue;
                    }

                    fs::path p = fs::absolute(ctx->currentFile).parent_path();
                    p /= ss.str();
                    includes.push_back(p.string());
                }
                else if (t.keywordType == KeywordType::IfDef || t.keywordType == KeywordType::IfNDef)
                {
                    if (cr.skipWhitespace(out))
                    {
                        out.emplace_back(TokenType::Error, cr.line, cr.column, "unexpected_eof");
                        break;
                    }

                    bool invert = t.keywordType == KeywordType::IfNDef;
                    int line = cr.line, column = cr.column;
                    if (auto identifier = cr.readIdentifier())
                    {
                        bool skip = ctx->project->options.macros.find(*identifier) == ctx->project->options.macros.end();
                        if (invert) 
                            skip = !skip;

                        if (skip)
                            cr.skip = cr.stack;

                        cr.stack++;
                    }
                    else
                    {
                        out.emplace_back(TokenType::Error, line, column);
                    }
                }
                else if (t.keywordType == KeywordType::EndIf)
                {
                    if (cr.stack > 0)
                        cr.stack--;
                    else
                        out.emplace_back(TokenType::Error, t.line, t.column, "trailing_endif");
                }
            }
        }

        // Add includes to beginning of list, in reverse order
        for (auto it = includes.rbegin(); it != includes.rend(); ++it)
            ctx->queue.push_front(*it);
    }

    const char* tokenToString(Token t)
    {
        switch (t.type)
        {
        case TokenType::Identifier:
            return "Identifier";
        case TokenType::Number:
            return "Number";
        case TokenType::Percentage:
            return "Percentage";
        case TokenType::String:
        case TokenType::MarkedString:
        case TokenType::ExcludeString:
            return "String";
        case TokenType::OpenParen:
            return "'('";
        case TokenType::CloseParen:
            return "')'";
        case TokenType::OpenCurly:
            return "'{'";
        case TokenType::CloseCurly:
            return "'}'";
        case TokenType::OpenBrack:
            return "'['";
        case TokenType::CloseBrack:
            return "']'";
        case TokenType::Semicolon:
            return "';'";
        case TokenType::Colon:
            return "':'";
        case TokenType::Comma:
            return "','";
        case TokenType::Ternary:
            return "'?'";
        case TokenType::VariableStart:
            return "'$'";
        case TokenType::Equals:
            return "'='";
        case TokenType::Plus:
            return "'+'";
        case TokenType::Increment:
            return "'++'";
        case TokenType::PlusEquals:
            return "'+='";
        case TokenType::Minus:
            return "'-'";
        case TokenType::Decrement:
            return "'--'";
        case TokenType::MinusEquals:
            return "'-='";
        case TokenType::Multiply:
            return "'*'";
        case TokenType::Power:
            return "'**'";
        case TokenType::MultiplyEquals:
            return "'*='";
        case TokenType::Divide:
            return "'/'";
        case TokenType::DivideEquals:
            return "'/='";
        case TokenType::Mod:
            return "'%'";
        case TokenType::ModEquals:
            return "'%='";
        case TokenType::Not:
            return "'!'";
        case TokenType::CompareEQ:
            return "'=='";
        case TokenType::CompareGT:
            return "'>'";
        case TokenType::CompareLT:
            return "'<'";
        case TokenType::CompareGTE:
            return "'>='";
        case TokenType::CompareLTE:
            return "'<='";
        case TokenType::CompareNEQ:
            return "'!='";
        case TokenType::LogicalAnd:
            return "'&&'";
        case TokenType::LogicalOr:
            return "'||'";
        case TokenType::BitwiseLShift:
            return "'<<'";
        case TokenType::BitwiseRShift:
            return "'>>'";
        case TokenType::BitwiseAnd:
            return "'&'";
        case TokenType::BitwiseAndEquals:
            return "'&='";
        case TokenType::BitwiseOr:
            return "'|'";
        case TokenType::BitwiseOrEquals:
            return "'|='";
        case TokenType::BitwiseXor:
            return "'^'";
        case TokenType::BitwiseXorEquals:
            return "'^='";
        case TokenType::BitwiseNegate:
            return "'~'";
        case TokenType::MarkedComment:
            return "MarkedComment";
        case TokenType::Range:
            return "Range";
        case TokenType::GroupKeyword:
            switch (t.keywordType)
            {
            case KeywordType::Namespace:
                return "'namespace'";
            case KeywordType::Scene:
                return "'scene'";
            case KeywordType::Def:
                return "'def'";
            case KeywordType::Func:
                return "'func'";
            }
            break;
        case TokenType::MainKeyword:
            switch (t.keywordType)
            {
            case KeywordType::Choice:
                return "'choice'";
            case KeywordType::Choose:
                return "'choose'";
            case KeywordType::If:
                return "'if'";
            case KeywordType::Else:
                return "'else'";
            case KeywordType::While:
                return "'while'";
            case KeywordType::For:
                return "'for'";
            case KeywordType::Do:
                return "'do'";
            case KeywordType::Repeat:
                return "'repeat'";
            case KeywordType::Switch:
                return "'switch'";
            case KeywordType::Continue:
                return "'continue'";
            case KeywordType::Break:
                return "'break'";
            case KeywordType::Return:
                return "'return'";
            case KeywordType::Case:
                return "'case'";
            case KeywordType::Default:
                return "'default'";
            case KeywordType::Sequence:
                return "'sequence'";
            }
            break;
        case TokenType::MainSubKeyword:
            switch (t.keywordType)
            {
            case KeywordType::Require:
                return "'require'";
            }
            break;
        case TokenType::ModifierKeyword:
            switch (t.keywordType)
            {
            case KeywordType::Local:
                return "'local'";
            case KeywordType::Global:
                return "'global'";
            }
            break;
        }
        return "<unknown token>";
    }
}
