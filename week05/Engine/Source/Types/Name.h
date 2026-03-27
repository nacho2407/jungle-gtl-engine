#pragma once

#include "String.h"
#include "Map.h"
#include "CoreTypes.h"
#include <vector>

class FName {
public:
	FName()=default;
	FName(const FString& InString);
	FName(const char* InString);
	FName(const FString& InString, int32 InNumber);
	FName(const char* InString, int32 InNumber);

	bool operator==(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number; }
	bool operator==(const FString& Other) const { return ToString() == Other; }
	bool operator==(const char* Other) const { return ToString() == FString(Other); }
	bool operator!=(const FName& Other) const { return !(*this == Other); }
	bool operator<(const FName& Other) const
	{
		if (ComparisonIndex != Other.ComparisonIndex) return ComparisonIndex < Other.ComparisonIndex;
		return Number < Other.Number;
	}
	int32 Compare(const FName& Other) const;
	FString GetPlainName() const;
	FString ToString() const;
	int32 GetNumber() const { return Number; }
	bool IsNone() const { return DisplayIndex == 0; }
private:
	uint32 DisplayIndex = 0;
	uint32 ComparisonIndex;
	int32  Number = 0;
	struct FNameTable
	{
		std::vector<FString> Names;
		TMap<FString, uint32> LookupMap;
		FNameTable() { Names.push_back("None"); }
		uint32 FindOrAdd(const FString& InString);
		const FString& Resolve(uint32 InIndex) const { return Names[InIndex]; }
	};

	static FNameTable& GetTable()
	{
		static FNameTable Table;
		return Table;
	}
	static void SplitNameAndNumber(const FString& InString, FString& OutBase, int32& OutNumber);
};