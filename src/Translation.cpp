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

        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
        {
            if(it->key != prevKey) {
                if(prevKey != "") {
                    s << "\n";
                }

                prevKey = it->key;
                s << "@" << prevKey << "\n";
            }

            if (it->isComment) {
                size_t startOffset = 0;
                
                for(size_t endOffset = 0; endOffset != std::string::npos; startOffset = endOffset + 1) {
                    endOffset = it->text.find("\n", startOffset);
                    
                    s << "#" << it->text.substr(startOffset, (endOffset == std::string::npos) ? std::string::npos : endOffset - startOffset) << "\n";
                }
            } else {
                s << "\"" << SanitizeString(it->text) << "\"\n";
            }
        }
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