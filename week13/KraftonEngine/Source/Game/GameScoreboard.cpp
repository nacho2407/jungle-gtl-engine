#include "Game/GameScoreboard.h"

#include "Engine/Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace
{
	constexpr const char* KeyScore = "score";
	constexpr int32 MaxScoreboardEntries = 10;

	TArray<FScoreboardEntry> ScoreboardEntries;

	int32 SanitizeScore(int32 Score)
	{
		return Score < 0 ? 0 : Score;
	}
}

FString FGameScoreboard::GetDefaultPath()
{
	return FPaths::ToUtf8(FPaths::Combine(FPaths::SaveDir(), L"Scoreboard.json"));
}

void FGameScoreboard::SubmitScore(int32 Score)
{
	Load();
	AddScore(Score);
	Save();
}

void FGameScoreboard::AddScore(int32 Score)
{
	FScoreboardEntry Entry;
	Entry.Score = SanitizeScore(Score);

	ScoreboardEntries.push_back(Entry);
	SortAndTrim();
}

void FGameScoreboard::Clear()
{
	ScoreboardEntries.clear();
}

bool FGameScoreboard::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return false;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (Root.JSONType() != JSON::Class::Array)
	{
		return false;
	}

	ScoreboardEntries.clear();

	const int32 Count = Root.size();
	for (int32 i = 0; i < Count; ++i)
	{
		JSON Item = Root[i];
		if (!Item.hasKey(KeyScore))
		{
			continue;
		}

		FScoreboardEntry Entry;
		Entry.Score = SanitizeScore(static_cast<int32>(Item[KeyScore].ToInt()));
		ScoreboardEntries.push_back(Entry);
	}

	SortAndTrim();
	return true;
}

bool FGameScoreboard::SaveToFile(const FString& Path)
{
	using namespace json;

	JSON Root = Array();
	for (const FScoreboardEntry& Entry : ScoreboardEntries)
	{
		JSON Item = Object();
		Item[KeyScore] = Entry.Score;
		Root.append(Item);
	}

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath);
	if (!File.is_open())
	{
		return false;
	}

	File << Root;
	return true;
}

bool FGameScoreboard::Load()
{
	return LoadFromFile(GetDefaultPath());
}

bool FGameScoreboard::Save()
{
	return SaveToFile(GetDefaultPath());
}

int32 FGameScoreboard::GetEntryCount()
{
	return static_cast<int32>(ScoreboardEntries.size());
}

const FScoreboardEntry* FGameScoreboard::GetEntry(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(ScoreboardEntries.size()))
	{
		return nullptr;
	}

	return &ScoreboardEntries[static_cast<size_t>(Index)];
}

const TArray<FScoreboardEntry>& FGameScoreboard::GetEntries()
{
	return ScoreboardEntries;
}

void FGameScoreboard::SortAndTrim()
{
	std::sort(ScoreboardEntries.begin(), ScoreboardEntries.end(),
		[](const FScoreboardEntry& Lhs, const FScoreboardEntry& Rhs)
		{
			return Lhs.Score > Rhs.Score;
		});

	if (ScoreboardEntries.size() > static_cast<size_t>(MaxScoreboardEntries))
	{
		ScoreboardEntries.resize(static_cast<size_t>(MaxScoreboardEntries));
	}
}
