#ifndef DIANNEX_UTILITY_H
#define DIANNEX_UTILITY_H

#include <string>

#include "Project.h"
#include "Context.h"

namespace diannex
{
    struct Token;
    void generate_project(std::string name);
    void load_project(std::string path, ProjectFormat &proj);
}

#endif // DIANNEX_UTILITY_H