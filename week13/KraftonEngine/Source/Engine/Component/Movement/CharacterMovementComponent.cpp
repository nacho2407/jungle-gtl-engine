#include "CharacterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Core/TickFunction.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/IPhysicsScene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

UCharacterMovementComponent::UCharacterMovementComponent()
{
	// USkeletalMeshComponent::TickComponent (TG_PrePhysics, default) 가 UpdateAnimation 으로
	// AnimInstance->PendingRootMotion 을 채운 다음에 CMC 가 그 값을 가져가야 같은 frame 데이터를
	// 쓸 수 있다. Prerequisite API 가 우리 엔진에 없으므로 TickGroup 분리로 순서 보장.
	// FTickManager 가 group 순서대로 실행하므로 PrePhysics 가 모두 끝난 뒤 DuringPhysics 가 돈다.
	PrimaryComponentTick.SetTickGroup(TG_DuringPhysics);
	PrimaryComponentTick.SetEndTickGroup(TG_DuringPhysics);
}

void UCharacterMovementComponent::AddInputVector(const FVector& WorldDirection, float ScaleValue)
{
	AccumulatedInput = AccumulatedInput + WorldDirection * ScaleValue;
}

void UCharacterMovementComponent::ConsumeInputVector(FVector& Out)
{
	Out = AccumulatedInput;
	AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
}

void UCharacterMovementComponent::AddRootMotionDelta(const FTransform& LocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		PendingRootMotion = LocalDelta;
		bHasPendingRootMotion = true;
		return;
	}

	// 누적 합성 — AnimInstance::AccumulateRootMotion 과 동일한 매트릭스 곱 패턴.
	// 같은 frame 에 base + montage 처럼 여러 소스가 push 할 수 있어 합성 보장 필요.
	const FMatrix M = LocalDelta.ToMatrix() * PendingRootMotion.ToMatrix();
	PendingRootMotion.Location = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	PendingRootMotion.Rotation = (LocalDelta.Rotation * PendingRootMotion.Rotation).GetNormalized();
	// Scale 은 root motion 에서 보통 1 — 무시.
}

bool UCharacterMovementComponent::ConsumePendingRootMotion(FTransform& OutLocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		OutLocalDelta = FTransform();   // Identity
		return false;
	}
	OutLocalDelta = PendingRootMotion;
	PendingRootMotion = FTransform();
	bHasPendingRootMotion = false;
	return true;
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode) return;
	MovementMode = NewMode;
	// 추후 OnMovementModeChanged delegate 위치.
}

void UCharacterMovementComponent::Jump()
{
	// Walking 중에만 점프 허용 — 공중 다단 점프 막음. (필요 시 자식 override.)
	if (MovementMode != EMovementMode::Walking) return;
	bWantsJump = true;
}

void UCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;
	if (DeltaTime <= 0.0f) return;

	// 매 Tick 회전 적용 상태 reset — 이번 frame 에 root motion 이 yaw 를 적용했는지를
	// 외부 (Character::Tick) 가 query 할 수 있어야 yaw 충돌 회피 가능.
	bAppliedRootMotionYawThisFrame = false;

	FVector Input;
	ConsumeInputVector(Input);
	Input.Z = 0.0f;   // XY 평면만 — Z 는 mode 가 결정.

	// 1) Input 처리 — XY velocity 갱신 (양 mode 공통).
	ApplyInputToVelocity(Input, DeltaTime);

	// 1.5) Owner Character 의 Mesh AnimInstance 가 누적해둔 root motion 을 가져와 자기 buffer 로 push.
	//      Mesh tick (TG_PrePhysics) 이 이미 끝나 PendingRootMotion 이 채워진 상태.
	//      Mode 가 Ignore 면 가져갈 필요 자체가 없음 (AccumulateRootMotion 측에서 누적도 안 됨).
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AI = Mesh->GetAnimInstance())
			{
				if (AI->GetRootMotionMode() != ERootMotionMode::IgnoreRootMotion)
				{
					AddRootMotionDelta(AI->ConsumeRootMotion());
				}
			}
		}
	}

	// 2) Root motion 소비 — local delta 를 world frame 으로 변환 (Updated 의 yaw 기준).
	//    XY 만 mode 분기로 위임. Z 는 두 mode 모두 무시:
	//      Walking — floor stick 이 Z 결정
	//      Falling — gravity 가 Z 결정
	//    Climbing/Swimming 같은 mode 추가 시 그때 재검토.
	FTransform RootMotionDelta;
	const bool bHadRootMotion = ConsumePendingRootMotion(RootMotionDelta);
	FVector RootMotionWorldXY(0.0f, 0.0f, 0.0f);
	if (bHadRootMotion)
	{
		const FRotator ActorRot = Updated->GetWorldRotation();
		const FQuat    YawOnly  = FRotator(0.0f, 0.0f, ActorRot.Yaw).ToQuaternion();
		const FVector  World    = YawOnly.RotateVector(RootMotionDelta.Location);
		RootMotionWorldXY.X     = World.X;
		RootMotionWorldXY.Y     = World.Y;
	}

	// 3) Mode 별 Z 처리 + 위치 적용 (input velocity + root motion XY 합산).
	if (MovementMode == EMovementMode::Walking)
	{
		TickWalking(DeltaTime, RootMotionWorldXY);
	}
	else
	{
		TickFalling(DeltaTime, RootMotionWorldXY);
	}

	// 4) Root motion yaw 적용. yaw 만 추출 — root motion 의 pitch/roll 은 캐릭터 capsule
	//    회전에 일반적으로 의미 없음 (UE 도 yaw 만 적용).
	//    yaw 가 적용되면 bAppliedRootMotionYawThisFrame 을 켜서 PhysOrientToMovement /
	//    Character 의 control yaw 덮어쓰기 둘 다 같은 frame skip 되도록 한다.
	if (bHadRootMotion)
	{
		const FRotator DeltaRot = RootMotionDelta.Rotation.ToRotator();
		if (std::fabs(DeltaRot.Yaw) > 1e-4f)
		{
			FRotator R = Updated->GetRelativeRotation();
			R.Yaw += DeltaRot.Yaw;
			Updated->SetRelativeRotation(R);
			bAppliedRootMotionYawThisFrame = true;
		}
	}

	// 5) Orient yaw to movement direction. Root motion 이 yaw 를 잡고 있는 frame 은 skip —
	//    그렇지 않으면 PhysOrient 가 root motion 회전을 Velocity 방향으로 다시 lerp 해
	//    의도된 회전이 무효화된다 (turn-in-place anim 가장 큰 피해).
	if (bOrientRotationToMovement && !bAppliedRootMotionYawThisFrame)
	{
		PhysOrientToMovement(DeltaTime);
	}
}

void UCharacterMovementComponent::PhysOrientToMovement(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	// 평면 속도 작으면 회전 skip — 마지막 facing 유지.
	const float SpeedSq2D = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y;
	constexpr float MinSpeedSq = 1e-4f;
	if (SpeedSq2D < MinSpeedSq) return;

	// Target yaw — Velocity 방향. UE 의 atan2(Y, X) 는 +X 가 0°, +Y 가 90° (좌표계 가정).
	const float TargetYaw = std::atan2(Velocity.Y, Velocity.X) * (180.0f / 3.14159265f);

	FRotator R = Updated->GetRelativeRotation();
	const float CurrentYaw = R.Yaw;

	// 최단 회전 방향 (delta ∈ [-180, 180])
	float Delta = TargetYaw - CurrentYaw;
	while (Delta >  180.0f) Delta -= 360.0f;
	while (Delta < -180.0f) Delta += 360.0f;

	const float Step = RotationYawRate * DeltaTime;
	if (std::fabs(Delta) <= Step)
	{
		R.Yaw = TargetYaw;
	}
	else
	{
		R.Yaw = CurrentYaw + (Delta > 0.0f ? Step : -Step);
	}
	Updated->SetRelativeRotation(R);
}

void UCharacterMovementComponent::ApplyInputToVelocity(const FVector& Input, float DeltaTime)
{
	const float InputLen = Input.Length();
	if (InputLen > 0.0f)
	{
		// 입력 방향으로 가속 (XY 만).
		const FVector Direction = Input * (1.0f / InputLen);
		Velocity.X += Direction.X * MaxAcceleration * DeltaTime;
		Velocity.Y += Direction.Y * MaxAcceleration * DeltaTime;
	}
	else if (MovementMode == EMovementMode::Walking)
	{
		// Walking 에선 input 없으면 braking. Falling 중 air control 없음 = 평면 속도 유지.
		FVector V2D(Velocity.X, Velocity.Y, 0.0f);
		const float Speed2D = V2D.Length();
		if (Speed2D > 0.0f)
		{
			const float NewSpeed = std::max(0.0f, Speed2D - BrakingFriction * DeltaTime);
			const FVector Dir    = V2D * (1.0f / Speed2D);
			Velocity.X = Dir.X * NewSpeed;
			Velocity.Y = Dir.Y * NewSpeed;
		}
	}

	// MaxWalkSpeed 클램프 (평면 속도만).
	FVector V2D(Velocity.X, Velocity.Y, 0.0f);
	const float Speed2D = V2D.Length();
	if (Speed2D > MaxWalkSpeed)
	{
		const FVector Dir = V2D * (1.0f / Speed2D);
		Velocity.X = Dir.X * MaxWalkSpeed;
		Velocity.Y = Dir.Y * MaxWalkSpeed;
	}
}

void UCharacterMovementComponent::TickWalking(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Jump 의도가 있으면 — Velocity.Z 박고 즉시 Falling 으로 전환. 이 frame 의 XY 는 그대로 진행.
	if (bWantsJump)
	{
		bWantsJump = false;
		Velocity.Z = JumpZVelocity;
		SetMovementMode(EMovementMode::Falling);
		// XY 이동은 Falling 분기로 위임 — 한 frame 안 mode 전환이라 즉시 falling tick.
		TickFalling(DeltaTime, RootMotionWorldXY);
		return;
	}

	// Walking 중 Z velocity 는 0 — floor stick 으로만 Z 결정.
	Velocity.Z = 0.0f;

	// XY 이동: input velocity * dt + root motion XY (이미 world frame).
	const FVector XYOffset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		0.0f);
	Updated->SetWorldLocation(Updated->GetWorldLocation() + XYOffset);

	// Floor 잡혔는지 — 이동 직후 위치에서 다시 trace.
	FHitResult Floor;
	if (!TraceFloor(Floor))
	{
		// 발 아래 floor 없음 (예: 절벽 끝) → falling 전환.
		SetMovementMode(EMovementMode::Falling);
		return;
	}

	// Floor stick — capsule 중심 = floor.Z + HalfHeight.
	FVector NewLoc = Updated->GetWorldLocation();
	NewLoc.Z = Floor.WorldHitLocation.Z + GetCapsuleHalfHeight();
	Updated->SetWorldLocation(NewLoc);
}

void UCharacterMovementComponent::TickFalling(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Gravity — Z 만. (양수 Gravity → -Z 가속)
	Velocity.Z -= Gravity * DeltaTime;

	// Velocity * dt 의 XY 에 root motion XY 합산. Z 는 gravity 가 책임이라 root motion 무시.
	const FVector Offset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		Velocity.Z * DeltaTime);
	Updated->SetWorldLocation(Updated->GetWorldLocation() + Offset);

	// 올라가는 중 (점프 arc 상승) 엔 floor 체크 skip — 안 그러면 점프 직후 1 frame 의
	// 작은 상승 (≈ JumpZVelocity * dt) 이 raycast probe 거리 안에 있어 즉시 착지로 잡힘.
	// UE 도 동일 — Velocity.Z > 0 이면 ground 안 잡음.
	if (Velocity.Z > 0.0f) return;

	// 떨어지는 중에만 floor 체크.
	FHitResult Floor;
	if (!TraceFloor(Floor)) return;

	// 착지 — capsule Z 보정 + Walking 전환 + Velocity.Z = 0.
	// raycast 가 hit 했다는 건 capsule bottom 이 floor 위 (또는 약간 안) 에 있다는 뜻.
	// hit 위치를 floor 표면으로 보고 그 위에 stick.
	FVector LandLoc = Updated->GetWorldLocation();
	LandLoc.Z = Floor.WorldHitLocation.Z + GetCapsuleHalfHeight();
	Updated->SetWorldLocation(LandLoc);
	Velocity.Z = 0.0f;
	SetMovementMode(EMovementMode::Walking);
}

bool UCharacterMovementComponent::TraceFloor(FHitResult& OutHit) const
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;
	AActor* Owner = GetOwner();
	if (!Owner) return false;
	UWorld* World = Owner->GetWorld();
	if (!World) return false;

	const float HalfHeight = GetCapsuleHalfHeight();
	if (HalfHeight <= 0.0f) return false;   // capsule 아니면 floor 의미 없음

	// capsule 중심에서 down — bottom 까지 HalfHeight + 약간의 probe.
	const FVector  Start = Updated->GetWorldLocation();
	const FVector  Dir(0.0f, 0.0f, -1.0f);
	const float    MaxDist = HalfHeight + FloorProbeDistance;

	// 바닥은 WorldStatic ObjectType 만 후보로 본다. 채널 raycast (응답=Block) 시맨틱으로 가면
	// 다이내믹/폰도 기본 응답이 Block 이라 바닥으로 잘못 잡힌다. ObjectType 마스크는
	// shape의 ObjectType 자체를 검사하므로 다이내믹 박스 위 / 다른 폰 머리 위에서도 바닥으로
	// 인식되지 않는다.
	return World->PhysicsRaycastByObjectTypes(Start, Dir, MaxDist, OutHit,
		ObjectTypeBit(ECollisionChannel::WorldStatic), Owner);
}

float UCharacterMovementComponent::GetCapsuleHalfHeight() const
{
	// UpdatedComponent 가 capsule 이라야 의미 있음 — 다른 shape 면 0.
	if (UCapsuleComponent* Cap = Cast<UCapsuleComponent>(GetUpdatedComponent()))
	{
		return Cap->GetScaledCapsuleHalfHeight();
	}
	return 0.0f;
}

void UCharacterMovementComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << MaxWalkSpeed;
	Ar << MaxAcceleration;
	Ar << BrakingFriction;
	Ar << Gravity;
	Ar << FloorProbeDistance;
	Ar << JumpZVelocity;
	Ar << bOrientRotationToMovement;
	Ar << RotationYawRate;
}
