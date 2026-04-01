#pragma once

#include "Core/Serialization/Archive.h"

#include <filesystem>
#include <fstream>

class FWindowsBinWriter : public FArchive
{
public:
    explicit FWindowsBinWriter(const std::filesystem::path& Path)
        : FArchive(false)
    {
        Stream.open(Path, std::ios::binary | std::ios::trunc);
        if (!Stream)
        {
            bHasError = true;
        }
    }

    bool IsValid() const { return Stream.good() && !bHasError; }
    bool HasError() const { return bHasError; }

    void Serialize(void* Data, size_t Size) override
    {
        if (bHasError)
            return;

        if (!Stream.write(reinterpret_cast<const char*>(Data),
                          static_cast<std::streamsize>(Size)))
        {
            bHasError = true;
        }
    }

private:
    std::ofstream Stream;
    bool          bHasError = false;
};

class FWindowsBinReader : public FArchive
{
public:
    explicit FWindowsBinReader(const std::filesystem::path& Path)
        : FArchive(true)
    {
        Stream.open(Path, std::ios::binary);
        if (!Stream)
        {
            bHasError = true;
        }
    }

    bool IsValid() const { return Stream.good() && !bHasError; }
    bool HasError() const { return bHasError; }

    void Serialize(void* Data, size_t Size) override
    {
        if (bHasError)
            return;

        if (!Stream.read(reinterpret_cast<char*>(Data),
                         static_cast<std::streamsize>(Size)))
        {
            bHasError = true;
        }
    }

private:
    std::ifstream Stream;
    bool          bHasError = false;
};
