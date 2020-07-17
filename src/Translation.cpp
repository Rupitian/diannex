#include "Translation.h"

#include <sstream>

namespace diannex
{
    void Translation::GeneratePublicFile(std::ofstream& s, CompileContext* ctx)
    {
        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
        {
            if (!it->isComment)
                s << SanitizeString(it->text) << "\n";
        }
    }

    void Translation::GeneratePrivateFile(std::ofstream& s, CompileContext* ctx)
    {
        std::string prevKey = "";
        bool first = true;

        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
        {
            if (it->key != prevKey)
            {
                if (prevKey != "" || first)
                {
                    s << "\n";

                    first = false;
                }

                prevKey = it->key;
                if (prevKey != "")
                    s << "@" << prevKey << "\n";
            }

            if (it->isComment)
            {
                size_t startOffset = 0;
                
                for (size_t endOffset = 0; endOffset != std::string::npos; startOffset = endOffset + 1)
                {
                    endOffset = it->text.find("\n", startOffset);
                    
                    std::string str = it->text.substr(startOffset, (endOffset == std::string::npos) ? std::string::npos : endOffset - startOffset);
                    if (str.length() != 0 && std::isspace(str.at(0)))
                    {
                        // Push to the left; only have one space
                        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) 
                        {
                            return !std::isspace(ch);
                        }));
                        str.insert(str.begin(), ' ');
                    }
                    s << "#" << str << "\n";
                }
            }
            else
            {
                s << "\"" << SanitizeString(it->text) << "\"\n";
            }

            first = false;
        }
    }

    void Translation::ConvertPrivateToPublic(std::ifstream& in, std::ofstream& out) 
    {
        CompileContext ctx;
        std::string text = "";
        TranslationInfo translationInfo;
        translationInfo.isComment = false;
        
        while (std::getline(in, text))
        {
            const size_t infoStart = text.find_first_not_of(" ");

            if (text[infoStart] == '"')
            {
                translationInfo.text = text.substr(infoStart + 1, text.find_last_of('"') - infoStart - 1);
                ctx.translationInfo.push_back(translationInfo);
            }
        }

        GeneratePublicFile(out, &ctx);
    }

    std::string Translation::SanitizeString(const std::string& str)
    {
        std::stringstream ss(std::ios_base::app | std::ios_base::out);

        for (auto it = str.begin(); it != str.end(); ++it)
        {
            const char c = *it;
            switch (c)
            {
            case '\a':
                ss << "\\a";
                break;
            case '\n':
                ss << "\\n";
                break;
            case '\r':
                ss << "\\r";
                break;
            case '\t':
                ss << "\\t";
                break;
            case '\v':
                ss << "\\v";
                break;
            case '\f':
                ss << "\\f";
                break;
            case '\b':
                ss << "\\b";
                break;
            case '"':
                ss << "\\\"";
                break;
            case '\\':
                ss << "\\\\";
                break;
            default:
                ss << c;
                break;
            }
        }

        return ss.str();
    }
}