#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

struct FScoreSnapshot
{
	FString RunId;
	FString StartedAt;
	FString FinishedAt;
	FString EndingId;
	float PlayTimeSeconds = 0.0f;
	int32 EnemyKills = 0;
	int32 BossKills = 0;
	int32 RetryCount = 0;
	int32 DeathCount = 0;
	int32 ShotsFired = 0;
	int32 ShotsHit = 0;
	float DamageDealt = 0.0f;
	float DamageTaken = 0.0f;
	int32 ItemsInspected = 0;
	int32 SecretsFound = 0;
	int32 CheckpointsReached = 0;
	int32 Score = 0;
};

class FScoreManager : public TSingleton<FScoreManager>
{
	friend class TSingleton<FScoreManager>;

public:
	void StartRun(const FString& RunId = "");
	void ResetRun();
	FScoreSnapshot GetSnapshot() const;

	void AddEnemyKill(int32 Count = 1);
	void AddBossKill(int32 Count = 1);
	void AddRetry(int32 Count = 1);
	void AddDeath(int32 Count = 1);
	void AddShotFired(int32 Count = 1);
	void AddShotHit(int32 Count = 1);
	void AddDamageDealt(float Amount);
	void AddDamageTaken(float Amount);
	void AddItemInspected(int32 Count = 1);
	void AddSecretFound(int32 Count = 1);
	void AddCheckpointReached(int32 Count = 1);

	void SetCustomStat(const FString& Name, float Value);
	void AddCustomStat(const FString& Name, float Delta);
	float GetCustomStat(const FString& Name) const;

	bool SaveFinalScore(const FString& EndingId = "Ending");
	bool FinishRun(const FString& EndingId = "Ending");

	FString GetSaveFilePath() const;

private:
	FScoreManager();

	float GetCurrentPlayTimeSeconds() const;
	int32 CalculateScore(const FScoreSnapshot& Snapshot) const;

private:
	FScoreSnapshot Current;
	TMap<FString, float> CustomStats;
	double AccumulatedPlayTimeSeconds = 0.0;
	double RunStartTimeSeconds = 0.0;
	bool bRunActive = false;
};
