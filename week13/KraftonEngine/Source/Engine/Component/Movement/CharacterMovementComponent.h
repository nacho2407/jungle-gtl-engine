#pragma once

#include "MovementComponent.h"
#include "Core/CollisionTypes.h"   // FHitResult
#include "Math/Vector.h"

// UE 의 EMovementMode minimal subset — 후속 단계에서 NavWalking/Swimming 등 확장.
enum class EMovementMode : uint8
{
	Walking,    // floor 위 — 평면 이동 + floor stick, Velocity.Z = 0.
	Falling,    // 공중 — gravity 적용, air control 만.
};

// UE 의 UCharacterMovementComponent minimal:
//   - Walking: 입력 → velocity (XY clamp by MaxWalkSpeed) → floor stick (raycast 로 capsule Z = floor + HalfHeight).
//     발 아래 floor 사라지면 자동 Falling.
//   - Falling: gravity 적용 (Velocity.Z -= Gravity*dt), air control (입력은 그대로 적용),
//     착지 시 자동 Walking + Velocity.Z = 0.
//   - Jump: 후속 phase (F-5).
//
// Floor detection: IPhysicsScene::Raycast — capsule 중심에서 down 으로 (HalfHeight + Probe).
// Owner 는 ignore 해서 자기 capsule 안 잡힘. Wall sweep 은 없으므로 벽 통과 가능 (minimal).

#include "Source/Engine/Component/Movement/CharacterMovementComponent.generated.h"

UCLASS()
class UCharacterMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UCharacterMovementComponent() = default;
	~UCharacterMovementComponent() override = default;

	// Controller 등 외부에서 매 frame 누적. TickComponent 가 ConsumeInputVector 로 비움.
	void AddInputVector(const FVector& WorldDirection, float ScaleValue = 1.0f);
	void ConsumeInputVector(FVector& OutAccumulated);

	const FVector& GetVelocity() const { return Velocity; }
	float          GetSpeed()    const { return Velocity.Length(); }

	EMovementMode  GetMovementMode() const { return MovementMode; }
	void           SetMovementMode(EMovementMode NewMode);
	bool           IsWalking() const { return MovementMode == EMovementMode::Walking; }
	bool           IsFalling() const { return MovementMode == EMovementMode::Falling; }

	// Walking 중이면 다음 Tick 에 Velocity.Z = JumpZVelocity, Mode → Falling. 비-Walking 이면 무시.
	// edge-triggered — input 측에서 Pressed 마다 호출.
	void           Jump();

	// UMovementComponent:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void Serialize(FArchive& Ar) override;

	// Editor-set 파라미터.
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Max Walk Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float MaxWalkSpeed       = 6.0f;     // m/s — Idle/Walk threshold 기준 정도
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Max Acceleration", Min=0.0f, Max=200.0f, Speed=0.5f)
	float MaxAcceleration    = 20.0f;    // m/s^2
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Braking Friction", Min=0.0f, Max=100.0f, Speed=0.1f)
	float BrakingFriction    = 8.0f;     // 입력 없을 때 감속률 (m/s^2). Walking 만 적용.
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Gravity", Min=0.0f, Max=100.0f, Speed=0.1f)
	float Gravity            = 9.8f;     // m/s^2 (positive — 적용 시 Velocity.Z -= Gravity*dt)
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Floor Probe Distance", Min=0.0f, Max=5.0f, Speed=0.01f)
	float FloorProbeDistance = 0.1f;     // capsule HalfHeight 아래 추가 probe 거리
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Jump Z Velocity", Min=0.0f, Max=50.0f, Speed=0.1f)
	float JumpZVelocity      = 6.0f;     // m/s — Jump 시 Velocity.Z 에 박는 값

	// UE 패턴 — true 면 매 frame Updated 의 yaw 를 현재 Velocity.XY 방향으로 lerp 회전.
	// 이동 중에만 회전 (정지 시 마지막 facing 유지). Pawn::bUseControllerRotationYaw 와 동시
	// 켜면 이쪽이 마지막 우선 (Component Tick 이 Actor Tick 후 호출). 보통 둘 중 하나만.
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Orient Rotation To Movement")
	bool  bOrientRotationToMovement = true;
	UPROPERTY(Edit, Save, Category="CharacterMovement", DisplayName="Rotation Yaw Rate", Min=0.0f, Max=3600.0f, Speed=5.0f)
	float RotationYawRate           = 540.0f;   // deg/sec

protected:
	// XY 입력을 velocity 에 반영 + Walking 시 braking. 양 mode 공통 호출.
	void  ApplyInputToVelocity(const FVector& Input, float DeltaTime);

	// Mode 별 Z 처리 + 위치 갱신.
	void  TickWalking(float DeltaTime);
	void  TickFalling(float DeltaTime);

	// capsule 중심에서 down raycast — bHit + WorldHitLocation 사용.
	bool  TraceFloor(FHitResult& OutHit) const;
	float GetCapsuleHalfHeight() const;

	FVector       AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
	FVector       Velocity         = FVector(0.0f, 0.0f, 0.0f);
	// 시작 시 floor 잡힐 때까지 Falling — 첫 frame TickFalling 이 raycast 후 자동 Walking 전환.
	EMovementMode MovementMode     = EMovementMode::Falling;

	// Jump() 가 set, TickWalking 이 consume. edge-triggered 라 동일 프레임 다중 호출도 1회 점프.
	bool          bWantsJump       = false;

	// 평면 속도 기준 yaw 를 RotationYawRate * dt 로 lerp. TickComponent 끝에서 적용.
	void  PhysOrientToMovement(float DeltaTime);
};
