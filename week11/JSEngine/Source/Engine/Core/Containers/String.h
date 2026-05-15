#pragma once

#include "Core/CoreTypes.h"

#include <string>
#include <functional>

using FString = std::string;
using FWString = std::wstring;
inline uint32 GetTypeHash(const FString& String)
{
    return static_cast<uint32>(std::hash<FString>{}(String));
}
