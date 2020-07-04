#include "BinaryWriter.h"

#include <cstring>

static int ENDIAN = -1;

uint16_t swapbits(uint16_t src);
uint32_t swapbits(uint32_t src);
uint64_t swapbits(uint64_t src);

namespace diannex
{
    BinaryWriter::BinaryWriter()
    {
        if (ENDIAN == -1)
        {
            uint16_t number = 0x1;
            char* numPtr = (char*)&number;
            ENDIAN = (numPtr[0] == 1) ? LITTLE_ENDIAN : BIG_ENDIAN;
        }
        this->endian = ENDIAN;
    }

    void BinaryWriter::WriteUInt8(uint8_t value)
    {
        Write(&value, 1);
    }

    void BinaryWriter::WriteUInt16(uint16_t value)
    {
        Write(&value, 2);
    }
    
    void BinaryWriter::WriteUInt32(uint32_t value)
    {
        Write(&value, 4);
    }
    
    void BinaryWriter::WriteUInt64(uint64_t value)
    {
        Write(&value, 8);
    }

    void BinaryWriter::WriteInt8(int8_t value)
    {
        Write(&value, 1);
    }
    
    void BinaryWriter::WriteInt16(int16_t value)
    {
        Write(&value, 2);
    }
    
    void BinaryWriter::WriteInt32(int32_t value)
    {
        Write(&value, 4);
    }
    
    void BinaryWriter::WriteInt64(int64_t value)
    {
        Write(&value, 8);
    }

    void BinaryWriter::WriteFloat(float value)
    {
        Write(&value, sizeof(value));
    }
    
    void BinaryWriter::WriteDouble(double value)
    {
        Write(&value, sizeof(value));
    }

    void BinaryWriter::WriteString(std::string value)
    {
        if (value[value.size() - 1] != '\0')
        {
            value.push_back('\0');
        }

        for (int i = 0; i < value.size(); i++)
        {
            Write(&(value[i]), 1);
        }
    }

    void BinaryWriter::WriteBytes(const char* buff, int size)
    {
        Write(buff, size);
    }

    BinaryFileWriter::BinaryFileWriter(std::string filePath)
        : BinaryWriter()
    {
        fd = fopen(filePath.c_str(), "wb");
        // TODO: Log error upon failure to open file
        if (fd == NULL)
            canWrite = false;
        else
            canWrite = true;
    }

    BinaryFileWriter::~BinaryFileWriter()
    {
        if (canWrite)
            fclose(this->fd);
    }

    bool BinaryFileWriter::CanWrite()
    {
        return canWrite;
    }

    int BinaryFileWriter::Write(const void* ptr, size_t size)
    {
        if (!canWrite) return 0;
        if (this->endian == BIG_ENDIAN)
        {
            switch (size)
            {
                case 1:
                    return (int)fwrite(ptr, size, 1, fd);
                case 2:
                {
                    uint16_t res = *((uint16_t*)ptr);
                    res = swapbits(res);
                    return (int)fwrite(&res, size, 1, fd);
                }
                case 4:
                {
                    uint32_t res = *((uint32_t*)ptr);
                    res = swapbits(res);
                    return (int)fwrite(&res, size, 1, fd);
                }
                case 8:
                {
                    uint64_t res = *((uint32_t*)ptr);
                    res = swapbits(res);
                    return (int)fwrite(&res, size, 1, fd);
                }
                default:
                    return (int)fwrite(ptr, size, 1, fd);
            }
        }
        return (int)fwrite(ptr, size, 1, fd);
    }
}

#if defined(_MSC_VER)
#include <intrin.h>

uint16_t swapbits(uint16_t src)
{
    return _byteswap_ushort(src);
}

uint32_t swapbits(uint32_t src)
{
    return _byteswap_ulong(src);
}

uint64_t swapbits(uint64_t src)
{
    return _byteswap_uint64(src);
}
#elif defined(__GLIBC__)
#include <byteswap.h>
uint16_t swapbits(uint16_t src)
{
    return bswap_16(src);
}

uint32_t swapbits(uint32_t src)
{
    return bswap_32(src);
}

uint64_t swapbits(uint64_t src)
{
    return bswap_64(src);
}
#else
uint16_t swapbits(uint16_t src)
{
    return (((src >> 8) & 0xff) | ((src & 0xff) << 8));
}

uint32_t swapbits(uint32_t src)
{
    return (((src & 0xff000000) >> 24) | ((src & 0x00ff0000) >> 8) | ((src & 0x0000ff00) << 8) | ((src & 0x000000ff) << 24));
}

uint64_t swapbits(uint64_t src)
{
    return ((((src) & 0xff00000000000000ull) >> 56)   
      | (((src) & 0x00ff000000000000ull) >> 40)
      | (((src) & 0x0000ff0000000000ull) >> 24)
      | (((src) & 0x000000ff00000000ull) >> 8)
      | (((src) & 0x00000000ff000000ull) << 8)
      | (((src) & 0x0000000000ff0000ull) << 24)
      | (((src) & 0x000000000000ff00ull) << 40)
      | (((src) & 0x00000000000000ffull) << 56));
}
#endif