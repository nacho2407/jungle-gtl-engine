#include "Name.h"
#include <algorithm>
#include <cctype>

TMap<FString, int32> FName::ComparisonLookupTable; //이미 존재하는지 존재하지 않는지
TArray<FString> FName::ComparisonEntries; //id -> 실제 비교 String
TMap<FString, int32> FName::DisplayLookupTable; // 이미 존재하는지 존재하지 않는지
TArray<FString> FName::DisplayEntries; //id -> 실제 출력 String
TArray<FNumberState> FUniqueNameGenerator::ComparisonNumberStates; //비교이름의 제일작은 Number을 관리

FName::FName(const char* PStr) : FName(FString(PStr))
{

}

FName::FName(const FString& Str)//Cube -> cube
{
	FString Lower = Str;
	std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char c) { return std::tolower(c); });
	auto ComparisonIter = ComparisonLookupTable.find(Lower);
	if (ComparisonLookupTable.end() == ComparisonIter)// 존재하지 않는다면
	{
		ComparisonIndex = static_cast<int32>(ComparisonEntries.size());
		ComparisonEntries.push_back(Lower);
		ComparisonLookupTable.insert({ Lower, ComparisonIndex });
	}
	else
	{
		ComparisonIndex = ComparisonIter->second;
	}

	auto DisplayIter = DisplayLookupTable.find(Str); //
	if (DisplayLookupTable.end() == DisplayIter)
	{
		DisplayIndex = static_cast<int32>(DisplayEntries.size());
		DisplayEntries.push_back(Str);
		DisplayLookupTable.insert({ Str, DisplayIndex });
	}
	else
	{
		DisplayIndex = DisplayIter->second;
	}
}


bool FName::operator==(const FName& Other) const
{
	return (Other.ComparisonIndex == this->ComparisonIndex) && (Other.Number == this->Number);
}

FString FName::GetDisplayName() const
{
	return GetPlainDisplayName() + "_" + std::to_string(Number);
}

const FString& FName::GetPlainDisplayName() const
{
	static const FString EmptyName;
	if (DisplayIndex < 0 || DisplayIndex >= static_cast<int32>(DisplayEntries.size()))
	{
		return EmptyName;
	}

	return DisplayEntries[DisplayIndex];
}

const FString& FName::GetComparisonName() const
{
	return ComparisonEntries[ComparisonIndex];
}




FName FUniqueNameGenerator::MakeUniqueName(const FString& BaseName)
{
	FName Name(BaseName);
	while (Name.ComparisonIndex >= ComparisonNumberStates.size())
	{
		ComparisonNumberStates.push_back({});
	}
	if (ComparisonNumberStates[Name.ComparisonIndex].FreeNumbers.empty())
	{
		Name.Number = ComparisonNumberStates[Name.ComparisonIndex].NextNumber++;
	}
	else
	{
		Name.Number = ComparisonNumberStates[Name.ComparisonIndex].FreeNumbers.top();
		ComparisonNumberStates[Name.ComparisonIndex].FreeNumbers.pop();
	}
	return Name;
}

void FUniqueNameGenerator::ReleaseName(const FName& Name)
{
	if (Name.ComparisonIndex < 0) return;

	while (Name.ComparisonIndex >= ComparisonNumberStates.size())
	{
		ComparisonNumberStates.push_back({});
	}

	FNumberState& State = ComparisonNumberStates[Name.ComparisonIndex];
	State.FreeNumbers.push(Name.Number);
	if (static_cast<int32>(State.FreeNumbers.size()) == State.NextNumber)
	{
		State.NextNumber = 0;
		decltype(State.FreeNumbers) EmptyQueue;
		State.FreeNumbers.swap(EmptyQueue);
	}
}
