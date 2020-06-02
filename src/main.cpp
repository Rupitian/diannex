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
        ProjectFormat project;
        load_project(result["project"].as<std::string>(), project);
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
    }

    std::cout << "Parser testing" << std::endl;

    std::vector<Token> res = std::vector<Token>();
    auto start = std::chrono::high_resolution_clock::now();

    Lexer::LexString("//! Test string", res);
    std::unique_ptr<Node> parsed = Parser::ParseTokens(&res);

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "Took " << duration.count() << " milliseconds" << std::endl;
    std::cin.get();

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