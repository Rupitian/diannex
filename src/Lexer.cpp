#include "Lexer.h"

#include <string>
#include <memory>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace diannex
{
    // Utility class for reading code strings easily
    class CodeReader
    {
    public:
        CodeReader(const std::string& code)
        {
            this->code = code;

            position = 0;
            length = code.length();
            line = 1;
            column = 1;
            skip = -1;
            stack = 0;
            directiveFollowup = false;
        }

        uint32_t position;
        uint32_t length;
        uint32_t line;
        uint16_t column;
        int16_t skip;
        int16_t stack;
        bool directiveFollowup;

        char peekChar()
        {
            return code[position];
        }

        char peekCharNext()
        {
            return code[position + 1];
        }

        char peekCharNext2()
        {
            return code[position + 2];
        }

        void advanceChar()
        {
            column++;
            position++;
        }

        void advanceChar(int count)
        {
            for (int i = 0; i < count; i++)
                advanceChar();
        }

        void backUpChar()
        {
            column--;
            position--;
        }

        char readChar()
        {
            column++;
            return code[position++];
        }

        bool matchChars(char c, char c2)
        {
            if (position + 1 >= length)
                return false;
            if (peekChar() != c)
                return false;
            return (peekCharNext() == c2);
        }

        bool matchChars(char c, char c2, char c3)
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
                            if (readChar() == '*')
                            {
                                if (position + 1 < length && peekChar() == '/')
                                {
                                    advanceChar();
                                    return true;
                                }
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
        std::unique_ptr<std::string> readIdentifier()
        {
            uint32_t base = position;

            if (position == length || !isValidIdentifierStart(readChar()))
                return std::unique_ptr<std::string>(nullptr); // invalid

            while (position < length)
            {
                char curr = peekChar();
                if (isValidIdentifierMid(curr))
                    advanceChar();
                else
                    break;
            }

            return std::make_unique<std::string>(code.substr(base, position - base));
        }
    private:
        std::string code;

        static bool isValidIdentifierStart(char c)
        {
            return (c >= 'a' && c < 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0xC0;
        }

        static bool isValidIdentifierMid(char c)
        {
            return (c >= 'a' && c < 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || (c >= '0' && c <= '9') || (unsigned char)c >= 0x80;
        }
    };

    void Lexer::LexString(const std::string& in, LexerContext& ctx, std::vector<Token>& out)
    {
        CodeReader cr = CodeReader(in);

        if (cr.matchChars(0xEF, 0xBB, 0xBF))
            cr.advanceChar(3); // UTF-8 BOM

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
                    std::unique_ptr<std::string> identifier = cr.readIdentifier();
                    if (identifier)
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
                            out.emplace_back(TokenType::ErrorString, line, col, *identifier.get());
                        }
                    }
                    else
                    {
                        out.emplace_back(TokenType::Error, line, col);
                    }
                }
                else if ((curr >= '0' && curr <= '9') || curr == '.') // Number or percentage
                {
                    uint32_t line = cr.line;
                    uint16_t col = cr.column;

                    uint32_t base = cr.position;
                    bool foundSeparator = (curr == '.'), foundNumber = (curr >= '0' && curr <= '9'), isPercent = false;
                    cr.advanceChar();

                    while (cr.position < cr.length)
                    {
                        curr = cr.readChar();
                        if (foundNumber && curr == '%')
                        {
                            isPercent = true;
                            break;
                        }
                        else if (((curr != '.') && curr < '0' || curr > '9') || (foundSeparator && curr == '.'))
                        {
                            cr.backUpChar();
                            break;
                        }
                        else if (curr == '.')
                            foundSeparator = true;
                        else
                            foundNumber = true;
                    }

                    if (isPercent)
                        out.emplace_back(TokenType::Percentage, line, col, in.substr(base, cr.position - base - 1));
                    else
                        out.emplace_back(TokenType::Number, line, col, in.substr(base, cr.position - base));
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
                    default: // Must be an identifier, or it's invalid
                        std::unique_ptr<std::string> identifier = cr.readIdentifier();
                        if (identifier)
                        {
                            if (identifier->compare("namespace") == 0)
                                out.emplace_back(TokenType::GroupKeyword, line, col, KeywordType::Namespace);
                            else if (identifier->compare("scene") == 0)
                                out.emplace_back(TokenType::GroupKeyword, line, col, KeywordType::Scene);
                            else if (identifier->compare("def") == 0)
                                out.emplace_back(TokenType::GroupKeyword, line, col, KeywordType::Def);
                            else if (identifier->compare("func") == 0)
                                out.emplace_back(TokenType::GroupKeyword, line, col, KeywordType::Func);
                            else if (identifier->compare("choice") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Choice);
                            else if (identifier->compare("choose") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Choose);
                            else if (identifier->compare("if") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::If);
                            else if (identifier->compare("else") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Else);
                            else if (identifier->compare("while") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::While);
                            else if (identifier->compare("for") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::For);
                            else if (identifier->compare("do") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Do);
                            else if (identifier->compare("repeat") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Repeat);
                            else if (identifier->compare("switch") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Switch);
                            else if (identifier->compare("continue") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Continue);
                            else if (identifier->compare("break") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Break);
                            else if (identifier->compare("return") == 0)
                                out.emplace_back(TokenType::MainKeyword, line, col, KeywordType::Return);
                            else if (identifier->compare("require") == 0)
                                out.emplace_back(TokenType::MainSubKeyword, line, col, KeywordType::Require);
                            else if (identifier->compare("chance") == 0)
                                out.emplace_back(TokenType::MainSubKeyword, line, col, KeywordType::Chance);
                            else if (identifier->compare("local") == 0)
                                out.emplace_back(TokenType::ModifierKeyword, line, col, KeywordType::Local);
                            else if (identifier->compare("global") == 0)
                                out.emplace_back(TokenType::ModifierKeyword, line, col, KeywordType::Global);
                            else
                                out.emplace_back(TokenType::Identifier, line, col, *identifier.get());
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
                        out.emplace_back(TokenType::Error, cr.line, cr.column, "Unexpected EOF");
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

                    fs::path p = fs::absolute(ctx.currentFile).parent_path();
                    p /= ss.str();
                    ctx.queue.push(p.string());
                }
                else if (t.keywordType == KeywordType::IfDef || t.keywordType == KeywordType::IfNDef)
                {
                    if (cr.skipWhitespace(out))
                    {
                        out.emplace_back(TokenType::Error, cr.line, cr.column, "Unexpected EOF");
                        break;
                    }

                    bool invert = t.keywordType == KeywordType::IfNDef;
                    int line = cr.line, column = cr.column;
                    std::unique_ptr<std::string> identifier = cr.readIdentifier();
                    if (!identifier)
                    {
                        out.emplace_back(TokenType::Error, line, column);
                        continue;
                    }

                    bool skip = ctx.project->options.macros.find(*identifier) == ctx.project->options.macros.end();
                    if (invert) skip = !skip;

                    if (skip)
                        cr.skip = cr.stack;

                    cr.stack++;
                }
                else if (t.keywordType == KeywordType::EndIf)
                {
                    out.emplace_back(TokenType::Error, t.line, t.column, "Trailing #endif");
                }
            }
        }
    }

    const char* tokenToString(Token t)
    {
        switch (t.type)
        {
        case Identifier:
            return "Identifier";
        case Number:
            return "Number";
        case Percentage:
            return "Percentage";
        case String:
        case MarkedString:
        case ExcludeString:
            return "String";
        case OpenParen:
            return "'('";
        case CloseParen:
            return "')'";
        case OpenCurly:
            return "'{'";
        case CloseCurly:
            return "'}'";
        case OpenBrack:
            return "'['";
        case CloseBrack:
            return "']'";
        case Semicolon:
            return "';'";
        case Colon:
            return "':'";
        case Comma:
            return "','";
        case Ternary:
            return "'?'";
        case VariableStart:
            return "'$'";
        case Equals:
            return "'='";
        case Plus:
            return "'+'";
        case Increment:
            return "'++'";
        case PlusEquals:
            return "'+='";
        case Minus:
            return "'-'";
        case Decrement:
            return "'--'";
        case MinusEquals:
            return "'-='";
        case Multiply:
            return "'*'";
        case Power:
            return "'**'";
        case MultiplyEquals:
            return "'*='";
        case Divide:
            return "'/'";
        case DivideEquals:
            return "'/='";
        case Mod:
            return "'%'";
        case ModEquals:
            return "'%='";
        case Not:
            return "'!'";
        case CompareEQ:
            return "'=='";
        case CompareGT:
            return "'>'";
        case CompareLT:
            return "'<'";
        case CompareGTE:
            return "'>='";
        case CompareLTE:
            return "'<='";
        case CompareNEQ:
            return "'!='";
        case LogicalAnd:
            return "'&&'";
        case LogicalOr:
            return "'||'";
        case BitwiseLShift:
            return "'<<'";
        case BitwiseRShift:
            return "'>>'";
        case BitwiseAnd:
            return "'&'";
        case BitwiseAndEquals:
            return "'&='";
        case BitwiseOr:
            return "'|'";
        case BitwiseOrEquals:
            return "'|='";
        case BitwiseXor:
            return "'^'";
        case BitwiseXorEquals:
            return "'^='";
        case BitwiseNegate:
            return "'~'";
        case MarkedComment:
            return "MarkedComment";
        case GroupKeyword:
            switch (t.keywordType)
            {
            case Namespace:
                return "'namespace'";
            case Scene:
                return "'scene'";
            case Def:
                return "'def'";
            case Func:
                return "'func'";
            }
            break;
        case MainKeyword:
            switch (t.keywordType)
            {
            case Choice:
                return "'choice'";
            case Choose:
                return "'choose'";
            case If:
                return "'if'";
            case Else:
                return "'else'";
            case While:
                return "'while'";
            case For:
                return "'for'";
            case Do:
                return "'do'";
            case Repeat:
                return "'repeat'";
            case Switch:
                return "'switch'";
            case Continue:
                return "'continue'";
            case Break:
                return "'break'";
            case Return:
                return "'return'";
            }
            break;
        case MainSubKeyword:
            switch (t.keywordType)
            {
            case Require:
                return "'require'";
            case Chance:
                return "'chance'";
            }
            break;
        case ModifierKeyword:
            switch (t.keywordType)
            {
            case Local:
                return "'local'";
            case Global:
                return "'global'";
            }
            break;
        }
        return "<unknown token>";
    }
}