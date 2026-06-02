#pragma once

#include "Component/Movement/MovementComponent.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/Vector.h"
#include "Math/Transform.h"

// UE 의 EMovementMode minimal subset — 후속 단계에서 NavWalking/Swimming 등 확장.
enum class EMovementMode : uint8
{
	Walking,    // floor 위 — 평면 이동 + floor stick, Velocity.Z = 0.
	Falling,    // 공중 — gravity 적용, air control 만.
};

// Walking:
//   input/root-motion XY + small downward probe -> PhysX CCT move.
//   eCOLLISION_DOWN keeps Walking; short misses are tolerated by GroundMissToleranceFrames.
// Falling:
//   gravity -> PhysX CCT move.
//   downward hit switches back to Walking.
// Collision:
//   movement uses PhysX Character Controller sweep with Pawn-channel query filtering.

#include "Source/Engine/Component/Movement/CharacterMovementComponent.generated.h"

namespace physx
{
	class PxController;
}

struct FControllerMoveResult
{
	bool bHitDown = false;
};

UCLASS()
class UCharacterMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UCharacterMovementComponent();
	~UCharacterMovementComponent() override = default;

	void EndPlay() override;

	// Controller 등 외부에서 매 frame 누적. TickComponent 가 ConsumeInputVector 로 비움.
	void AddInputVector(const FVector& WorldDirection, float ScaleValue = 1.0f);

	// Root motion delta 입력 — local 좌표계 (root 본 기준) 의 한 프레임 분.
	// 호출자 (보통 ACharacter::Tick 또는 CMC 가 직접 mesh anim instance 에서) 가 매 frame 누적.
	// 여러 번 호출 시 합성됨 (translation 합산, rotation quat 곱). TickComponent 가 1회 소비.
	// CMC 는 mode 를 모름 — "받으면 적용" 만. 어디서 가져올지는 AnimInstance::RootMotionMode 가 결정.
	void AddRootMotionDelta(const FTransform& LocalDelta);
	bool HasPendingRootMotion() const { return bHasPendingRootMotion; }

	void SetMovementMode(EMovementMode NewMode);
	void Jump();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 직전 TickComponent 에서 root motion 의 yaw 가 capsule rotation 에 실제로 적용됐는지.
	// ACharacter::Tick 이 control yaw 를 capsule 에 덮어쓰기 전에 query 해서 충돌 회피
	// (root motion 회전이 활성 중인 frame 은 control yaw 가 덮으면 회전이 토글되어 끊김).
	bool HasYawDrivenByRootMotion() const { return bAppliedRootMotionYawThisFrame; }

	const FVector& GetVelocity() const { return Velocity; }
	float          GetSpeed()    const { return Velocity.Length(); }

	EMovementMode  GetMovementMode() const { return MovementMode; }
	bool           IsWalking() const { return MovementMode == EMovementMode::Walking; }
	bool           IsFalling() const { return MovementMode == EMovementMode::Falling; }

	// UMovementComponent:
	void Serialize(FArchive& Ar) override;

protected:
	// XY 입력을 velocity 에 반영 + Walking 시 braking. 양 mode 공통 호출.
	void  ApplyInputToVelocity(const FVector& Input, float DeltaTime);

	// Mode 별 Z 처리 + 위치 갱신.
	// RootMotionWorldXY 는 이번 frame 의 root motion 평면 변위 (world frame, Z=0 보장).
	// XY 적용 단계에 합산되고 floor stick / gravity 는 mode 가 자체 결정.
	void  TickWalking(float DeltaTime, const FVector& RootMotionWorldXY);
	void  TickFalling(float DeltaTime, const FVector& RootMotionWorldXY);

	FVector       AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
	FVector       Velocity         = FVector(0.0f, 0.0f, 0.0f);
	// 시작 시 floor 잡힐 때까지 Falling — 첫 frame TickFalling 이 raycast 후 자동 Walking 전환.
	EMovementMode MovementMode     = EMovementMode::Falling;

	// Jump() 가 set, TickWalking 이 consume. edge-triggered 라 동일 프레임 다중 호출도 1회 점프.
	bool          bWantsJump       = false;

	int32 GroundMissFrames = 0;

	// Root motion 누적 buffer — 매 frame AddRootMotionDelta 로 합성, TickComponent 가 1회 소비.
	// PendingRootMotion 이 identity 라도 "이번 frame 에 root motion 이 있었다" 와 구분 필요해 bool 별도.
	FTransform    PendingRootMotion;
	bool          bHasPendingRootMotion = false;

	// 직전 TickComponent 에서 root motion yaw 가 실제 적용됐는지 (외부 query 용 — Character 의 yaw 가드).
	// 매 Tick 시작에 reset 후 yaw 적용 시 true.
	bool          bAppliedRootMotionYawThisFrame = false;

	// 평면 속도 기준 yaw 를 RotationYawRate * dt 로 lerp. TickComponent 끝에서 적용.
	void  PhysOrientToMovement(float DeltaTime);

private:
	bool EnsureController();
	void ReleaseController();

	void SyncUpdatedComponentFromController();
	void SyncControllerToUpdatedComponentIfNeeded();

	FControllerMoveResult MoveController(const FVector& Delta, float DeltaTime);
	
	void ConsumeInputVector(FVector& OutAccumulated);
	bool ConsumePendingRootMotion(FTransform& OutLocalDelta);

public:
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Walk Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float MaxWalkSpeed = 6.0f;     // m/s — Idle/Walk threshold 기준 정도
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Acceleration", Min = 0.0f, Max = 200.0f, Speed = 0.5f)
	float MaxAcceleration = 20.0f;    // m/s^2
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Braking Friction", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float BrakingFriction = 8.0f;     // 입력 없을 때 감속률 (m/s^2). Walking 만 적용.
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Gravity", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float Gravity = 9.8f;     // m/s^2 (positive — 적용 시 Velocity.Z -= Gravity*dt)
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Floor Probe Distance", Min = 0.0f, Max = 5.0f, Speed = 0.01f)
	float FloorProbeDistance = 0.1f;     // capsule HalfHeight 아래 추가 probe 거리
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Jump Z Velocity", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float JumpZVelocity = 6.0f;     // m/s — Jump 시 Velocity.Z 에 박는 값

	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Orient Rotation To Movement")
	bool  bOrientRotationToMovement = true;
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Rotation Yaw Rate", Min = 0.0f, Max = 3600.0f, Speed = 5.0f)
	float RotationYawRate = 540.0f;   // deg/sec

private:
	physx::PxController* Controller = nullptr;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Contact Offset", Min = 0.001f, Max = 1.0f, Speed = 0.01f)
	float ControllerContactOffset = 0.05f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Max Step Height", Min = 0.0f, Max = 5.0f, Speed = 0.01f)
	float MaxStepHeight = 0.4f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Walkable Slope Angle", Min = 0.0f, Max = 89.0f, Speed = 1.0f)
	float WalkableSlopeAngle = 45.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Min Move Distance", Min = 0.0f, Max = 0.1f, Speed = 0.001f)
	float ControllerMinMoveDistance = 0.001f;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Ground Miss Tolerance Frames", Min = 0, Max = 10, Speed = 1)
	int32 GroundMissToleranceFrames = 2;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Sync Teleport Distance", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float ControllerSyncTeleportDistance = 1.0f;

	USceneComponent* ControllerUpdatedComponent = nullptr;
	float CachedControllerRadius = 0.0f;
	float CachedControllerHalfHeight = 0.0f;
	float CachedControllerContactOffset = 0.0f;
	float CachedMaxStepHeight = 0.0f;
	float CachedWalkableSlopeAngle = 0.0f;
};
