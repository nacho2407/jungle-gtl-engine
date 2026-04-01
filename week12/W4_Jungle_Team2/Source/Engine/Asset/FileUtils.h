#pragma once

#include "Core/CoreMinimal.h"
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

	// 하위 폴더를 검색하여 타겟 파일의 전체(또는 상대) 경로를 찾는 함수
	static bool FindFileRecursively(const FString& SearchRootPath, const FString& TargetFileName, FString& OutFoundPath);
};
