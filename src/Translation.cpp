#include "Translation.h"

#include <sstream>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <libs/rang.hpp>

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
        bool writtenAnything = false;

        std::stringstream ss(std::ios_base::app | std::ios_base::out);

        for (auto it = ctx->translationInfo.begin(); it != ctx->translationInfo.end(); ++it)
        {
            if (it->key != prevKey)
            {
                if (prevKey != "" || first)
                {
                    if (writtenAnything)
                       s << "\n";

                    first = false;
                }

                prevKey = it->key;
                if (prevKey != "")
                {
                    s << "@" << prevKey << "\n";
                    writtenAnything = true;
                }
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
                    writtenAnything = true;
                }
            }
            else
            {
                s << "\"" << SanitizeString(it->text) << "\"";
                if (ctx->project->options.useStringIds && it->localizationStringId != -1)
                {
                    ss.str(std::string());
                    ss.clear();
                    ss << '&' << std::setfill('0') << std::setw(8) << std::hex << it->localizationStringId;
                    s << ss.str();
                }
                s << "\n";
                writtenAnything = true;
            }

            first = false;
        }
    }

    void Translation::ConvertPrivateToPublic(std::ifstream& in, std::ofstream& out) 
    {
        CompileContext ctx;
        std::string text {};
        TranslationInfo translationInfo;
        translationInfo.isComment = false;
        
        while (std::getline(in, text))
        {
            const size_t infoStart = text.find_first_not_of(' ');
            if (infoStart != std::string::npos && text[infoStart] == '"')
            {
                translationInfo.text = text.substr(infoStart + 1, text.find_last_of('"') - infoStart - 1);
                ctx.translationInfo.push_back(translationInfo);
            }
        }

        for (auto it = ctx.translationInfo.begin(); it != ctx.translationInfo.end(); ++it)
            out << it->text << "\n";
    }

    void Translation::ConvertPublicToPrivate(std::ifstream& in, std::ifstream& inMatch, std::ofstream& out)
    {
        // Iterate over lines in matching private translation file
        // Write new lines to output file (replacing strings, mainly)
        std::string text{};
        std::string textPublic{};
        while (std::getline(inMatch, text))
        {
            const size_t infoStart = text.find_first_not_of(' ');
            if (infoStart != std::string::npos && text[infoStart] == '"')
            {
                // Replace this string with public file's version of the string
                if (std::getline(in, textPublic))
                {
                    out << "\"" << textPublic << text.substr(text.find_last_of('"')) << "\n";
                }
                else
                {
                    // Matching private translation file has more strings than public - mismatch
                    std::cout << rang::fgB::red << "Private translation file has too many strings for this public file." << rang::fg::reset << std::endl;
                }
            }
            else
            {
                // Copy this line to output verbatim; not a string
                out << text << "\n";
            }
        }
    }

    void Translation::UpgradeFileToNewer(std::ifstream& in, bool isInputPrivate, std::ifstream& inNewer, std::ofstream& out)
    {
        std::unordered_map<int, std::string> inputStringsById {};
        std::string text{};

        if (isInputPrivate)
        {
            // Iterate over all lines of private input file, pair IDs to strings
            while (std::getline(in, text))
            {
                const size_t infoStart = text.find_first_not_of(' ');
                if (infoStart != std::string::npos && text[infoStart] == '"')
                {
                    // Ensure ID marking exists
                    const size_t idStart = text.find_last_of('&');
                    if (idStart < text.find_last_of('"') || idStart == std::string::npos)
                    {
                        std::cout << rang::fgB::red << "Missing string ID in private translation file!" << rang::fg::reset << std::endl;
                        exit(1);
                    }

                    // Parse ID
                    int id;
                    try
                    {
                        id = std::stoi(text.substr(idStart + 1), nullptr, 16);
                    }
                    catch (std::exception&)
                    {
                        std::cout << rang::fgB::red << "Invalid string ID format!" << rang::fg::reset << std::endl;
                        exit(1);
                    }

                    // Add to map
                    const std::string& contents = text.substr(infoStart + 1, text.find_last_of('"') - infoStart - 1);
                    inputStringsById[id] = contents;
                }
            }
        }
        else
        {
            // Iterate over all lines of public input file, and generate IDs based on line number
            int currentId = 0;
            while (std::getline(in, text))
            {
                inputStringsById[currentId] = text;
                currentId++;
            }
        }

        // Iterate over newer file's lines, write "upgraded" lines to output file
        while (std::getline(inNewer, text))
        {
            const size_t infoStart = text.find_first_not_of(' ');
            if (infoStart != std::string::npos && text[infoStart] == '"')
            {
                // Locate string ID in string
                const size_t idStart = text.find_last_of('&');
                if (idStart < text.find_last_of('"') || idStart == std::string::npos)
                {
                    // If missing the string ID in newer translation file, treat it as new; no need to actually crash
                    out << text << " [new]\n";
                    continue;
                }

                // Parse ID
                const std::string& idString = text.substr(idStart + 1, 8);
                int id;
                try
                {
                    id = std::stoi(idString, nullptr, 16);
                }
                catch (std::exception&)
                {
                    std::cout << rang::fgB::red << "Invalid string ID format!" << rang::fg::reset << std::endl;
                    exit(1);
                }

                auto& older = inputStringsById.find(id);
                if (older != inputStringsById.end())
                {
                    // If ID exists in older input, make the old input replace this line
                    out << "\"" << older->second << "\"&" << idString << "\n";
                }
                else
                {
                    // Otherwise, copy the line to output, and comment it as new
                    out << text << " [new]\n";
                }
            }
            else
            {
                // Copy line verbatim to the output; no need to do anything (it's not a string)
                out << text << "\n";
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