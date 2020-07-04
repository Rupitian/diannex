#ifndef DIANNEX_BINARY_H
#define DIANNEX_BINARY_H

#include "Context.h"
#include "BinaryWriter.h"

namespace diannex
{
    class Binary
    {
    public:
        static void Write(BinaryWriter* bw, CompileContext* ctx);
    private:
        Binary();
    };
}

#endif // DIANNEX_BINARY_H