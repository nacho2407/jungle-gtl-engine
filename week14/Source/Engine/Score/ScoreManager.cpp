#include "Score/ScoreManager.h"

#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	constexpr int32 MaxSavedScores = 7;

	double NowSeconds()
	{
		using Clock = std::chrono::steady_clock;
		static const Clock::time_point Start = Clock::now();
		return std::chrono::duration<double>(Clock::now() - Start).count();
	}

	FString MakeTimestamp()
	{
		const auto Now = std::chrono::system_clock::now();
		const std::time_t Time = std::chrono::system_clock::to_time_t(Now);
		std::tm LocalTime{};
		localtime_s(&LocalTime, &Time);

		std::ostringstream Stream;
		Stream << std::put_time(&LocalTime, "%Y-%m-%d %H:%M:%S");
		return Stream.str();
	}

	FString MakeRunId()
	{
		const auto Now = std::chrono::system_clock::now();
		const std::time_t Time = std::chrono::system_clock::to_time_t(Now);
		std::tm LocalTime{};
		localtime_s(&LocalTime, &Time);

		const auto Millis = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()).count() % 1000;
		std::ostringstream Stream;
		Stream << "Run_" << std::put_time(&LocalTime, "%Y%m%d_%H%M%S") << "_" << std::setw(3) << std::setfill('0') << Millis;
		return Stream.str();
	}


	FString NormalizePlayerId(const FString& PlayerId)
	{
		FString Trimmed = PlayerId;
		const auto IsSpace = [](unsigned char Ch)
		{
			return std::isspace(Ch) != 0;
		};

		while (!Trimmed.empty() && IsSpace(static_cast<unsigned char>(Trimmed.front())))
		{
			Trimmed.erase(Trimmed.begin());
		}
		while (!Trimmed.empty() && IsSpace(static_cast<unsigned char>(Trimmed.back())))
		{
			Trimmed.pop_back();
		}

		FString Sanitized;
		Sanitized.reserve(16);
		for (unsigned char Ch : Trimmed)
		{
			if (std::isalnum(Ch) || Ch == '_' || Ch == '-')
			{
				Sanitized.push_back(static_cast<char>(Ch));
			}
			if (Sanitized.size() >= 16)
			{
				break;
			}
		}

		return Sanitized.empty() ? FString("PLAYER") : Sanitized;
	}

	bool ReadTextFile(const std::wstring& Path, FString& OutText)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File.is_open())
		{
			return false;
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		OutText = Buffer.str();
		return true;
	}

	json::JSON SnapshotToJson(const FScoreSnapshot& Snapshot, const TMap<FString, float>& CustomStats)
	{
		json::JSON Object;
		Object["runId"] = Snapshot.RunId;
		Object["playerId"] = Snapshot.PlayerId;
		Object["startedAt"] = Snapshot.StartedAt;
		Object["finishedAt"] = Snapshot.FinishedAt;
		Object["endingId"] = Snapshot.EndingId;
		Object["playTimeSeconds"] = Snapshot.PlayTimeSeconds;
		Object["enemyKills"] = Snapshot.EnemyKills;
		Object["bossKills"] = Snapshot.BossKills;
		Object["retryCount"] = Snapshot.RetryCount;
		Object["deathCount"] = Snapshot.DeathCount;
		Object["shotsFired"] = Snapshot.ShotsFired;
		Object["shotsHit"] = Snapshot.ShotsHit;
		Object["damageDealt"] = Snapshot.DamageDealt;
		Object["damageTaken"] = Snapshot.DamageTaken;
		Object["itemsInspected"] = Snapshot.ItemsInspected;
		Object["secretsFound"] = Snapshot.SecretsFound;
		Object["checkpointsReached"] = Snapshot.CheckpointsReached;
		Object["score"] = Snapshot.Score;

		const float Accuracy = Snapshot.ShotsFired > 0
			? static_cast<float>(Snapshot.ShotsHit) / static_cast<float>(Snapshot.ShotsFired)
			: 0.0f;
		Object["accuracy"] = Accuracy;

		json::JSON Custom = json::JSON::Make(json::JSON::Class::Object);
		for (const auto& Pair : CustomStats)
		{
			Custom[Pair.first] = Pair.second;
		}
		Object["custom"] = Custom;

		return Object;
	}

	json::JSON SortAndLimitScores(json::JSON& Scores)
	{
		std::vector<json::JSON> SortedScores;
		for (auto& Score : Scores.ArrayRange())
		{
			if (Score.JSONType() == json::JSON::Class::Object)
			{
				SortedScores.push_back(Score);
			}
		}

		std::sort(SortedScores.begin(), SortedScores.end(), [](json::JSON A, json::JSON B)
		{
			return static_cast<int32>(A["score"].ToInt()) > static_cast<int32>(B["score"].ToInt());
		});

		json::JSON LimitedScores = json::JSON::Make(json::JSON::Class::Array);
		for (int32 Index = 0; Index < static_cast<int32>(SortedScores.size()) && Index < MaxSavedScores; ++Index)
		{
			LimitedScores.append(SortedScores[Index]);
		}

		return LimitedScores;
	}
}

FScoreManager::FScoreManager()
{
	ResetRun();
}

void FScoreManager::StartRun(const FString& RunId)
{
	if (bRunActive)
	{
		return;
	}

	if (Current.RunId.empty())
	{
		Current.RunId = RunId.empty() ? MakeRunId() : RunId;
	}
	if (Current.StartedAt.empty())
	{
		Current.StartedAt = MakeTimestamp();
	}

	RunStartTimeSeconds = NowSeconds();
	bRunActive = true;
}

void FScoreManager::ResetRun()
{
	Current = FScoreSnapshot();
	Current.RunId = MakeRunId();
	Current.StartedAt = MakeTimestamp();
	CustomStats.clear();
	AccumulatedPlayTimeSeconds = 0.0;
	RunStartTimeSeconds = NowSeconds();
	bRunActive = false;
}

FScoreSnapshot FScoreManager::GetSnapshot() const
{
	FScoreSnapshot Snapshot = Current;
	Snapshot.PlayTimeSeconds = GetCurrentPlayTimeSeconds();
	Snapshot.Score = CalculateScore(Snapshot);
	return Snapshot;
}

void FScoreManager::AddEnemyKill(int32 Count)
{
	StartRun();
	Current.EnemyKills += (std::max)(0, Count);
}

void FScoreManager::AddBossKill(int32 Count)
{
	StartRun();
	Current.BossKills += (std::max)(0, Count);
}

void FScoreManager::AddRetry(int32 Count)
{
	StartRun();
	Current.RetryCount += (std::max)(0, Count);
}

void FScoreManager::AddDeath(int32 Count)
{
	StartRun();
	Current.DeathCount += (std::max)(0, Count);
}

void FScoreManager::AddShotFired(int32 Count)
{
	StartRun();
	Current.ShotsFired += (std::max)(0, Count);
}

void FScoreManager::AddShotHit(int32 Count)
{
	StartRun();
	Current.ShotsHit += (std::max)(0, Count);
}

void FScoreManager::AddDamageDealt(float Amount)
{
	StartRun();
	Current.DamageDealt += (std::max)(0.0f, Amount);
}

void FScoreManager::AddDamageTaken(float Amount)
{
	StartRun();
	Current.DamageTaken += (std::max)(0.0f, Amount);
}

void FScoreManager::AddItemInspected(int32 Count)
{
	StartRun();
	Current.ItemsInspected += (std::max)(0, Count);
}

void FScoreManager::AddSecretFound(int32 Count)
{
	StartRun();
	Current.SecretsFound += (std::max)(0, Count);
}

void FScoreManager::AddCheckpointReached(int32 Count)
{
	StartRun();
	Current.CheckpointsReached += (std::max)(0, Count);
}

void FScoreManager::SetCustomStat(const FString& Name, float Value)
{
	if (Name.empty())
	{
		return;
	}
	StartRun();
	CustomStats[Name] = Value;
}

void FScoreManager::AddCustomStat(const FString& Name, float Delta)
{
	if (Name.empty())
	{
		return;
	}
	StartRun();
	CustomStats[Name] += Delta;
}

float FScoreManager::GetCustomStat(const FString& Name) const
{
	auto It = CustomStats.find(Name);
	return It != CustomStats.end() ? It->second : 0.0f;
}

bool FScoreManager::SaveFinalScore(const FString& EndingId, const FString& PlayerId)
{
	FScoreSnapshot Snapshot = GetSnapshot();
	Snapshot.PlayerId = NormalizePlayerId(PlayerId);
	Snapshot.EndingId = EndingId;
	Snapshot.FinishedAt = MakeTimestamp();

	const std::wstring SaveDir = FPaths::SaveDir();
	FPaths::CreateDir(SaveDir);
	const std::wstring SavePath = FPaths::Combine(SaveDir, L"scores.json");

	json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
	FString Existing;
	if (ReadTextFile(SavePath, Existing))
	{
		Root = json::JSON::Load(Existing);
	}

	if (Root.JSONType() != json::JSON::Class::Object)
	{
		Root = json::JSON::Make(json::JSON::Class::Object);
	}
	if (!Root.hasKey("scores") || Root["scores"].JSONType() != json::JSON::Class::Array)
	{
		Root["scores"] = json::JSON::Make(json::JSON::Class::Array);
	}

	Root["scores"].append(SnapshotToJson(Snapshot, CustomStats));
	Root["scores"] = SortAndLimitScores(Root["scores"]);

	std::ofstream File(SavePath, std::ios::binary | std::ios::trunc);
	if (!File.is_open())
	{
		UE_LOG("[ScoreManager] Failed to open score save file: %s", FPaths::ToUtf8(SavePath).c_str());
		return false;
	}

	File << Root.dump(4);
	UE_LOG("[ScoreManager] Saved final score: %s", FPaths::ToUtf8(SavePath).c_str());
	return true;
}

bool FScoreManager::FinishRun(const FString& EndingId, const FString& PlayerId)
{
	const bool bSaved = SaveFinalScore(EndingId, PlayerId);
	ResetRun();
	return bSaved;
}

FString FScoreManager::GetSaveFilePath() const
{
	return FPaths::ToUtf8(FPaths::Combine(FPaths::SaveDir(), L"scores.json"));
}

float FScoreManager::GetCurrentPlayTimeSeconds() const
{
	double PlayTime = AccumulatedPlayTimeSeconds;
	if (bRunActive)
	{
		PlayTime += NowSeconds() - RunStartTimeSeconds;
	}
	return static_cast<float>((std::max)(0.0, PlayTime));
}

int32 FScoreManager::CalculateScore(const FScoreSnapshot& Snapshot) const
{
	const int32 TimeScore = (std::max)(0, 100000 - static_cast<int32>(Snapshot.PlayTimeSeconds) * 100);
	const int32 KillScore = (std::max)(0, Snapshot.EnemyKills * 1000);
	const int32 RetryScore = (std::max)(0, 100000 - Snapshot.RetryCount * 10000);
	const int32 DeathScore = (std::max)(0, 100000 - Snapshot.DeathCount * 10000);
	const int32 Score = TimeScore + KillScore + RetryScore + DeathScore;
	return (std::max)(0, Score);
}
