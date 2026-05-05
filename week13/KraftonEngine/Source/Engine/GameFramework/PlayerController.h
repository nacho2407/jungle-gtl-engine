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

	// Pawn을 점유한다. 이미 다른 Pawn을 점유 중이면 먼저 해제.
	void Possess(APawn* Pawn);
	void UnPossess();

	APawn* GetPossessedPawn() const { return PossessedPawn; }

	// ─── Camera Manager ──────────────────────────────────────────
	// CameraManager 는 현재 World 가 소유하므로 World 로 위임.
	// UE: APlayerController::PlayerCameraManager (멤버) — 우리는 accessor 로 노출.
	APlayerCameraManager* GetPlayerCameraManager() const;

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
};
