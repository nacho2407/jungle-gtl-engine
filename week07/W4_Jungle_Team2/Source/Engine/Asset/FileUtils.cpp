#include "FileUtils.h"

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
