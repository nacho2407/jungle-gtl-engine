#include "Game/GameMode/GameModeCarGame.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Game/PlayerController/PlayerControllerCarGame.h"
#include "Game/Pawn/PoliceCar.h"
#include "Game/Pawn/CarPawn.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "GameFramework/GameplayStatics.h"
#include "Game/Component/CarGasComponent.h"
#include "Game/Component/DirtComponent.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "Core/Log.h"

#include <cmath>
#include <cstdlib>

IMPLEMENT_CLASS(AGameModeCarGame, AGameModeBase)

namespace
{
	FString GetScorePhaseName(ECarGamePhase Phase)
	{
		switch (Phase)
		{
		case ECarGamePhase::CarWash:      return "Car Wash";
		case ECarGamePhase::CarGas:       return "Gas Fill";
		case ECarGamePhase::EscapePolice: return "Escape Police";
		case ECarGamePhase::DodgeMeteor:  return "Dodge Meteor";
		case ECarGamePhase::Goal:         return "Goal";
		default:                          return "Phase";
		}
	}
}

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
		GS->SetMatchTimerRunning(false);
		GS->SetLastEndedPhase(ECarGamePhase::None);
		GS->SetLastPhaseResult(EPhaseResult::None);
		GS->SetHealth(GS->GetMaxHealth());
		GS->SetFinishOutcome(EFinishOutcome::None);
		GS->ResetScore();
		UE_LOG("[CarGame] StartMatch — Phase = None, MatchTime=%.1fs, HP=%d/%d",
			MatchDuration, GS->GetHealth(), GS->GetMaxHealth());
	}
}

void AGameModeCarGame::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;
	if (GS->GetPhase() == ECarGamePhase::Finished) return;

	// ── 매치 전체 타이머 ──
	if (GS->IsMatchTimerRunning())
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

			// 매치 시간 만료 시점에 모든 페이즈가 클리어돼 있으면 Win, 아니면 Lose.
			constexpr uint32 AllPhases =
				(1u << static_cast<uint32>(ECarGamePhase::CarWash))      |
				(1u << static_cast<uint32>(ECarGamePhase::CarGas))       |
				(1u << static_cast<uint32>(ECarGamePhase::EscapePolice)) |
				(1u << static_cast<uint32>(ECarGamePhase::DodgeMeteor)) |
				(1u << static_cast<uint32>(ECarGamePhase::Goal));
			const bool bAllCleared = (GS->GetClearedPhasesMask() & AllPhases) == AllPhases;
			GS->SetFinishOutcome(bAllCleared ? EFinishOutcome::Win : EFinishOutcome::Lose);

			ApplyMatchEndBonus();
			GS->SetPhase(ECarGamePhase::Finished);
			UE_LOG("[CarGame] Match time elapsed — Phase = Finished, Outcome=%s",
				bAllCleared ? "Win" : "Lose");
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
		// DodgeMeteor: HP 0 워치독 — JudgePhaseResult 는 timer 만료 시에만 호출되므로
		// HP 가 mid-phase 에 0 이 되면 timer 만료까지 대기. 폴링으로 즉시 Failed 처리.
		if (Phase == ECarGamePhase::DodgeMeteor)
		{
			if (auto* Car = Cast<ACarPawn>(GetPlayerPawn()))
			{
				if (Car->GetMeteorHealth() <= 0.0f)
				{
					GS->SetRemainingPhaseTime(0.0f);
					EndPhase(EPhaseResult::Failed);
					return;
				}
			}
		}

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

	// 현재 quest 의 트리거는 그대로 진입 허용. 이미 클리어한 페이즈는 quest 와 무관하게
	// 자유 재진입(replay) 허용. 아직 클리어 안 했고 quest 도 다른 페이즈를 가리키고 있으면
	// 차단 — 페이즈 순서 / 진행 가드 유지.
	if (GS->GetQuestPhase() != Target)
	{
		const uint32 ClearedBit = 1u << static_cast<uint32>(Target);
		if ((GS->GetClearedPhasesMask() & ClearedBit) == 0) return;
	}

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

void AGameModeCarGame::SuccessPhase()
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;
	if (GS->GetPhase() == ECarGamePhase::None || GS->GetPhase() == ECarGamePhase::Result) return;

	EPhaseResult Result = EPhaseResult::Success;

	UE_LOG("[CarGame] SuccessPhase called — Phase=%d Result=%d, RemainingPhaseTime=%.2f",
		static_cast<int32>(GS->GetPhase()), static_cast<int32>(Result), GS->GetRemainingPhaseTime());

	// EndPhase 가 Result 페이즈로 전이하면서 RemainingPhaseTime 을 ResultDisplayDuration
	// 로 덮어쓰므로 여기서 0 으로 만들 필요 없음. 오히려 0 으로 만들면 Score Time Bonus
	// 계산이 항상 0 이 돼서 Lua-driven 페이즈 클리어가 base 점수만 받음.
	EndPhase(Result);
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
	else if (Target == ECarGamePhase::CarWash)
	{
		// replay 진입 — 이전 클리어 시 invisible/washed 처리된 dirt 컴포넌트들을
		// 모두 dirty 상태로 되돌려놓고 다시 시작. 첫 진입에서도 idempotent.
		if (UWorld* World = GetWorld())
		{
			for (AActor* Actor : World->GetActors())
			{
				if (Actor && Actor->GetFName() == FName("DirtyCar"))
				{
					UDirtComponent::ResetAllOnActor(*Actor);
					break;
				}
			}
		}
	}
	else if (Target == ECarGamePhase::Goal)
	{
		// 골인 트리거 진입 자체가 성공. BeginPhase 안에서 즉시 EndPhase 호출 →
		// MarkPhaseCleared + Result 페이즈 → 다음 Result tick 의 TryFinishOnAllCleared
		// 에서 Goal 비트까지 set 되었음을 보고 Phase=Finished, Outcome=Win.
		EndPhase(EPhaseResult::Success);
	}
	else if (Target == ECarGamePhase::DodgeMeteor)
	{
		// 운석 페이즈 시작 — 차량 MeteorHealth 를 max 로 리셋. replay 시에도 동일 시작 조건.
		if (auto* Car = Cast<ACarPawn>(GetPlayerPawn()))
		{
			Car->SetMeteorHealth(Car->GetMaxMeteorHealth());
		}
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

		// Base + 잔여시간 비례 보너스. 시간 다 써서 클리어해도 base 는 보장.
		const float Duration  = GetPhaseDuration(Cur);
		const float Remaining = GS->GetRemainingPhaseTime();
		const float Ratio     = (Duration > 0.0f) ? (Remaining / Duration) : 0.0f;
		const int32 TimeBonus = static_cast<int32>(Ratio * static_cast<float>(PhaseTimeBonusMax));
		const int32 PhaseScore = BasePhaseScore + TimeBonus;
		GS->AddScore(PhaseScore, EScoreCategory::Phase, GetScorePhaseName(Cur), Cur);
		UE_LOG("[CarGame] Phase score +%d (base=%d, time=%d, total=%d)",
			PhaseScore, BasePhaseScore, TimeBonus, GS->GetScore());
	}
	else if (Result == EPhaseResult::Failed)
	{
		GS->LoseHealth(1);
		UE_LOG("[CarGame] Phase failed — HP=%d/%d", GS->GetHealth(), GS->GetMaxHealth());
	}

	GS->SetLastEndedPhase(Cur);
	GS->SetLastPhaseResult(Result);

	// HP 소진 시 즉시 게임오버 — Result 페이즈 거치지 않고 Finished 로 전이.
	if (GS->GetHealth() <= 0)
	{
		GS->SetRemainingPhaseTime(0.0f);
		GS->SetFinishOutcome(EFinishOutcome::Lose);
		ApplyMatchEndBonus();
		GS->SetPhase(ECarGamePhase::Finished);
		UE_LOG("[CarGame] HP depleted — Phase = Finished, Outcome=Lose");
		return;
	}

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
		UWorld* World = GetWorld();
		if (!World) return EPhaseResult::Failed;

		AActor* DirtOwner = nullptr;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Cast<UDirtComponent>(Component))
				{
					DirtOwner = Actor;
					break;
				}
			}

			if (DirtOwner)
			{
				break;
			}
		}

		if (!DirtOwner) return EPhaseResult::Failed;
		return UDirtComponent::AreAllDirtComponentsWashed(*DirtOwner)
			? EPhaseResult::Success : EPhaseResult::Failed;
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
		return Car && Car->GetMeteorHealth() > 0.0f
			? EPhaseResult::Success : EPhaseResult::Failed;

	case ECarGamePhase::Goal:
		// 트리거 진입 자체가 성공 — BeginPhase 가 즉시 EndPhase(Success) 를 호출하므로
		// 여기까진 보통 도달하지 않지만, match-time-elapsed 같은 경로로 호출되면 Success.
		return EPhaseResult::Success;

	default:
		return EPhaseResult::None;
	}
}

void AGameModeCarGame::ApplyMatchEndBonus()
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;

	const float SafeRemaining = GS->GetRemainingMatchTime() < 0.0f ? 0.0f : GS->GetRemainingMatchTime();
	const int32 TimeBonus   = static_cast<int32>(SafeRemaining) * MatchTimeBonusPerSec;
	const int32 HealthBonus = GS->GetHealth() * HealthBonusPerHP;
	const int32 Total       = TimeBonus + HealthBonus;
	if (Total != 0)
	{
		GS->AddScore(Total, EScoreCategory::MatchEnd, "Match End Bonus", ECarGamePhase::Finished);
	}
	UE_LOG("[CarGame] Match-end bonus: time=%d, hp=%d, total=%d (score=%d)",
		TimeBonus, HealthBonus, Total, GS->GetScore());
}

bool AGameModeCarGame::TryFinishOnAllCleared()
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return false;

	constexpr uint32 AllPhases =
		(1u << static_cast<uint32>(ECarGamePhase::CarWash))      |
		(1u << static_cast<uint32>(ECarGamePhase::CarGas))       |
		(1u << static_cast<uint32>(ECarGamePhase::EscapePolice)) |
		(1u << static_cast<uint32>(ECarGamePhase::DodgeMeteor)) |
		(1u << static_cast<uint32>(ECarGamePhase::Goal));

	if ((GS->GetClearedPhasesMask() & AllPhases) == AllPhases)
	{
		GS->SetFinishOutcome(EFinishOutcome::Win);
		ApplyMatchEndBonus();
		GS->SetPhase(ECarGamePhase::Finished);
		UE_LOG("[CarGame] All phases cleared — Phase = Finished, Outcome=Win");
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
	if (Tag == FName("GoalTrigger"))  return ECarGamePhase::Goal;
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
	case ECarGamePhase::Goal:         return GoalDuration;
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

	// 월드에 미리 배치된 "SpawnPoliceCar" 태그 액터들의 위치마다 경찰차 1마리씩 spawn.
	// 디자이너가 에디터에서 위치 / 개수 / 각도 모두 조정 가능. 마커 액터의 종류는 무관 —
	// AActor 의 Tags 만 보고 위치 추출.
	const TArray<AActor*> Spawners = FGameplayStatics::FindActorsByTag(W, FName("SpawnPoliceCar"));
	if (Spawners.empty())
	{
		UE_LOG("[Police] No actor tagged 'SpawnPoliceCar' — no police spawned");
		return;
	}

	for (size_t i = 0; i < Spawners.size(); ++i)
	{
		AActor* Spawner = Spawners[i];
		if (!Spawner) continue;

		auto* Police = W->SpawnActor<APoliceCar>();
		if (!Police) continue;

		Police->SetTarget(PlayerPawn);

		const FVector SpawnLoc = Spawner->GetActorLocation();
		const FRotator SpawnRot = Spawner->GetActorRotation();
		Police->SetActorLocation(SpawnLoc);
		Police->SetActorRotation(SpawnRot);

		SpawnedPolice.push_back(Police);

		UE_LOG("[Police] spawn idx=%zu at (%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) (marker=%s)", i,
			SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z,
			SpawnRot.Roll, SpawnRot.Pitch, SpawnRot.Yaw,
			Spawner->GetFName().ToString().c_str());
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
