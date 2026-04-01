#include "FileUtils.h"
#include "Core/Paths.h"

#include <fstream>
#include <filesystem>

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

// 하위 폴더를 검색하여 타겟 파일의 전체(또는 상대) 경로를 찾는 함수

/* [사용 예시] FoundPath에 "Asset/Mesh/Nature/Lowpoly tree.mtl" 같은 실제 경로가 들어오게 됩니다.
FString FoundPath;
FFileUtils::FindFileRecursively("Asset", "Lowpoly tree.mtl", FoundPath)

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
			OutFoundPath = FPaths::ToUtf8(Entry.path().generic_wstring());
			return true;
		}
	}

	return false;
}