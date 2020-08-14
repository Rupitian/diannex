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

    int CompileContext::string(std::string str)
    {
        int res = std::find(internalStrings.begin(), internalStrings.end(), str) - internalStrings.begin();
        if (res == internalStrings.size())
            internalStrings.push_back(str);
        return res;
    }
}