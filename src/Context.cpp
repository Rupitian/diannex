#include "Context.h"

namespace diannex
{
    CompileContext::~CompileContext()
    {
        for (auto it = parseList.begin(); it != parseList.end(); it++)
        {
            delete it->second;
        }
    }

    int CompileContext::string(const std::string& str)
    {
        int index = internalStrings.size();
        auto p = internalStringsMap.insert({ str, index });
        if (p.second)
        {
            // This is a new string; add to list as well
            internalStrings.push_back(str);
            return index;
        }

        // Return index of previously-stored string
        return p.first->second;
    }
}