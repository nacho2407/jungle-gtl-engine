#include "CharacterMovementComponent.h"

#include "Component/CapsuleComponent.h"
#include "Component/SceneComponent.h"
#include "Core/PropertyTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Physics/IPhysicsScene.h"
#include "Serialization/Archive.h"

#include <algorithm>

IMPLEMENT_CLASS(UCharacterMovementComponent, UMovementComponent)

void UCharacterMovementComponent::AddInputVector(const FVector& WorldDirection, float ScaleValue)
{
	AccumulatedInput = AccumulatedInput + WorldDirection * ScaleValue;
}

void UCharacterMovementComponent::ConsumeInputVector(FVector& Out)
{
	Out = AccumulatedInput;
	AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
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

	FVector Input;
	ConsumeInputVector(Input);
	Input.Z = 0.0f;   // XY 평면만 — Z 는 mode 가 결정.

	// 1) Input 처리 — XY velocity 갱신 (양 mode 공통).
	ApplyInputToVelocity(Input, DeltaTime);

	// 2) Mode 별 Z 처리 + 위치 적용.
	if (MovementMode == EMovementMode::Walking)
	{
		TickWalking(DeltaTime);
	}
	else
	{
		TickFalling(DeltaTime);
	}
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

void UCharacterMovementComponent::TickWalking(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Jump 의도가 있으면 — Velocity.Z 박고 즉시 Falling 으로 전환. 이 frame 의 XY 는 그대로 진행.
	if (bWantsJump)
	{
		bWantsJump = false;
		Velocity.Z = JumpZVelocity;
		SetMovementMode(EMovementMode::Falling);
		// XY 이동은 Falling 분기로 위임 — 한 frame 안 mode 전환이라 즉시 falling tick.
		TickFalling(DeltaTime);
		return;
	}

	// Walking 중 Z velocity 는 0 — floor stick 으로만 Z 결정.
	Velocity.Z = 0.0f;

	// XY 먼저 이동.
	const FVector XYOffset(Velocity.X * DeltaTime, Velocity.Y * DeltaTime, 0.0f);
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

void UCharacterMovementComponent::TickFalling(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Gravity — Z 만. (양수 Gravity → -Z 가속)
	Velocity.Z -= Gravity * DeltaTime;

	const FVector Offset = Velocity * DeltaTime;
	Updated->SetWorldLocation(Updated->GetWorldLocation() + Offset);

	// 새 위치에서 floor 체크.
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

	return World->PhysicsRaycast(Start, Dir, MaxDist, OutHit,
		ECollisionChannel::WorldStatic, Owner);
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

void UCharacterMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);

	const char* Category = "CharacterMovement";

	FPropertyDescriptor SpeedProp;
	SpeedProp.Name     = "Max Walk Speed";
	SpeedProp.Type     = EPropertyType::Float;
	SpeedProp.Category = Category;
	SpeedProp.ValuePtr = &MaxWalkSpeed;
	SpeedProp.Min      = 0.0f;
	SpeedProp.Max      = 100.0f;
	SpeedProp.Speed    = 0.1f;
	OutProps.push_back(SpeedProp);

	FPropertyDescriptor AccelProp;
	AccelProp.Name     = "Max Acceleration";
	AccelProp.Type     = EPropertyType::Float;
	AccelProp.Category = Category;
	AccelProp.ValuePtr = &MaxAcceleration;
	AccelProp.Min      = 0.0f;
	AccelProp.Max      = 200.0f;
	AccelProp.Speed    = 0.5f;
	OutProps.push_back(AccelProp);

	FPropertyDescriptor BrakeProp;
	BrakeProp.Name     = "Braking Friction";
	BrakeProp.Type     = EPropertyType::Float;
	BrakeProp.Category = Category;
	BrakeProp.ValuePtr = &BrakingFriction;
	BrakeProp.Min      = 0.0f;
	BrakeProp.Max      = 100.0f;
	BrakeProp.Speed    = 0.1f;
	OutProps.push_back(BrakeProp);

	FPropertyDescriptor GravityProp;
	GravityProp.Name     = "Gravity";
	GravityProp.Type     = EPropertyType::Float;
	GravityProp.Category = Category;
	GravityProp.ValuePtr = &Gravity;
	GravityProp.Min      = 0.0f;
	GravityProp.Max      = 100.0f;
	GravityProp.Speed    = 0.1f;
	OutProps.push_back(GravityProp);

	FPropertyDescriptor ProbeProp;
	ProbeProp.Name     = "Floor Probe Distance";
	ProbeProp.Type     = EPropertyType::Float;
	ProbeProp.Category = Category;
	ProbeProp.ValuePtr = &FloorProbeDistance;
	ProbeProp.Min      = 0.0f;
	ProbeProp.Max      = 5.0f;
	ProbeProp.Speed    = 0.01f;
	OutProps.push_back(ProbeProp);

	FPropertyDescriptor JumpProp;
	JumpProp.Name     = "Jump Z Velocity";
	JumpProp.Type     = EPropertyType::Float;
	JumpProp.Category = Category;
	JumpProp.ValuePtr = &JumpZVelocity;
	JumpProp.Min      = 0.0f;
	JumpProp.Max      = 50.0f;
	JumpProp.Speed    = 0.1f;
	OutProps.push_back(JumpProp);
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
}
