#include "CharacterMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Core/PropertyTypes.h"
#include "Object/ObjectFactory.h"
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

void UCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;
	if (DeltaTime <= 0.0f) return;

	FVector Input;
	ConsumeInputVector(Input);
	// minimal — XY 평면만. Z 는 후속 phase (gravity/jump).
	Input.Z = 0.0f;

	const float InputLen = Input.Length();
	if (InputLen > 0.0f)
	{
		// 입력 방향으로 가속 (0 length 가드는 Length>0 분기에 포함).
		const FVector Direction = Input * (1.0f / InputLen);
		Velocity = Velocity + Direction * (MaxAcceleration * DeltaTime);
	}
	else
	{
		// 입력 없으면 평면 속도만 braking 으로 감속 (Z 는 그대로).
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
	{
		FVector V2D(Velocity.X, Velocity.Y, 0.0f);
		const float Speed2D = V2D.Length();
		if (Speed2D > MaxWalkSpeed)
		{
			const FVector Dir = V2D * (1.0f / Speed2D);
			Velocity.X = Dir.X * MaxWalkSpeed;
			Velocity.Y = Dir.Y * MaxWalkSpeed;
		}
	}

	// World offset 적용 — UpdatedComponent (Capsule root) 의 위치를 옮긴다.
	if (Velocity.Length() > 0.0f)
	{
		const FVector Offset = Velocity * DeltaTime;
		Updated->SetWorldLocation(Updated->GetWorldLocation() + Offset);
	}
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
}

void UCharacterMovementComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << MaxWalkSpeed;
	Ar << MaxAcceleration;
	Ar << BrakingFriction;
}
