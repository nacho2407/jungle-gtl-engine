#pragma once

#include "Component/SceneComponent.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

// ============================================================
// USpringArmComponent — 부착 액터 뒤를 부드럽게 따라가는 카메라 부착점.
//
// 사용 패턴: Pawn(Owner) → SpringArm(자식) → Camera(SpringArm 의 자식).
// SpringArm 의 World 는 매 Tick 에 부모의 World 를 따라 갱신되되, lag 옵션이
// 켜져 있으면 부드러운 보간으로 따라온다. Camera 컴포넌트는 SpringArm 의
// World 를 자동 상속하므로 별도 후크 없이 부드럽게 끌려오는 효과가 난다.
//
// 차량/플레이어 뒤를 따라오는 3인칭 카메라, 흔들림 있는 카메라 마운트 등에 사용.
// 충돌 인지(raycast) 는 별도 PR 에서 추가 — 현재는 lag 만 처리.
// UE: USpringArmComponent (간소화)
// ============================================================
class USpringArmComponent : public USceneComponent
{
public:
	DECLARE_CLASS(USpringArmComponent, USceneComponent)

	USpringArmComponent() = default;
	~USpringArmComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	// ─── 튜닝 파라미터 ─────────────────────────────────────────────
	// arm 길이 — 부착점에서 카메라까지의 거리 (Local -X 방향).
	float TargetArmLength = 300.0f;

	// arm 끝점(카메라 위치) 에 추가되는 offset (Lagged 회전 기준 적용).
	FVector SocketOffset = FVector(0.0f, 0.0f, 0.0f);

	// 부착점 자체에 추가되는 offset (Lagged 회전 기준 적용). 보통 머리 위/어깨 높이.
	FVector TargetOffset = FVector(0.0f, 0.0f, 0.0f);

	// Lag 옵션 — 끄면 부모를 즉시 따라감 (lag 없는 부착).
	bool bEnableCameraLag = false;
	bool bEnableCameraRotationLag = false;
	float CameraLagSpeed = 10.0f;          // 클수록 빠르게 따라옴
	float CameraRotationLagSpeed = 10.0f;
	float CameraLagMaxDistance = 0.0f;     // 0 = 무제한

private:
	// 매 Tick 에 갱신되는 보간 상태 — 부착점 (parent + TargetOffset) 위치/회전.
	FVector LaggedAttachLoc = FVector(0.0f, 0.0f, 0.0f);
	FQuat LaggedAttachRot;
	bool bHasPreviousState = false;
};
