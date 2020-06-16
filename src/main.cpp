#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <exception>

#include "libs/cxxopts.hpp"

#include "Lexer.h"
#include "Parser.h"
#include "Project.h"
#include "Utility.h"

using namespace diannex;
namespace fs = std::filesystem;

cxxopts::ParseResult parse_options(int argc, char** argv, cxxopts::Options& options)
{
    try
    {
        return options.parse(argc, argv);
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl;
        std::cout << options.help() << std::endl;
        exit(1);
    }
}

int main(int argc, char** argv)
{
    ProjectFormat project;
    bool loaded = false;
    bool fatalError = false;

    cxxopts::Options options("diannex", "Universal tool for the diannex dialogue system");

    options
        .add_options()
            ("p,project", "Load project file", cxxopts::value<std::string>())
            ("g,generate", "Generate new project file", cxxopts::value<std::string>()->implicit_value(fs::current_path().filename().string()))
            ("h,help", "Shows this message");

    auto result = parse_options(argc, argv, options);

    if (result.count("project") && result.count("generate"))
    {
        std::cout << options.help() << "\nCan't define --project and --generate at the same time!" << std::endl;
        return 1;
    }

    if (result.count("generate"))
    {
        generate_project(result["generate"].as<std::string>());
        return 0;
    }

    fs::path baseDirectory;

    if (result.count("project"))
    {
        // TODO: Clean this up, perhaps put it in a class method
        std::string projectFilePath = result["project"].as<std::string>();
        baseDirectory = fs::absolute(projectFilePath).parent_path();
        load_project(projectFilePath, project);
        print_project(projectFilePath, project);
        loaded = true;
    }

    if (!loaded)
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::cout << "Beginning compilation process..." << std::endl;
    std::cout << "Currently: lexer testing" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    LexerContext context;
    context.project = &project;
    for (auto& file : project.options.files)
        context.queue.push(file);

    while (!context.queue.empty())
    {
        std::string file = (baseDirectory / context.queue.front()).string();
        context.queue.pop();
        std::string buf;
        try
        {
            if (!fs::exists(file))
                throw std::runtime_error("File does not exist.");
            if (context.tokenList.find(file) != context.tokenList.end())
                continue; // Already tokenized this file
            std::ifstream f(file, std::ios::in);
            f.seekg(0, std::ios::end);
            buf.reserve(f.tellg());
            f.seekg(0, std::ios::beg);

            buf.assign((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to read file '" << file << "': " << e.what() << std::endl;
            fatalError = true;
            continue;
        }
        context.currentFile = file;
        std::vector<Token> tokens;
        Lexer::LexString(buf, context, tokens);

        // TESTING
        ParseResult parsed = Parser::ParseTokens(&tokens);

        context.tokenList.insert(std::make_pair(file, tokens));
    }

    if (fatalError)
    {
        std::cout << "Not proceeding with compilation due to fatal errors." << std::endl;
        return 1;
    }

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "Took " << duration.count() << " milliseconds" << std::endl;

    for (auto& tokens : context.tokenList)
    {
        std::cout << std::endl << "Tokens from '" << tokens.first << "':" << std::endl;
        for (auto& token : tokens.second)
        {
            std::cout << token << std::endl;
        }
    }
    
    return 0;
}