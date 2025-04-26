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
#include "ParseResult.h"

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
            ("c,cli", "Don't use a project file and read commands from cli")
            ("h,help", "Shows this message");

    options
        .add_options("Translation conversion")
            ("convert", "Convert a translation file from private to public, or vice versa")
            ("upgrade", "Upgrade a translation file to a newer version")
            ("to_binary", "Convert a public (or private) translation file to a binary format")
            ("in_private", "Path to private input file", cxxopts::value<std::string>())
            ("in_public", "Path to public input file", cxxopts::value<std::string>())
            ("out", "Path to output file", cxxopts::value<std::string>())
            ("in_newer", "Path to newer private input file", cxxopts::value<std::string>())
            ("in_match", "Path to matching private input file", cxxopts::value<std::string>());

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
    std::vector<std::string> mainCommands { "project", "generate", "convert", "upgrade", "to_binary", "cli" };
    bool foundMainCommand = false;
    for (auto& command : mainCommands)
    {
        if (result.count(command))
        {
            if (foundMainCommand)
            {
                // We already found a main command!
                help(options);
                std::cout << "\nToo many main commands specified!" << std::endl;
                return 1;
            }
            foundMainCommand = true;
        }
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
        if (!result.count("out"))
        {
            help(options);
            std::cout << "\n--out is required for --convert!" << std::endl;
            return 1;
        }

        if (result.count("in_private"))
        {
            std::cout << "Converting..." << std::endl;

            const std::filesystem::path& input = fs::absolute(result["in_private"].as<std::string>());
            const std::filesystem::path& outResult = fs::absolute(result["out"].as<std::string>());

            // Ensure directories exist
            if (!fs::exists(input))
                fs::create_directories(input.parent_path());
            if (!fs::exists(outResult))
                fs::create_directories(outResult.parent_path());

            // Open files, check that they were opened properly
            std::ifstream in;
            std::ofstream out;
            in.open(input, std::ios_base::binary | std::ios_base::in);
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

            // Actual operation
            Translation::ConvertPrivateToPublic(in, out);

            // Close files we opened earlier
            in.close();
            out.close();
        }
        else if (result.count("in_public"))
        {
            if (!result.count("in_match"))
            {
                help(options);
                std::cout << "\n--in_match is required for --convert and --in_public!" << std::endl;
                return 1;
            }

            std::cout << "Converting..." << std::endl;

            const std::filesystem::path& input = fs::absolute(result["in_public"].as<std::string>());
            const std::filesystem::path& inputMatch = fs::absolute(result["in_match"].as<std::string>());
            const std::filesystem::path& outResult = fs::absolute(result["out"].as<std::string>());

            // Ensure directories exist
            if (!fs::exists(input))
                fs::create_directories(input.parent_path());
            if (!fs::exists(inputMatch))
                fs::create_directories(inputMatch.parent_path());
            if (!fs::exists(outResult))
                fs::create_directories(outResult.parent_path());

            // Open files, check that they were opened properly
            std::ifstream in;
            std::ifstream inMatch;
            std::ofstream out;
            in.open(input, std::ios_base::binary | std::ios_base::in);
            inMatch.open(inputMatch, std::ios_base::binary | std::ios_base::in);
            out.open(outResult, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
            if (!in.is_open())
            {
                std::cout << std::endl << rang::fgB::red << "Failed to open input public translation file for reading!" << rang::fg::reset << std::endl;
                return 1;
            }
            if (!inMatch.is_open())
            {
                in.close();
                std::cout << std::endl << rang::fgB::red << "Failed to open input matching translation file for reading!" << rang::fg::reset << std::endl;
                return 1;
            }
            if (!out.is_open())
            {
                in.close();
                inMatch.close();
                std::cout << std::endl << rang::fgB::red << "Failed to open output private translation file for writing!" << rang::fg::reset << std::endl;
                return 1;
            }

            // Actual operation
            Translation::ConvertPublicToPrivate(in, inMatch, out);

            // Close files we opened earlier
            in.close();
            inMatch.close();
            out.close();
        }
        else
        {
            help(options);
            std::cout << "\n--in_private or --in_public is required for --convert!" << std::endl;
            return 1;
        }

        std::cout << "Completed!" << std::endl;

        return 0;
    }

    // -upgrade
    if (result.count("upgrade"))
    {
        bool isInputPrivate = result.count("in_private");
        if (result.count("in_public"))
        {
            if (isInputPrivate)
            {
                help(options);
                std::cout << "\n--in_private and --in_public cannot be used simultaneously!" << std::endl;
                return 1;
            }
        }
        else if (!isInputPrivate)
        {
            help(options);
            std::cout << "\n--in_private or --in_public must be specified!" << std::endl;
            return 1;
        }
        if (!result.count("in_newer"))
        {
            help(options);
            std::cout << "\n--in_newer is required for --upgrade!" << std::endl;
            return 1;
        }
        if (!result.count("out"))
        {
            help(options);
            std::cout << "\n--out is required for --upgrade!" << std::endl;
            return 1;
        }

        std::cout << "Upgrading..." << std::endl;

        const std::filesystem::path& input = fs::absolute(result[isInputPrivate ? "in_private" : "in_public"].as<std::string>());
        const std::filesystem::path& inputNewer = fs::absolute(result["in_newer"].as<std::string>());
        const std::filesystem::path& outResult = fs::absolute(result["out"].as<std::string>());

        // Ensure directories exist
        if (!fs::exists(input))
            fs::create_directories(input.parent_path());
        if (!fs::exists(inputNewer))
            fs::create_directories(inputNewer.parent_path());
        if (!fs::exists(outResult))
            fs::create_directories(outResult.parent_path());

        // Open files, check that they were opened properly
        std::ifstream in;
        std::ifstream inNewer;
        std::ofstream out;
        in.open(input, std::ios_base::binary | std::ios_base::in);
        inNewer.open(inputNewer, std::ios_base::binary | std::ios_base::in);
        out.open(outResult, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
        if (!in.is_open())
        {
            std::cout << std::endl << rang::fgB::red << "Failed to open input translation file for reading!" << rang::fg::reset << std::endl;
            return 1;
        }
        if (!inNewer.is_open())
        {
            in.close();
            std::cout << std::endl << rang::fgB::red << "Failed to open newer input translation file for reading!" << rang::fg::reset << std::endl;
            return 1;
        }
        if (!out.is_open())
        {
            in.close();
            inNewer.close();
            std::cout << std::endl << rang::fgB::red << "Failed to open output translation file for writing!" << rang::fg::reset << std::endl;
            return 1;
        }

        // Actual operation
        Translation::UpgradeFileToNewer(in, isInputPrivate, inNewer, out);

        // Close files we opened earlier
        in.close();
        inNewer.close();
        out.close();

        std::cout << "Completed!" << std::endl;

        return 0;
    }

    // --to_binary
    if (result.count("to_binary"))
    {
        bool isInputPrivate = result.count("in_private");
        if (result.count("in_public"))
        {
            if (isInputPrivate)
            {
                help(options);
                std::cout << "\n--in_private and --in_public cannot be used simultaneously!" << std::endl;
                return 1;
            }
        }
        else if (!isInputPrivate)
        {
            help(options);
            std::cout << "\n--in_private or --in_public must be specified!" << std::endl;
            return 1;
        }
        if (!result.count("out"))
        {
            help(options);
            std::cout << "\n--out is required for --to_binary!" << std::endl;
            return 1;
        }

        std::cout << "Converting to binary format..." << std::endl;

        const std::filesystem::path& input = fs::absolute(result[isInputPrivate ? "in_private" : "in_public"].as<std::string>());
        const std::filesystem::path& outResult = fs::absolute(result["out"].as<std::string>());

        // Ensure directories exist
        if (!fs::exists(input))
            fs::create_directories(input.parent_path());
        if (!fs::exists(outResult))
            fs::create_directories(outResult.parent_path());

        {
            // Open files, check that they were opened properly
            std::ifstream in;
            BinaryFileWriter out(outResult.string());
            in.open(input, std::ios_base::binary | std::ios_base::in);
            if (!in.is_open())
            {
                std::cout << std::endl << rang::fgB::red << "Failed to open input translation file for reading!" << rang::fg::reset << std::endl;
                return 1;
            }
            if (!out.CanWrite())
            {
                in.close();
                std::cout << std::endl << rang::fgB::red << "Failed to open output binary file for writing!" << rang::fg::reset << std::endl;
                return 1;
            }

            // Actual operation
            Translation::ConvertToBinary(in, isInputPrivate, out);

            // Close file we opened earlier (BinaryFileWriter closes upon exiting scope)
            in.close();
        }

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
    {
#if DIANNEX_OLD_INCLUDE_ORDER
        context.queue.push(file);
#else
        context.queue.push_back(file);
#endif
    }

    // Load all of the files in the queue and lex them into tokens
    std::cout << "Lexing..." << std::endl;
    while (!context.queue.empty())
    {
        std::string file = (baseDirectory / context.queue.front()).string();
#if DIANNEX_OLD_INCLUDE_ORDER
        context.queue.pop();
#else
        context.queue.pop_front();
#endif
        std::string buf;
        try
        {
            if (!fs::exists(file))
                throw std::runtime_error("File does not exist.");
            if (context.files.find(file) != context.files.end())
                continue; // Already tokenized this file
            std::ifstream f(file, std::ios::in | std::ios::binary);
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

        context.tokenList.push_back(std::make_pair(file, tokens));
        context.files.insert(file);
    }

    if (fatalError)
    {
        std::cout << std::endl << rang::fgB::red << "Not proceeding with compilation due to fatal errors." << rang::fg::reset << std::endl;
        return 1;
    }

    // Parse each token stream
    std::cout << "Parsing..." << std::endl;
    for (auto& pair : context.tokenList)
    {
        context.currentFile = pair.first;
        ParseResult* parsed = Parser::ParseTokens(&context, &pair.second);
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
                if (e.line == 0 && e.column == 0)
                    std::cout << "[" << pair.first << ":?:?] ";
                else
                    std::cout << "[" << pair.first << ":" << e.line << ":" << e.column << "] ";
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
                case ParseError::ErrorType::DuplicateFlagName:
                    std::cout << "Duplicate flag names." << std::endl;
                    break;
                case ParseError::ErrorType::ErrorToken:
                    std::cout << e.info1 << std::endl;
                    break;
                }
            }

            std::cout << rang::fg::reset;
        }
        else
        {
            context.parseList.push_back(std::make_pair(pair.first, parsed));
        }
    }

    if (fatalError)
    {
        std::cout << std::endl << rang::fgB::red << "Not proceeding with compilation due to fatal errors." << rang::fg::reset << std::endl;
        return 1;
    }

    // Generate bytecode
    std::cout << "Generating bytecode..." << std::endl;
    for (auto& pair : context.parseList)
    {
        context.currentFile = pair.first;

        // Initialize string ID map for this file, if necessary
        if (context.project->options.addStringIds)
            context.stringIdPositions.insert(std::pair<std::string, std::vector<std::pair<uint32_t, int32_t>>>(context.currentFile, std::vector<std::pair<uint32_t, int32_t>>()));

        BytecodeResult* bytecode = Bytecode::Generate(pair.second, &context);
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
                    std::cout << "[" << pair.first << ":?:?] ";
                else
                    std::cout << "[" << pair.first << ":" << e.line << ":" << e.column << "] ";
                
                switch (e.type)
                {
                case BytecodeError::ErrorType::SceneAlreadyExists:
                    std::cout << "Duplicate scene name '" << e.info1 << "'." << std::endl;
                    break;
                case BytecodeError::ErrorType::FunctionAlreadyExists:
                    std::cout << "Duplicate function name '" << e.info1 << "'." << std::endl;
                    break;
                case BytecodeError::ErrorType::DefinitionAlreadyExists:
                    std::cout << "Duplicate definition name '" << e.info1 << "'." << std::endl;
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
                case BytecodeError::ErrorType::UnexpectedError:
                    std::cout << "Unexpected error. May be invalid syntax." << std::endl;
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

    if (context.project->options.addStringIds)
    {
        // Output string IDs to necessary files
        std::cout << "Writing string IDs..." << std::endl;
        
        std::vector<char> fileData;
        for (auto it = context.stringIdPositions.begin(); it != context.stringIdPositions.end(); ++it)
        {
            const std::string& currentFile = it->first;

            // Make backup of new file we're processing
            fs::copy_file(currentFile, currentFile + ".backup", fs::copy_options::overwrite_existing);

            // Read in the data from the file
            fileData.clear();
            std::ifstream f(currentFile, std::ios::in | std::ios::binary);
            f.seekg(0, std::ios::end);
            fileData.reserve(f.tellg());
            f.seekg(0, std::ios::beg);
            fileData.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();

            // Insert new IDs
            int offset = 0;
            std::stringstream ss(std::ios_base::app | std::ios_base::out);
            for (const std::pair<int, int>& info : it->second)
            {
                ss.str(std::string());
                ss.clear();
                ss << '&' << std::setfill('0') << std::setw(8) << std::hex << info.second;
                std::string ss_str = ss.str();
                fileData.insert(fileData.begin() + info.first + offset, ss_str.begin(), ss_str.begin() + 9);
                offset += 9;
            }

            // Save file
            std::ofstream fileOut(currentFile, std::ios::out | std::ios::binary);
            fileOut.write(&fileData[0], fileData.size());
            fileOut.close();
        }

        return 0;
    }

    // Write binary
    std::cout << "Writing binary..." << std::endl;
    const fs::path mainOutput = fs::absolute(baseDirectory / project.options.binaryOutputDir);
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

        const fs::path privateOutput = fs::absolute(baseDirectory / project.options.translationPrivateOutDir);
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