#include "Core/Guid.h"

#include <iomanip>
#include <objbase.h>
#include <sstream>

bool FGuid::IsValid() const
{
    return A != 0 || B != 0 || C != 0 || D != 0;
}

void FGuid::Reset()
{
    A = 0;
    B = 0;
    C = 0;
    D = 0;
}

FString FGuid::ToString() const
{
    std::ostringstream Stream;
    Stream << std::uppercase << std::hex << std::setfill('0')
        << std::setw(8) << A << "-"
        << std::setw(8) << B << "-"
        << std::setw(8) << C << "-"
        << std::setw(8) << D;
    return Stream.str();
}

FGuid FGuid::NewGuid()
{
    GUID WindowsGuid = {};
    if (CoCreateGuid(&WindowsGuid) != S_OK)
    {
        return {};
    }

    const uint32 PartA = WindowsGuid.Data1;
    const uint32 PartB = (static_cast<uint32>(WindowsGuid.Data2) << 16) | WindowsGuid.Data3;
    const uint32 PartC =
        (static_cast<uint32>(WindowsGuid.Data4[0]) << 24) |
        (static_cast<uint32>(WindowsGuid.Data4[1]) << 16) |
        (static_cast<uint32>(WindowsGuid.Data4[2]) << 8) |
        static_cast<uint32>(WindowsGuid.Data4[3]);
    const uint32 PartD =
        (static_cast<uint32>(WindowsGuid.Data4[4]) << 24) |
        (static_cast<uint32>(WindowsGuid.Data4[5]) << 16) |
        (static_cast<uint32>(WindowsGuid.Data4[6]) << 8) |
        static_cast<uint32>(WindowsGuid.Data4[7]);
    return FGuid(PartA, PartB, PartC, PartD);
}

bool FGuid::Parse(const FString& Text, FGuid& OutGuid)
{
    FString HexOnly;
    HexOnly.reserve(32);
    for (char Ch : Text)
    {
        const bool bDigit = Ch >= '0' && Ch <= '9';
        const bool bUpper = Ch >= 'A' && Ch <= 'F';
        const bool bLower = Ch >= 'a' && Ch <= 'f';
        if (bDigit || bUpper || bLower)
        {
            HexOnly.push_back(Ch);
        }
    }

    if (HexOnly.size() != 32)
    {
        OutGuid.Reset();
        return false;
    }

    try
    {
        OutGuid.A = static_cast<uint32>(std::stoul(HexOnly.substr(0, 8), nullptr, 16));
        OutGuid.B = static_cast<uint32>(std::stoul(HexOnly.substr(8, 8), nullptr, 16));
        OutGuid.C = static_cast<uint32>(std::stoul(HexOnly.substr(16, 8), nullptr, 16));
        OutGuid.D = static_cast<uint32>(std::stoul(HexOnly.substr(24, 8), nullptr, 16));
    }
    catch (...)
    {
        OutGuid.Reset();
        return false;
    }

    return true;
}

FGuid FGuid::FromString(const FString& Text)
{
    FGuid Result;
    Parse(Text, Result);
    return Result;
}

bool FGuid::operator==(const FGuid& Other) const
{
    return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
}

bool FGuid::operator!=(const FGuid& Other) const
{
    return !(*this == Other);
}
