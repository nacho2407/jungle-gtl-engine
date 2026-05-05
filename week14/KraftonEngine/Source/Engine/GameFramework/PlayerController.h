#pragma once

#include "GameFramework/AActor.h"
#include "GameFramework/CameraTypes.h"

class APawn;
class APlayerCameraManager;

// ============================================================
// APlayerController — 플레이어의 의도(Possess/입력)를 Pawn에 전달
//
// Pawn은 "조종 가능한 액터"이고, PlayerController는 "조종자".
// World당 (지금은) 1개만 spawn되며 GameMode가 spawn/관리.
// ============================================================
class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;

	// E.2/1: BeginPlay 에서 World->GetCameraManager() 캐싱. E.3 에서 직접 SpawnActor 로 전환 예정.
	void BeginPlay() override;

	// Pawn을 점유한다. 이미 다른 Pawn을 점유 중이면 먼저 해제.
	void Possess(APawn* Pawn);
	void UnPossess();

	APawn* GetPossessedPawn() const { return PossessedPawn; }

	// ─── Camera Manager ──────────────────────────────────────────
	// UE: APlayerController::PlayerCameraManager 멤버. 현재는 World 가 owner 이고 PC 는 reference 만 보유.
	// E.2 청크 3 에서 World 의 CameraManager 멤버가 제거되면 PC 가 직접 SpawnActor 로 owner.
	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }

	// ─── View Target ─────────────────────────────────────────────
	// 새 view target 으로 전환 (블렌드 가능). UCameraComponent 가 붙어있는 액터 권장.
	// UE: APlayerController::SetViewTargetWithBlend
	virtual void SetViewTargetWithBlend(
		AActor* NewViewTarget,
		float BlendTime = 0.0f,
		EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Linear,
		float BlendExp = 0.0f,
		bool bLockOutgoing = false);

private:
	APawn* PossessedPawn = nullptr;  // 직렬화 제외

	// PlayerCameraManager — UE 의 PC->PlayerCameraManager 와 동일 의미. 직렬화 제외.
	// 현재(E.2 청크 1)는 World 의 CameraManager 를 reference 로 캐싱. E.2 청크 3 에서
	// PC 가 BeginPlay 에서 직접 SpawnActor 로 생성하는 owner 로 전환.
	APlayerCameraManager* PlayerCameraManager = nullptr;
};
