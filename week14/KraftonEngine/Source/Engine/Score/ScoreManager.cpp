#include "Score/ScoreManager.h"

#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
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

bool FScoreManager::SaveFinalScore(const FString& EndingId)
{
	FScoreSnapshot Snapshot = GetSnapshot();
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

bool FScoreManager::FinishRun(const FString& EndingId)
{
	const bool bSaved = SaveFinalScore(EndingId);
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
	// 현재 점수 정책은 잡몹 처치만 반영합니다.
	// 보스전/시간/피해량/아이템 점수는 해당 콘텐츠가 준비된 뒤 별도로 합산합니다.
	const int32 Score = Snapshot.EnemyKills * 1000;
	return (std::max)(0, Score);
}
