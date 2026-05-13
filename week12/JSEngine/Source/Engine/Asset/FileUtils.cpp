#include "FileUtils.h"
#include "Core/Paths.h"

#include <fstream>
#include <filesystem>
#include "Core/Paths.h"

bool FFileUtils::FileExists(const FString& FileName)
{
	return std::filesystem::exists(std::filesystem::path(FPaths::ToWide(FileName)));
}

bool FFileUtils::LoadFileToString(const FString& FileName, FString& OutText)
{
	OutText.clear();
	
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FileName)), std::ios::in);
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
	
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FileName)), std::ios::in);
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
	OutFoundPath.clear();

	if (!std::filesystem::exists(RootPath) || !std::filesystem::is_directory(RootPath))
	{
		return false;
	}

	constexpr size_t MaxSearchLimit = 5000;
    size_t CurrentSearchCount = 0;

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(
		RootPath,
		std::filesystem::directory_options::skip_permission_denied))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		if (Entry.path().filename() == TargetName)
		{
			std::filesystem::path RelPath = std::filesystem::relative(Entry.path(), RootPath);
			OutFoundPath = FPaths::ToUtf8(RelPath.generic_wstring());
			return true;
		}

		// 최대 검색 회수를 초과하면 탐색을 중단하여 무한 루프 방지
		if (CurrentSearchCount++ > MaxSearchLimit)
        {
            return false;
        }
	}

	return false;
}
