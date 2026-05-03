#include "Game/GameMode/GameModeCarGame.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Game/PlayerController/PlayerControllerCarGame.h"
#include "Game/Pawn/PoliceCar.h"
#include "Game/Pawn/CarPawn.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Game/Component/CarGasComponent.h"
#include "Game/Component/DirtComponent.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Core/Log.h"

#include <cmath>
#include <cstdlib>

IMPLEMENT_CLASS(AGameModeCarGame, AGameModeBase)

AGameModeCarGame::AGameModeCarGame()
{
	GameStateClass = AGameStateCarGame::StaticClass();
	PlayerControllerClass = APlayerControllerCarGame::StaticClass();
}

// ============================================================
// Match flow
// ============================================================

void AGameModeCarGame::StartMatch()
{
	Super::StartMatch();

	if (auto* GS = Cast<AGameStateCarGame>(GetGameState()))
	{
		GS->SetPhase(ECarGamePhase::None);
		GS->SetQuestPhase(ECarGamePhase::None);
		GS->SetRemainingMatchTime(MatchDuration);
		GS->SetRemainingPhaseTime(0.0f);
		GS->SetLastEndedPhase(ECarGamePhase::None);
		GS->SetLastPhaseResult(EPhaseResult::None);
		UE_LOG("[CarGame] StartMatch — Phase = None, MatchTime=%.1fs", MatchDuration);
	}
}

void AGameModeCarGame::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;
	if (GS->GetPhase() == ECarGamePhase::Finished) return;

	// ── 매치 전체 타이머 ──
	{
		float t = GS->GetRemainingMatchTime() - DeltaTime;
		if (t <= 0.0f)
		{
			GS->SetRemainingMatchTime(0.0f);

			// 진행 중 페이즈가 있다면 즉시 판정 후 매치 종료. EndPhase 가 Result 페이즈
			// 로 전이하지만 그 위에 바로 Finished 를 덮어쓰는 식으로 확정.
			ECarGamePhase Cur = GS->GetPhase();
			if (Cur != ECarGamePhase::None && Cur != ECarGamePhase::Result)
			{
				EPhaseResult R = JudgePhaseResult(Cur);
				if (R == EPhaseResult::Success) GS->MarkPhaseCleared(Cur);
				if (Cur == ECarGamePhase::EscapePolice) DespawnPoliceCars();
				GS->SetLastEndedPhase(Cur);
				GS->SetLastPhaseResult(R);
			}
			GS->SetPhase(ECarGamePhase::Finished);
			UE_LOG("[CarGame] Match time elapsed — Phase = Finished");
			return;
		}
		GS->SetRemainingMatchTime(t);
	}

	// ── 페이즈 타이머 ──
	const ECarGamePhase Phase = GS->GetPhase();

	if (Phase == ECarGamePhase::Result)
	{
		float t = GS->GetRemainingPhaseTime() - DeltaTime;
		if (t <= 0.0f)
		{
			GS->SetRemainingPhaseTime(0.0f);
			if (!TryFinishOnAllCleared())
			{
				GS->SetPhase(ECarGamePhase::None);
			}
		}
		else
		{
			GS->SetRemainingPhaseTime(t);
		}
		return;
	}

	if (Phase != ECarGamePhase::None && GS->GetRemainingPhaseTime() > 0.0f)
	{
		float t = GS->GetRemainingPhaseTime() - DeltaTime;
		if (t <= 0.0f)
		{
			GS->SetRemainingPhaseTime(0.0f);
			EndPhase(JudgePhaseResult(Phase));
		}
		else
		{
			GS->SetRemainingPhaseTime(t);
		}
	}
}

// ============================================================
// Trigger handlers
// ============================================================

void AGameModeCarGame::OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS || !Trigger) return;

	// Phase != None 인 동안엔 다른 트리거 무시 (사용자 결정 — 페이즈 동시 진행 X).
	const ECarGamePhase Target = TagToPhase(Trigger->GetTriggerTag());
	if (Target == ECarGamePhase::None) return;
	if (GS->GetQuestPhase() != Target) return;
	if (GS->GetPhase() != ECarGamePhase::None) return;

	BeginPhase(Target, Pawn);
}

void AGameModeCarGame::OnPossessedPawnExitedTrigger(ATriggerVolumeBase* /*Trigger*/, APawn* /*Pawn*/)
{
	// 종료는 타이머가 단일 권한자. 트리거 영역 이탈은 무시.
}

void AGameModeCarGame::OnPlayerCaught(AActor* /*Catcher*/)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;
	if (GS->GetPhase() != ECarGamePhase::EscapePolice) return; // 이미 종료/Result 면 무시

	UE_LOG("[CarGame] Player caught — EscapePolice failed");
	EndPhase(EPhaseResult::Failed);
}

// ============================================================
// Phase begin / end
// ============================================================

void AGameModeCarGame::BeginPhase(ECarGamePhase Target, APawn* TriggerPawn)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;
	if (GS->GetPhase() != ECarGamePhase::None) return;

	GS->SetRemainingPhaseTime(GetPhaseDuration(Target));
	GS->SetPhase(Target);

	UE_LOG("[CarGame] BeginPhase = %d (%.1fs)", static_cast<int32>(Target), GetPhaseDuration(Target));

	if (Target == ECarGamePhase::EscapePolice)
	{
		// trigger 콜백이 전달한 pawn 우선, 없으면 PlayerController 경로
		APawn* PlayerPawn = TriggerPawn ? TriggerPawn : GetPlayerPawn();
		SpawnPoliceCars(PlayerPawn);
	}
}

void AGameModeCarGame::EndPhase(EPhaseResult Result)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;

	const ECarGamePhase Cur = GS->GetPhase();
	if (Cur == ECarGamePhase::None || Cur == ECarGamePhase::Result || Cur == ECarGamePhase::Finished)
	{
		return; // 이미 종료/결과 표시 중이면 무시
	}

	if (Cur == ECarGamePhase::EscapePolice)
	{
		DespawnPoliceCars();
	}

	if (Result == EPhaseResult::Success)
	{
		GS->MarkPhaseCleared(Cur);
	}

	GS->SetLastEndedPhase(Cur);
	GS->SetLastPhaseResult(Result);

	// Result 페이즈로 전이 — 다음 트리거 진입까지 빈 페이즈가 되지 않도록 짧게 표시.
	GS->SetRemainingPhaseTime(ResultDisplayDuration);
	GS->SetPhase(ECarGamePhase::Result);

	UE_LOG("[CarGame] EndPhase ended=%d result=%d (mask=0x%08x)",
		static_cast<int32>(Cur), static_cast<int32>(Result), GS->GetClearedPhasesMask());
}

// ============================================================
// Judgment
// ============================================================

EPhaseResult AGameModeCarGame::JudgePhaseResult(ECarGamePhase Phase) const
{
	APawn* PlayerPawn = GetPlayerPawn();
	auto*  Car        = Cast<ACarPawn>(PlayerPawn);

	switch (Phase)
	{
	case ECarGamePhase::CarWash:
	{
		// v1: car에 부착된 모든 UDirtComponent 가 IsWashed 면 Success.
		// 향후 ratio 기반 정밀 판정으로 확장 예정.
		if (!Car) return EPhaseResult::Failed;
		bool bAnyDirty = false;
		for (UActorComponent* C : Car->GetComponents())
		{
			if (auto* Dirt = Cast<UDirtComponent>(C))
			{
				if (!Dirt->IsWashed()) { bAnyDirty = true; break; }
			}
		}
		return bAnyDirty ? EPhaseResult::Failed : EPhaseResult::Success;
	}
	case ECarGamePhase::CarGas:
	{
		if (!Car || !Car->GetGas()) return EPhaseResult::Failed;
		return Car->GetGas()->GetGasRatio() >= CarGasSuccessRatio
			? EPhaseResult::Success : EPhaseResult::Failed;
	}
	case ECarGamePhase::EscapePolice:
		// 잡히면 OnPlayerCaught → EndPhase(Failed) 직접 라우트. 이 경로에 도달했단 건
		// 시간을 다 채우고 도망 성공한 경우.
		return EPhaseResult::Success;

	case ECarGamePhase::DodgeMeteor:
		return Car && Car->GetHealth() > 0.0f
			? EPhaseResult::Success : EPhaseResult::Failed;

	default:
		return EPhaseResult::None;
	}
}

bool AGameModeCarGame::TryFinishOnAllCleared()
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return false;

	constexpr uint32 AllPhases =
		(1u << static_cast<uint32>(ECarGamePhase::CarWash))      |
		(1u << static_cast<uint32>(ECarGamePhase::CarGas))       |
		(1u << static_cast<uint32>(ECarGamePhase::EscapePolice)) |
		(1u << static_cast<uint32>(ECarGamePhase::DodgeMeteor));

	if ((GS->GetClearedPhasesMask() & AllPhases) == AllPhases)
	{
		GS->SetPhase(ECarGamePhase::Finished);
		UE_LOG("[CarGame] All phases cleared — Phase = Finished");
		return true;
	}
	return false;
}

// ============================================================
// Helpers
// ============================================================

ECarGamePhase AGameModeCarGame::TagToPhase(const FName& Tag)
{
	if (Tag == FName("CarWash"))      return ECarGamePhase::CarWash;
	if (Tag == FName("CarGas"))       return ECarGamePhase::CarGas;
	if (Tag == FName("EscapePolice")) return ECarGamePhase::EscapePolice;
	if (Tag == FName("DodgeMeteor"))  return ECarGamePhase::DodgeMeteor;
	return ECarGamePhase::None;
}

float AGameModeCarGame::GetPhaseDuration(ECarGamePhase Phase)
{
	switch (Phase)
	{
	case ECarGamePhase::CarWash:      return CarWashDuration;
	case ECarGamePhase::CarGas:       return CarGasDuration;
	case ECarGamePhase::EscapePolice: return EscapePoliceDuration;
	case ECarGamePhase::DodgeMeteor:  return DodgeMeteorDuration;
	default:                          return 0.0f;
	}
}

APawn* AGameModeCarGame::GetPlayerPawn() const
{
	if (APlayerController* PC = GetPlayerController())
	{
		return PC->GetPossessedPawn();
	}
	return nullptr;
}

// ============================================================
// Police spawn / despawn
// ============================================================

void AGameModeCarGame::SpawnPoliceCars(APawn* PlayerPawn)
{
	if (!PlayerPawn) return;
	UWorld* W = GetWorld();
	if (!W) return;

	DespawnPoliceCars();

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	constexpr int32 SpawnCount = 3;
	constexpr float MinRadius = 30.0f;
	constexpr float MaxRadius = 45.0f;

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		auto* Police = W->SpawnActor<APoliceCar>();
		if (!Police) continue;

		Police->SetTarget(PlayerPawn);

		const float RandT  = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		const float Angle  = (static_cast<float>(i) / SpawnCount + RandT * 0.1f) * 6.28318f;
		const float Radius = MinRadius + (MaxRadius - MinRadius) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
		FVector Offset(std::cos(Angle) * Radius, std::sin(Angle) * Radius, 1.0f);
		Police->SetActorLocation(PlayerLoc + Offset);

		SpawnedPolice.push_back(Police);

		UE_LOG("[Police] spawn idx=%d at (%.1f,%.1f,%.1f)", i,
			(PlayerLoc + Offset).X, (PlayerLoc + Offset).Y, (PlayerLoc + Offset).Z);
	}
}

void AGameModeCarGame::DespawnPoliceCars()
{
	UWorld* W = GetWorld();
	if (!W) { SpawnedPolice.clear(); return; }

	for (APoliceCar* P : SpawnedPolice)
	{
		if (P) W->DestroyActor(P);
	}
	SpawnedPolice.clear();
}
