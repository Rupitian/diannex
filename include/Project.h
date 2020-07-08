#ifndef DIANNEX_PROJECT_H
#define DIANNEX_PROJECT_H

#include <string>
#include <vector>
#include <unordered_map>

namespace diannex
{
    struct ProjectInterpolationFlags
    {
        // Symbol used for interpolation. default: $
        std::string symbol = "$"; // Limited to a single character
    };

    struct ProjectOptions
    {
        // Message to print when compilation finishes. default: None
        std::string compileFinishMessage;

        // Source files to compile. default: None
        std::vector<std::string> files;

        // Flags to set for interpolation. default: <see ProjectInterpolationFlags>
        ProjectInterpolationFlags interpolationFlags;

        // Directory to output the binary. default: './out/'
        // If using public translation files, they'll be output
        // to this directory as well
        std::string binaryOutputDir;

        // Filename of the binary. default: project name
        std::string binaryName;

        // Whether to output a private translation file with the public one. default: false
        bool translationPrivate;

        // Directory to output private translation files, if used. default: './translations/'
        std::string translationPrivateOutDir;

        // Whether to output public translation files. default: false
        bool translationPublic;

        // Whether or not to compress the binary using zlib. default: true
        bool compression;

        // Predefined macros/defines to be used in source files. default: None
        std::unordered_map<std::string, std::string> macros;
    };

    struct ProjectFormat
    {
        // Name of the project. default: Name of the .json file or name of the current directory
        std::string name;

        // List of authors/contributors. default: None
        std::vector<std::string> authors;

        // Project Options. default: <see ProjectOptions>
        ProjectOptions options;
    };
}

#endif // DIANNEX_PROJECT_H
