#include "Utility.h"

#include <fstream>
#include <iostream>
#include <filesystem>

#include <libs/json.hpp>

#include "Lexer.h"

namespace fs = std::filesystem;

namespace diannex
{
    void generate_project(std::string name)
    {
        nlohmann::json project = {
                {"name", name},
                {"authors", nlohmann::json::array()},
                {"options", {
                                 {"compile_finish_message", ""},
                                 {"files", {"main.dx"}},
                                 {"interpolation_enabled", true},
                                 {"binary_outdir", "./out/"},
                                 {"binary_name", ""},
                                 {"translation_private", false},
                                 {"translation_private_name", ""},
                                 {"translation_private_outdir", "./translations/"},
                                 {"translation_public", false},
                                 {"translation_public_name", ""},
                                 {"compression", true},
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
            proj.options.interpolationEnabled = true;
            proj.options.binaryOutputDir = "./out/";
            proj.options.binaryName = "out";
            proj.options.translationPrivate = false;
            proj.options.translationPrivateOutDir = "./translations/";
            proj.options.translationPublic = false;
            proj.options.compression = true;
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

        proj.options.interpolationEnabled = project["options"].contains("interpolation_enabled") ?
                                            project["options"]["interpolation_enabled"].get<bool>() :
                                            true;

        proj.options.binaryOutputDir = project["options"].contains("binary_outdir") ?
                                       project["options"]["binary_outdir"].get<std::string>() :
                                       "./out";

        proj.options.binaryName = project["options"].contains("binary_name") ?
                                  project["options"]["binary_name"].get<std::string>() :
                                  "";

        proj.options.translationPrivate = project["options"].contains("translation_private") ?
                                          project["options"]["translation_private"].get<bool>() :
                                          false;

        proj.options.translationPrivateName = project["options"].contains("translation_private_name") ?
                                             project["options"]["translation_private_name"].get<std::string>() :
                                             "";

        proj.options.translationPrivateOutDir = project["options"].contains("translation_private_outdir") ?
                                                project["options"]["translation_private_outdir"].get<std::string>() :
                                                "./translations";

        proj.options.translationPublic = project["options"].contains("translation_public") ?
                                         project["options"]["translation_public"].get<bool>() :
                                         false;

        proj.options.translationPublicName = project["options"].contains("translation_public_name") ?
                                             project["options"]["translation_public_name"].get<std::string>() :
                                             "";

        proj.options.compression = project["options"].contains("compression") ?
                                   project["options"]["compression"].get<bool>() :
                                   true;

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