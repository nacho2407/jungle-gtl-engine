#pragma once
#include "EngineCore/EngineTypes.h"

struct FNumberState
{
	int32 NextNumber = 0;
	TPriorityQueue<int32, TArray<int32>, std::greater<int32>> FreeNumbers;
};

struct FName
{
	FName() : DisplayIndex(-1), ComparisonIndex(-1), Number(0) {}
	FName(const char* PStr);
	FName(const FString& Str);

	//int32 Compare(const FName& Other) const;
	bool operator==(const FName& Other) const;

	int32 DisplayIndex = -1; //출력용 ID
	int32 ComparisonIndex = -1; //이름 비교용 ID
	int32 Number = 0;

	FString GetDisplayName() const;
	const FString& GetComparisonName() const;

	static TUMap<FString, int32> ComparisonLookupTable; //이미 존재하는지 존재하지 않는지
	static TArray<FString> ComparisonEntries; //id -> 실제 비교 String
	static TUMap<FString, int32> DisplayLookupTable; // 이미 존재하는지 존재하지 않는지
	static TArray<FString> DisplayEntries; //id -> 실제 출력 String
};

struct FUniqueNameGenerator
{
	static TArray<FNumberState> ComparisonNumberStates;
	static FName MakeUniqueName(const FString& BaseName);
	static void ReleaseName(const FName& Name);
};