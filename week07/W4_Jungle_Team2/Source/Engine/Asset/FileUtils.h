#pragma once

#include "Core/CoreTypes.h"
#include <sstream>
#include <filesystem>
#include <fstream>

#include "Core/StringUtils.h"

class FFileUtils
{
public:
	static bool FileExists ( const FString& FileName );
	static bool LoadFileToString(const FString& FileName, FString& OutText);
	static bool LoadFileToLines(const FString& FileName, TArray<FString>& OutLines);
};
