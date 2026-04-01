#include "FileUtils.h"
#include "Core/Paths.h"

#include <fstream>
#include <filesystem>
#include "Core/Paths.h"

bool FFileUtils::FileExists(const FString& FileName)
{
	return std::filesystem::exists(std::filesystem::path(FileName));
}

bool FFileUtils::LoadFileToString(const FString& FileName, FString& OutText)
{
	OutText.clear();
	
	std::ifstream File(std::filesystem::path(FileName), std::ios::in);
	if (!File.is_open())
	{
		return false;
	}
	
	std::ostringstream Buffer;
	Buffer << File.rdbuf();
	
	const FString Content = Buffer.str();
	OutText = Content;
	
	return true;
}

bool FFileUtils::LoadFileToLines(const FString& FileName, TArray<FString>& OutLines)
{
	OutLines.clear();
	
	std::ifstream File(std::filesystem::path(FileName), std::ios::in);
	if (!File.is_open())
	{
		return false;
	}
	
	FString Line;
	while (std::getline(File, Line))
	{
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		
		OutLines.push_back(Line);
	}
	
	return true;
}

/*
// 하위 폴더를 검색하여 타겟 파일의 SearchRootPath 기준 상대 경로를 찾는 함수

[사용 예시]
- 탐색 시작 폴더 (SearchRootPath): D:/DownloadedAssets/Nature
- 타겟 파일 이름 (TargetFileName): tree.mtl
- 실제 파일 위치 (Entry.path()): D:/DownloadedAssets/Nature/Trees/Lowpoly/tree.mtl
- 반환되는 결과 (OutFoundPath): Trees/Lowpoly/tree.mtl
*/
bool FFileUtils::FindFileRecursively(const FString& SearchRootPath, const FString& TargetFileName, FString& OutFoundPath)
{
	std::filesystem::path RootPath = FPaths::ToWide(SearchRootPath);
	std::filesystem::path TargetName = FPaths::ToWide(TargetFileName);

	if (!std::filesystem::exists(RootPath) || !std::filesystem::is_directory(RootPath))
	{
		return false;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(RootPath))
	{
		if (Entry.is_regular_file() && Entry.path().filename() == TargetName)
		{
			std::filesystem::path RelPath = std::filesystem::relative(Entry.path(), RootPath);
			OutFoundPath = FPaths::ToUtf8(RelPath.generic_wstring());
			return true;
		}
	}

	return false;
}
