#include "Utility.h"

#include <fstream>
#include <iostream>
#include <filesystem>

#include <libs/json.hpp>

#include "Lexer.h"

namespace fs = std::filesystem;

namespace diannex
{
    void print_project(const std::string& path, const ProjectFormat& project)
    {
        std::cout << path << ":"
                  << "\n\tProject Name: " << project.name
                  << "\n\tProject Authors: [ ";
        for (int i = 0; i < project.authors.size(); i++)
        {
            if (i != 0) std::cout << ", ";
            std::cout << project.authors[i];
        }
        std::cout << " ]"
                  << "\n\tProject Options:"
                  << "\n\t\tCompile Finish Message: " << project.options.compileFinishMessage
                  << "\n\t\tFiles: [ ";
        for (int i = 0; i < project.options.files.size(); i++)
        {
            if (i != 0) std::cout << ", ";
            std::cout << project.options.files[i];
        }
        std::cout << " ]"
                  << "\n\t\tInterpolation Flags:"
                  << "\n\t\t\tSymbol: " << project.options.interpolationFlags.symbol
                  << "\n\t\tTranslation Output: " << project.options.translationOutput
                  << "\n\t\tMacros: [ ";
        bool comma = false;
        for (auto& macro : project.options.macros)
        {
            if (comma)
                std::cout << ", ";
            else
                comma = true;

            std::cout << macro.first << '=' << macro.second;
        }
        std::cout << " ]" << std::endl;
    }

    void generate_project(std::string name)
    {
        nlohmann::json project = {
                {"name", name},
                {"authors", nlohmann::json::array()},
                {"options", {
                                 {"compile_finish_message", ""},
                                 {"files", {"main.dx"}},
                                 {"interpolation_flags", {
                                                                 {"symbol", "$"}
                                                         }},
                                 {"translation_output", "./translations"},
                                 {"macros", nlohmann::json::array()}
                         }}
        };

        const std::string path = name + ".json";
        try
        {
            std::ofstream ofs(path, std::ios::out);
            ofs << project.dump(4);
            ofs.close();
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to write project file at '" << path + "': " << e.what() << std::endl;
            exit(1);
        }
    }

    void load_project(std::string path, ProjectFormat& proj)
    {
        nlohmann::json project;
        try
        {
            if (!fs::exists(path))
                throw std::runtime_error("File does not exist.");
            std::ifstream ifs(path, std::ios::in);
            ifs >> project;
            ifs.close();
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load project file: " << e.what() << std::endl;
            exit(1);
        }

        proj.name = project.contains("name") ?
                    project["name"].get<std::string>() :
                    fs::path(path).filename().stem().string();

        if (project.contains("authors"))
        {
            for (auto& author : project["authors"])
            {
                proj.authors.push_back(author.get<std::string>());
            }
        }
        else if (project.contains("author"))
        {
            proj.authors.push_back(project["author"].get<std::string>());
        }

        if (!project.contains("options"))
        {
            proj.options.files.emplace_back("main.dx");
            proj.options.interpolationFlags.symbol = "$";
            proj.options.translationOutput = "./translations";
            return;
        }

        proj.options.compileFinishMessage = project["options"].contains("compile_finish_message") ?
                                            project["options"]["compile_finish_message"].get<std::string>() :
                                            "";

        if (project["options"].contains("files"))
        {
            for(auto& file : project["options"]["files"])
            {
                proj.options.files.push_back(file.get<std::string>());
            }
        }
        else
        {
            proj.options.files.emplace_back("main.dx");
        }

        proj.options.interpolationFlags.symbol = project["options"].contains("interpolation_flags") && project["options"]["interpolation_flags"].contains("symbol") ?
                                                 project["options"]["interpolation_flags"]["symbol"].get<std::string>() :
                                                 "$";

        proj.options.translationOutput = project["options"].contains("translation_output") ?
                                         project["options"]["translation_output"].get<std::string>() :
                                         "./translations";

        if (project["options"].contains("macros"))
        {
            for (auto& macro : project["options"]["macros"])
            {
                auto macro_str = macro.get<std::string>();
                auto pos = macro_str.find('=');
                if (pos == std::string::npos)
                {
                    proj.options.macros.insert(std::make_pair(macro_str, ""));
                }
                else
                {
                    proj.options.macros.insert(std::make_pair(macro_str.substr(0, pos), macro_str.substr(pos+1)));
                }
            }
        }
    }
}

std::ostream& operator<<(std::ostream& o, const diannex::Token& t)
{
    o << '[' << t.line << ':' << t.column << "] ";
    switch (t.type)
    {
        case diannex::Identifier:
            o << "Identifier";
            break;
        case diannex::Number:
            o << "Number";
            break;
        case diannex::Percentage:
            o << "Percentage";
            break;
        case diannex::String:
            o << "String";
            break;
        case diannex::MarkedString:
            o << "MarkedString";
            break;
        case diannex::ExcludeString:
            o << "ExcludeString";
            break;
        case diannex::GroupKeyword:
            o << "GroupKeyword";
            break;
        case diannex::MainKeyword:
            o << "MainKeyword";
            break;
        case diannex::MainSubKeyword:
            o << "MainSubKeyword";
            break;
        case diannex::ModifierKeyword:
            o << "ModifierKeyword";
            break;
        case diannex::OpenParen:
            o << "OpenParen";
            break;
        case diannex::CloseParen:
            o << "CloseParen";
            break;
        case diannex::OpenCurly:
            o << "OpenCurly";
            break;
        case diannex::CloseCurly:
            o << "CloseCurly";
            break;
        case diannex::OpenBrack:
            o << "OpenBrack";
            break;
        case diannex::CloseBrack:
            o << "CloseBrack";
            break;
        case diannex::Semicolon:
            o << "Semicolon";
            break;
        case diannex::Colon:
            o << "Colon";
            break;
        case diannex::Comma:
            o << "Comma";
            break;
        case diannex::Ternary:
            o << "Ternary";
            break;
        case diannex::VariableStart:
            o << "VariableStart";
            break;
        case diannex::Equals:
            o << "Equals";
            break;
        case diannex::Plus:
            o << "Plus";
            break;
        case diannex::Increment:
            o << "Increment";
            break;
        case diannex::PlusEquals:
            o << "PlusEquals";
            break;
        case diannex::Minus:
            o << "Minus";
            break;
        case diannex::Decrement:
            o << "Decrement";
            break;
        case diannex::MinusEquals:
            o << "MinusEquals";
            break;
        case diannex::Multiply:
            o << "Multiply";
            break;
        case diannex::Power:
            o << "Power";
            break;
        case diannex::MultiplyEquals:
            o << "MultiplyEquals";
            break;
        case diannex::Divide:
            o << "Divide";
            break;
        case diannex::DivideEquals:
            o << "DivideEquals";
            break;
        case diannex::Mod:
            o << "Mod";
            break;
        case diannex::ModEquals:
            o << "ModEquals";
            break;
        case diannex::Not:
            o << "Not";
            break;
        case diannex::CompareEQ:
            o << "CompareEQ";
            break;
        case diannex::CompareGT:
            o << "CompareGT";
            break;
        case diannex::CompareLT:
            o << "CompareLT";
            break;
        case diannex::CompareGTE:
            o << "CompareGTE";
            break;
        case diannex::CompareLTE:
            o << "CompareLTE";
            break;
        case diannex::CompareNEQ:
            o << "CompareNEQ";
            break;
        case diannex::LogicalAnd:
            o << "LogicalAnd";
            break;
        case diannex::LogicalOr:
            o << "LogicalOr";
            break;
        case diannex::BitwiseLShift:
            o << "BitwiseLShift";
            break;
        case diannex::BitwiseRShift:
            o << "BitwiseRShift";
            break;
        case diannex::BitwiseAnd:
            o << "BitwiseAnd";
            break;
        case diannex::BitwiseAndEquals:
            o << "BitwiseAndEquals";
            break;
        case diannex::BitwiseOr:
            o << "BitwiseOr";
            break;
        case diannex::BitwiseOrEquals:
            o << "BitwiseOrEquals";
            break;
        case diannex::BitwiseXor:
            o << "BitwiseXor";
            break;
        case diannex::BitwiseXorEquals:
            o << "BitwiseXorEquals";
            break;
        case diannex::BitwiseNegate:
            o << "BitwiseNegate";
            break;
        case diannex::Directive:
            o << "Directive";
            break;
        case diannex::MarkedComment:
            o << "MarkedComment";
            break;
        case diannex::Error:
            o << "Error";
            break;
        case diannex::ErrorString:
            o << "ErrorString";
            break;
        case diannex::ErrorUnenclosedString:
            o << "ErrorUnenclosedString";
            break;
        case diannex::Newline:
            o << "Newline";
            break;
    }
    
    if (!t.content.empty() || (t.type >= diannex::GroupKeyword && t.type <= diannex::ModifierKeyword))
        o << ": ";
    
    if (t.type >= diannex::GroupKeyword && t.type <= diannex::ModifierKeyword)
    {
        switch (t.keywordType)
        {
            case diannex::None:
                o << "None";
                break;
            case diannex::Namespace:
                o << "Namespace";
                break;
            case diannex::Scene:
                o << "Scene";
                break;
            case diannex::Def:
                o << "Def";
                break;
            case diannex::Func:
                o << "Func";
                break;
            case diannex::Choice:
                o << "Choice";
                break;
            case diannex::Choose:
                o << "Choose";
                break;
            case diannex::If:
                o << "If";
                break;
            case diannex::Else:
                o << "Else";
                break;
            case diannex::While:
                o << "While";
                break;
            case diannex::For:
                o << "For";
                break;
            case diannex::Do:
                o << "Do";
                break;
            case diannex::Repeat:
                o << "Repeat";
                break;
            case diannex::Switch:
                o << "Switch";
                break;
            case diannex::Continue:
                o << "Continue";
                break;
            case diannex::Break:
                o << "Break";
                break;
            case diannex::Return:
                o << "Return";
                break;
            case diannex::Require:
                o << "Require";
                break;
            case diannex::Local:
                o << "Local";
                break;
            case diannex::Global:
                o << "Global";
                break;
            case diannex::Include:
                o << "Include";
                break;
            case diannex::IfDef:
                o << "IfDef";
                break;
            case diannex::IfNDef:
                o << "IfNDef";
                break;
            case diannex::EndIf:
                o << "EndIf";
                break;
        }
    }
    else
    {
        o << t.content;
    }
    
    return o;
}