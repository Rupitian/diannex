#include "Binary.h"

namespace diannex
{
    void Binary::Write(BinaryWriter* bw, CompileContext* ctx)
    {
        bw->WriteBytes("DNX", 3);
        bw->WriteUInt8(0); // Version

        // File flags
        // -> compressed?
        // -> strings shuffled?
        // -> has internal translation file?
        // Remaining file size (decompressed size with compression)

        // Scene metadata
        // Function metadata
        // Definition metadata
        // Bytecode
        // Internal string table
        // Internal translation file (if applicable)
    }
}