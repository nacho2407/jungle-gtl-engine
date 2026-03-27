#pragma once

#include "Core/CoreTypes.h"
#include <sstream>

namespace FileStringUtils
{
	//	문자열의 양쪽 공백 제거
	inline FString Trim(const FString& Str)
	{
		size_t Start = 0;
		while (Start < Str.size() && std::isspace(static_cast<unsigned char>(Str[Start])))
		{
			Start++;
		}
		
		size_t End = Str.size();
		while (End > Start && std::isspace(static_cast<unsigned char>(Str[End - 1])))
		{
			End--;
		}
		
		return Str.substr(Start, End - Start);
	}
	
	//	현재 위치에 Prefix가 존재하는지 확인 후 boolean 반환
	inline bool StartWith(const FString& Str, const FString& Prefix)
	{
		return Str.rfind(Prefix) == 0;
	}
	
	inline TArray<FString> Split(const FString& Str, const FString& Separator)
	{
		TArray<FString> Result;
		std::istringstream iss(Str);
		
		FString Token;
		while (iss >> Token)
		{
			Result.push_back(Token);
		}
		
		return Result;
	}
	
}

class FFileUtils
{
public:
	static bool FileExists ( const FString& FileName );
	static bool LoadFileToString(const FString& FileName, FString& OutText);
	static bool LoadFileToLines(const FString& FileName, TArray<FString>& OutLines);
};
