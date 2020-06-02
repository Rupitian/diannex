#ifndef DIANNEX_PROJECT_H
#define DIANNEX_PROJECT_H

#include <string>
#include <vector>

namespace diannex
{
    struct ProjectInterpolationFlags
    {
        // Symbol used for interpolation. default: $
        std::string symbol; // NOTE: Should we limit it to a single character?
    };

    struct ProjectOptions
    {
        // Message to print when compilation finishes. default: None
        std::string compileFinishMessage;

        // Source files to compile. default: None
        std::vector<std::string> files;

        // Flags to set for interpolation. default: <see ProjectInterpolationFlags>
        ProjectInterpolationFlags interpolationFlags;

        // Directory to output translation files. default: './translations'
        std::string translationOutput;

        // Predefined macros/defines to be used in source files. default: None
        std::vector<std::string> macros;
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

#endif //DIANNEX_PROJECT_H
