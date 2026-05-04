#pragma once

#include "Core/CoreTypes.h"

struct FScoreboardEntry
{
	int32 Score = 0;
};

class FGameScoreboard
{
public:
	static FString GetDefaultPath();

	static void SubmitScore(int32 Score);
	static void AddScore(int32 Score);
	static void Clear();

	static bool LoadFromFile(const FString& Path);
	static bool SaveToFile(const FString& Path);

	static bool Load();
	static bool Save();

	static int32 GetEntryCount();
	static const FScoreboardEntry* GetEntry(int32 Index);
	static const TArray<FScoreboardEntry>& GetEntries();

private:
	static void SortAndTrim();
};
