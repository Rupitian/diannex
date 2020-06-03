#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>

#include "libs/json.hpp"
#include "libs/cxxopts.hpp"

#include "Lexer.h"
#include "Parser.h"
#include "Project.h"

using namespace diannex;
namespace fs = std::filesystem;

void generate_project(std::string name);
void load_project(std::string path, ProjectFormat& proj);

int main(int argc, char** argv)
{
    ProjectFormat project;
    bool loaded = false;

    cxxopts::Options options("diannex", "Universal tool for the diannex dialogue system");

    options.add_options()
            ("p,project", "Load project file", cxxopts::value<std::string>())
            ("g,generate", "Generate new project file", cxxopts::value<std::string>()->implicit_value(fs::current_path().filename().string()));

    auto result = options.parse(argc, argv);

    if (result.count("project") && result.count("generate"))
    {
        std::cout << options.help() << "\nCan't define --project and --generate at the same time!" << std::endl;
        return -1;
    }

    if (result.count("generate"))
    {
        generate_project(result["generate"].as<std::string>());
        return 0;
    }

    if (result.count("project"))
    {
        // TODO: Clean this up, perhaps put it in a class method
        load_project(result["project"].as<std::string>(), project);
#pragma region Print Project
        std::cout << result["project"].as<std::string>() << ":"
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
        for (int i = 0; i < project.options.macros.size(); i++)
        {
            if (i != 0) std::cout << ", ";
            std::cout << project.options.macros[i];
        }
        std::cout << " ]" << std::endl;
#pragma endregion
        loaded = true;
    }

    if (!loaded) return 0;

    std::cout << "Lexer testing" << std::endl;

    std::vector<Token> res = std::vector<Token>();
    auto start = std::chrono::high_resolution_clock::now();

    for (auto& file : project.options.files)
    {
        std::string buf, line;
        std::ifstream f(file);
        while (std::getline(f, line))
        {
            buf += line;
            buf.push_back('\n');
        }
        Lexer::LexString(buf, res);
    }
    //std::unique_ptr<Node> parsed = Parser::ParseTokens(&res);

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "Took " << duration.count() << " milliseconds\n" << std::endl;
    std::cout << "Tokens:" << std::endl;

    for (auto& token : res)
    {
        // TODO: Make this a << overload on Token?
        std::cout << "[" << token.line << ":" << token.column << "] ";
        switch (token.type)
        {
            case Identifier:
                std::cout << "Identifier";
                break;
            case Number:
                std::cout << "Number";
                break;
            case Percentage:
                std::cout << "Percentage";
                break;
            case String:
                std::cout << "String";
                break;
            case MarkedString:
                std::cout << "MarkedString";
                break;
            case ExcludeString:
                std::cout << "ExcludeString";
                break;
            case GroupKeyword:
                std::cout << "GroupKeyword";
                break;
            case MainKeyword:
                std::cout << "MainKeyword";
                break;
            case MainSubKeyword:
                std::cout << "MainSubKeyword";
                break;
            case ModifierKeyword:
                std::cout << "ModifierKeyword";
                break;
            case OpenParen:
                std::cout << "OpenParen";
                break;
            case CloseParen:
                std::cout << "CloseParen";
                break;
            case OpenCurly:
                std::cout << "OpenCurly";
                break;
            case CloseCurly:
                std::cout << "CloseCurly";
                break;
            case OpenBrack:
                std::cout << "OpenBrack";
                break;
            case CloseBrack:
                std::cout << "CloseBrack";
                break;
            case Semicolon:
                std::cout << "Semicolon";
                break;
            case Colon:
                std::cout << "Colon";
                break;
            case Comma:
                std::cout << "Comma";
                break;
            case Ternary:
                std::cout << "Ternary";
                break;
            case VariableStart:
                std::cout << "VariableStart";
                break;
            case Equals:
                std::cout << "Equals";
                break;
            case Plus:
                std::cout << "Plus";
                break;
            case Increment:
                std::cout << "Increment";
                break;
            case PlusEquals:
                std::cout << "PlusEquals";
                break;
            case Minus:
                std::cout << "Minus";
                break;
            case Decrement:
                std::cout << "Decrement";
                break;
            case MinusEquals:
                std::cout << "MinusEquals";
                break;
            case Multiply:
                std::cout << "Multiply";
                break;
            case Power:
                std::cout << "Power";
                break;
            case MultiplyEquals:
                std::cout << "MultiplyEquals";
                break;
            case Divide:
                std::cout << "Divide";
                break;
            case DivideEquals:
                std::cout << "DivideEquals";
                break;
            case Mod:
                std::cout << "Mod";
                break;
            case ModEquals:
                std::cout << "ModEquals";
                break;
            case Not:
                std::cout << "Not";
                break;
            case CompareEQ:
                std::cout << "CompareEQ";
                break;
            case CompareGT:
                std::cout << "CompareGT";
                break;
            case CompareLT:
                std::cout << "CompareLT";
                break;
            case CompareGTE:
                std::cout << "CompareGTE";
                break;
            case CompareLTE:
                std::cout << "CompareLTE";
                break;
            case CompareNEQ:
                std::cout << "CompareNEQ";
                break;
            case LogicalAnd:
                std::cout << "LogicalAnd";
                break;
            case LogicalOr:
                std::cout << "LogicalOr";
                break;
            case BitwiseLShift:
                std::cout << "BitwiseLShift";
                break;
            case BitwiseRShift:
                std::cout << "BitwiseRShift";
                break;
            case BitwiseAnd:
                std::cout << "BitwiseAnd";
                break;
            case BitwiseAndEquals:
                std::cout << "BitwiseAndEquals";
                break;
            case BitwiseOr:
                std::cout << "BitwiseOr";
                break;
            case BitwiseOrEquals:
                std::cout << "BitwiseOrEquals";
                break;
            case BitwiseXor:
                std::cout << "BitwiseXor";
                break;
            case BitwiseXorEquals:
                std::cout << "BitwiseXorEquals";
                break;
            case BitwiseNegate:
                std::cout << "BitwiseNegate";
                break;
            case Directive:
                std::cout << "Directive";
                break;
            case MarkedComment:
                std::cout << "MarkedComment";
                break;
            case Error:
                std::cout << "Error";
                break;
            case ErrorString:
                std::cout << "ErrorString";
                break;
            case ErrorUnenclosedString:
                std::cout << "ErrorUnenclosedString";
                break;
        }

        std::cout << ": ";

        if (token.type >= TokenType::GroupKeyword && token.type <= TokenType::ModifierKeyword)
        {
            switch (token.keywordType)
            {
                case None:
                    std::cout << "None";
                    break;
                case Namespace:
                    std::cout << "Namespace";
                    break;
                case Scene:
                    std::cout << "Scene";
                    break;
                case Def:
                    std::cout << "Def";
                    break;
                case Func:
                    std::cout << "Func";
                    break;
                case Choice:
                    std::cout << "Choice";
                    break;
                case Choose:
                    std::cout << "Choose";
                    break;
                case If:
                    std::cout << "If";
                    break;
                case Else:
                    std::cout << "Else";
                    break;
                case While:
                    std::cout << "While";
                    break;
                case For:
                    std::cout << "For";
                    break;
                case Do:
                    std::cout << "Do";
                    break;
                case Repeat:
                    std::cout << "Repeat";
                    break;
                case Switch:
                    std::cout << "Switch";
                    break;
                case Continue:
                    std::cout << "Continue";
                    break;
                case Break:
                    std::cout << "Break";
                    break;
                case Return:
                    std::cout << "Return";
                    break;
                case Require:
                    std::cout << "Require";
                    break;
                case Chance:
                    std::cout << "Chance";
                    break;
                case Local:
                    std::cout << "Local";
                    break;
                case Global:
                    std::cout << "Global";
                    break;
                case Macro:
                    std::cout << "Macro";
                    break;
                case Include:
                    std::cout << "Include";
                    break;
                case Exclude:
                    std::cout << "Exclude";
                    break;
                case IfDef:
                    std::cout << "IfDef";
                    break;
                case EndIf:
                    std::cout << "EndIf";
                    break;
            }
        }
        else
        {
            std::cout << token.content;
        }
        
        std::cout << std::endl;
    }
    
    return 0;
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

    std::ofstream ofs(name + ".json", std::ios::out);
    ofs << project.dump(4);
    ofs.close();
}

void load_project(std::string path, ProjectFormat& proj)
{
    std::ifstream ifs(path, std::ios::out);
    nlohmann::json project;
    ifs >> project;
    ifs.close();

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
            proj.options.macros.push_back(macro.get<std::string>());
        }
    }
}