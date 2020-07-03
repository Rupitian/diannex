#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <exception>

#include <libs/cxxopts.hpp>
#include <libs/rang.hpp>

#include "Lexer.h"
#include "Parser.h"
#include "Bytecode.h"
#include "Project.h"
#include "Utility.h"
#include "Context.h"

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

    auto start = std::chrono::high_resolution_clock::now();

    CompileContext context;
    context.project = &project;
    for (auto& file : project.options.files)
        context.queue.push(file);

    // Load all of the files in the queue and lex them into tokens
    std::cout << "Lexing..." << std::endl;
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
            std::cout << rang::fg::red << "Failed to read file '" << file << "': " << e.what() << rang::fg::reset << std::endl;
            fatalError = true;
            continue;
        }
        context.currentFile = file;
        std::vector<Token> tokens;
        Lexer::LexString(buf, &context, tokens);

        context.tokenList.insert(std::make_pair(file, tokens));
    }

    if (fatalError)
    {
        std::cout << std::endl << rang::fgB::red << "Not proceeding with compilation due to fatal errors." << rang::fg::reset << std::endl;
        return 1;
    }

    // Parse each token stream
    std::cout << "Parsing..." << std::endl;
    for (auto& kvp : context.tokenList)
    {
        ParseResult* parsed = Parser::ParseTokens(&context, &kvp.second);
        if (parsed->errors.size() != 0)
        {
            if (!fatalError)
            {
                std::cout << rang::fgB::red << std::endl << "Encountered errors while parsing:" << rang::fg::reset << std::endl;
                fatalError = true;
            }

            std::cout << rang::fg::red;

            for (ParseError& e : parsed->errors)
            {
                std::cout << "[" << kvp.first << ":" << e.line << ":" << e.column << "] ";
                switch (e.type)
                {
                case ParseError::ErrorType::ExpectedTokenButGot:
                    std::cout << "Expected token " << e.info1 << " but got " << e.info2 << "." << std::endl;
                    break;
                case ParseError::ErrorType::ExpectedTokenButEOF:
                    std::cout << "Expected token " << e.info1 << " but reached end of code." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedToken:
                    std::cout << "Unexpected token " << e.info1 << "." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedModifierFor:
                    std::cout << "Unexpected modifier for " << e.info1 << "." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedMarkedString:
                    std::cout << "Unexpected MarkedString token." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedEOF:
                    std::cout << "Unexpected end of code." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedSwitchCase:
                    std::cout << "Unexpected switch 'case' keyword." << std::endl;
                    break;
                case ParseError::ErrorType::UnexpectedSwitchDefault:
                    std::cout << "Unexpected switch 'default' keyword." << std::endl;
                    break;
                case ParseError::ErrorType::ChooseWithoutStatement:
                    std::cout << "Choose statement without any sub-statements." << std::endl;
                    break;
                case ParseError::ErrorType::ChoiceWithoutStatement:
                    std::cout << "Choice statement without any sub-statements." << std::endl;
                    break;
                }
            }

            std::cout << rang::fg::reset;
        }
        else
        {
            context.parseList.insert(std::make_pair(kvp.first, parsed));
        }
    }

    if (fatalError)
    {
        std::cout << std::endl << rang::fgB::red << "Not proceeding with compilation due to fatal errors." << rang::fg::reset << std::endl;
        return 1;
    }

    // Generate bytecode
    std::cout << "Generating bytecode..." << std::endl;
    for (auto& kvp : context.parseList)
    {
        BytecodeResult* bytecode = Bytecode::Generate(kvp.second, &context);
        if (bytecode->errors.size() != 0)
        {
            if (!fatalError)
            {
                std::cout << rang::fgB::red << std::endl << "Encountered errors while generating bytecode:" << rang::fg::reset << std::endl;
                fatalError = true;
            }

            std::cout << rang::fg::red;

            for (BytecodeError& e : bytecode->errors)
            {
                if (e.line == 0 && e.column == 0)
                    std::cout << "[" << kvp.first << ":?:?] ";
                else
                    std::cout << "[" << kvp.first << ":" << e.line << ":" << e.column << "] ";
                
                switch (e.type)
                {
                case BytecodeError::ErrorType::SceneAlreadyExists:
                    std::cout << "Duplicate scene name '" << e.info1 << "'." << std::endl;
                    break;
                case BytecodeError::ErrorType::FunctionAlreadyExists:
                    std::cout << "Duplicate function name '" << e.info1 << "'." << std::endl;
                    break;
                case BytecodeError::ErrorType::DefinitionBlockAlreadyExists:
                    std::cout << "Duplicate definition block name '" << e.info1 << "'." << std::endl;
                    break;
                case BytecodeError::ErrorType::LocalVariableAlreadyExists:
                    std::cout << "Local variable '" << e.info1 << "' already defined." << std::endl;
                    break;
                case BytecodeError::ErrorType::ContinueOutsideOfLoop:
                    std::cout << "Continue statement outside of a loop." << std::endl;
                    break;
                case BytecodeError::ErrorType::BreakOutsideOfLoop:
                    std::cout << "Break statement outside of a loop or switch statement." << std::endl;
                    break;
                case BytecodeError::ErrorType::StatementsBeforeSwitchCase:
                    std::cout << "Statements present before any cases in switch statement." << std::endl;
                    break;
                }
            }
        }
    }

    if (fatalError)
    {
        std::cout << std::endl << rang::fgB::red << "Not proceeding with compilation due to fatal errors." << rang::fg::reset << std::endl;
        return 1;
    }

    // TODO write binary

    // TODO write translation files

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << rang::fgB::green;
    if (project.options.compileFinishMessage.size() != 0)
        std::cout << project.options.compileFinishMessage << std::endl;
    else
        std::cout << "Finished! ";
    std::cout << "Took " << duration.count() << " milliseconds." << rang::fg::reset << std::endl;

    return 0;
}