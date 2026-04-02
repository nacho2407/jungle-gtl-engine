#include "FName.h"
#include <algorithm>
#include <cctype>

// ============================================================
// FNamePool
// ============================================================
uint32 FNamePool::Store(const FString& InString)
{
	auto It = LookupMap.find(InString);
	if (It != LookupMap.end())
	{
		return It->second;
	}
	uint32 Index = static_cast<uint32>(Entries.size());
	Entries.push_back(InString);
	LookupMap[InString] = Index;
	return Index;
}

const FString& FNamePool::Resolve(uint32 Index) const
{
	static const FString Empty;
	if (Index < static_cast<uint32>(Entries.size()))
	{
		return Entries[Index];
	}
	return Empty;
}

// ============================================================
// FName
// ============================================================
static FString ToLower(const FString& Str)
{
	FString Result = Str;
	std::transform(Result.begin(), Result.end(), Result.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return Result;
}

const FName FName::None = "Name_None";

FName::FName()
	: ComparisonIndex(0)
	, DisplayIndex(0)
{
}

FName::FName(const char* InName)
{
	if (InName == nullptr || InName[0] == '\0')
	{
		ComparisonIndex = 0;
		DisplayIndex = 0;
		return;
	}
	FString NameStr(InName);
	FString LowerStr = ToLower(NameStr);

	FNamePool& Pool = FNamePool::Get();
	DisplayIndex = Pool.Store(NameStr);
	ComparisonIndex = Pool.Store(LowerStr);
}

FName::FName(const FString& InName)
{
	if (InName.empty())
	{
		ComparisonIndex = 0;
		DisplayIndex = 0;
		return;
	}
	FString LowerStr = ToLower(InName);

	FNamePool& Pool = FNamePool::Get();
	DisplayIndex = Pool.Store(InName);
	ComparisonIndex = Pool.Store(LowerStr);
}

bool FName::operator==(const FName& Other) const
{
	return ComparisonIndex == Other.ComparisonIndex;
}

bool FName::operator!=(const FName& Other) const
{
	return ComparisonIndex != Other.ComparisonIndex;
}

size_t FName::Hash::operator()(const FName& Name) const
{
	return std::hash<uint32>()(Name.ComparisonIndex);
}

FString FName::ToString() const
{
	return FNamePool::Get().Resolve(DisplayIndex);
}

bool FName::IsValid() const
{
	return DisplayIndex != 0 || ComparisonIndex != 0;
}
