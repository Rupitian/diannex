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
#include "Binary.h"
#include "Translation.h"

using namespace diannex;
namespace fs = std::filesystem;

cxxopts::ParseResult parse_options(int argc, char** argv, cxxopts::Options& options)
{
    try
    {
        auto res = options.parse(argc, argv);
        return res;
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl;
        std::cout << options.help() << std::endl;
        exit(1);
    }
}

void help(cxxopts::Options& options)
{
    std::cout << options.help() << "  --files                       File(s) to compile" << std::endl;
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
            ("convert", "Convert a private file to the public format")
            ("c,cli", "Don't use a project file and read commands from cli")
            ("h,help", "Shows this message");

    options
        .add_options("Conversion")
            ("in", "Path to private input file", cxxopts::value<std::string>())
            ("out", "Path to public output file", cxxopts::value<std::string>());

    options
        .add_options("Project")
            ("b,binary", "Directory to output binary", cxxopts::value<std::string>(), "(default: \"./out\")")
            ("n,name", "Name of output binary file", cxxopts::value<std::string>(), "(default: \"out\")")
            ("t,public", "Whether to output public translation file")
            ("N,pubname", "Name of output public translation file", cxxopts::value<std::string>(), "(default: \"out\")")
            ("T,private", "Whether to output private translation files")
            ("D,privname", "Name of output private translation file", cxxopts::value<std::string>(), "(default: \"out\")")
            ("d,privdir", "Directory to output private translation files", cxxopts::value<std::string>(), "(default: \"./translations\")")
            ("C,compress", "Whether or not to use compression")
            ("files", "File(s) to compile", cxxopts::value<std::vector<std::string>>()->default_value(""));


    options.parse_positional({ "files" });
    options.positional_help("<files>");

    auto result = parse_options(argc, argv, options);

    // Prevent incorrect usage
    if (result.count("project") && result.count("generate"))
    {
        help(options);
        std::cout << "\nCan't define --project and --generate at the same time!" << std::endl;
        return 1;
    }

    if (result.count("project") && result.count("convert"))
    {
        help(options);
        std::cout << "\nCan't define --project and --convert at the same time!" << std::endl;
        return 1;
    }

    if (result.count("generate") && result.count("convert"))
    {
        help(options);
        std::cout << "\nCan't define --generate and --convert at the same time!" << std::endl;
        return 1;
    }

    if (result.count("cli") && (result.count("project") || result.count("generate") || result.count("convert")))
    {
        help(options);
        std::cout << "\n--cli can't be used in conjunction with --project, --generate or --convert!" << std::endl;
        return 1;
    }

    if ((result.count("in") || result.count("out")) && !result.count("convert"))
    {
        help(options);
        std::cout << "\n--convert must be used with --in and --out" << std::endl;
        return 1;
    }

    // --generate
    if (result.count("generate"))
    {
        generate_project(result["generate"].as<std::string>());
        return 0;
    }

    // --convert
    if (result.count("convert"))
    {
        if (!result.count("in"))
        {
            help(options);
            std::cout << "\n--in is required for --convert!" << std::endl;
            return 1;
        }

        if (!result.count("out"))
        {
            help(options);
            std::cout << "\n--out is required for --convert!" << std::endl;
            return 1;
        }

        std::ifstream in;
        std::ofstream out;

        const std::filesystem::path& inResult = fs::absolute(result["in"].as<std::string>());
        const std::filesystem::path& outResult = fs::absolute(result["out"].as<std::string>());

        if (!fs::exists(inResult))
            fs::create_directories(inResult.parent_path());

        if (!fs::exists(outResult))
            fs::create_directories(outResult.parent_path());


        std::cout << "Converting..." << std::endl;

        in.open(inResult, std::ios_base::binary | std::ios_base::in);
        out.open(outResult, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

        if (!in.is_open())
        {
            std::cout << std::endl << rang::fgB::red << "Failed to open input private translation file for reading!" << rang::fg::reset << std::endl;
            return 1;
        }
        if (!out.is_open())
        {
            in.close();
            std::cout << std::endl << rang::fgB::red << "Failed to open output public translation file for writing!" << rang::fg::reset << std::endl;
            return 1;
        }

        Translation::ConvertPrivateToPublic(in, out);

        in.close();
        out.close();

        std::cout << "Completed!" << std::endl;

        return 0;
    }

    fs::path baseDirectory;

    // --project
    if (result.count("project"))
    {
        // TODO: Clean this up, perhaps put it in a class method
        std::string projectFilePath = result["project"].as<std::string>();
        baseDirectory = fs::absolute(projectFilePath).parent_path();
        load_project(projectFilePath, project);

        if (result["files"].count())
        {
            project.options.files = result["files"].as<std::vector<std::string>>();
        }

        if (result["binary"].count())
            project.options.binaryOutputDir = result["binary"].as<std::string>();
        if (result["name"].count())
            project.options.binaryName = result["name"].as<std::string>();
        if (result["public"].count())
            project.options.translationPublic = result["public"].as<bool>();
        if (result["private"].count())
            project.options.translationPrivate = result["private"].as<bool>();
        if (result["privdir"].count())
            project.options.translationPrivateOutDir = result["privdir"].as<std::string>();
        if (result["compress"].count())
            project.options.compression = result["compress"].as<bool>();

        loaded = true;
    }

    // --cli
    if (result.count("cli"))
    {
        project = ProjectFormat();
        project.options.files = result["files"].as<std::vector<std::string>>();
        for (auto& path : project.options.files)
        {
            path = fs::absolute(path).string();
        }
        project.options.binaryOutputDir = result["binary"].count() == 1 ? result["binary"].as<std::string>() : "./out";
        project.options.binaryName = result["name"].count() == 1 ? result["name"].as<std::string>() : "out";
        project.options.translationPublic = result["public"].count() == 1 ? result["public"].as<bool>() : false;
        project.options.translationPublicName = result["pubname"].count() == 1 ? result["pubname"].as<std::string>() : "out";
        project.options.translationPrivate = result["private"].count() == 1 ? result["private"].as<bool>() : false;
        project.options.translationPrivateName = result["privname"].count() == 1 ? result["privname"].as<std::string>() : "out";
        project.options.translationPrivateOutDir = result["privdir"].count() == 1 ? result["privdir"].as<std::string>() : "./translations";
        project.options.compression = result["compress"].count() == 1 ? result["compress"].as<bool>() : false;
        loaded = true;
    }

    if (!loaded)
    {
        help(options);
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

    // Write binary
    std::cout << "Writing binary..." << std::endl;
    const fs::path mainOutput = fs::absolute(project.options.binaryOutputDir);
    const std::string binaryName = (project.options.binaryName.empty() ? project.name : project.options.binaryName);
    const std::string fileName = binaryName + ".dxb";
    if (!fs::exists(mainOutput))
        fs::create_directories(mainOutput);
    {
        BinaryFileWriter bw((mainOutput / fileName).string());
        if (!bw.CanWrite())
        {
            std::cout << std::endl << rang::fgB::red << "Failed to open output binary file for writing!" << rang::fg::reset << std::endl;
            return 1;
        }
        if (!Binary::Write(&bw, &context))
        {
            std::cout << std::endl << rang::fgB::red << "Failed to compress with zlib!" << rang::fg::reset << std::endl;
            return 1;
        }
    }

    // Write translation files
    if (context.project->options.translationPublic)
    {
        std::cout << "Writing public translation file..." << std::endl;

        const std::string pubFileName = (project.options.translationPublicName.empty() ? binaryName : project.options.translationPublicName) + ".dxt";

        std::ofstream s;
        s.open((mainOutput / pubFileName).string(), std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
        if (s.is_open())
        {
            Translation::GeneratePublicFile(s, &context);
        }
        else
        {
            std::cout << std::endl << rang::fgB::red << "Failed to open output translation file for writing!" << rang::fg::reset << std::endl;
            return 1;
        }
        s.close();
    }

    if (context.project->options.translationPrivate) 
    {
        std::cout << "Writing private translation file..." << std::endl;

        const fs::path privateOutput = fs::absolute(project.options.translationPrivateOutDir);
        if (!fs::exists(privateOutput))
            fs::create_directories(privateOutput);
        
        const std::string privFileName = (project.options.translationPrivateName.empty() ? binaryName : project.options.translationPrivateName) + ".dxt";

        std::ofstream s;
        s.open((privateOutput / privFileName).string(), std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
        if (s.is_open())
        {
            Translation::GeneratePrivateFile(s, &context);
        }
        else
        {
            std::cout << std::endl << rang::fgB::red << "Failed to open output translation file for writing!\nMake sure that all proper directories exist." << rang::fg::reset << std::endl;
            return 1;
        }
        s.close();
    }

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