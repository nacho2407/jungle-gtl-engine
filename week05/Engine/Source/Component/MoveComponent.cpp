#include "MoveComponent.h"
#include "Object/Class.h"
#include "Actor/Actor.h"

IMPLEMENT_RTTI(UMoveComponent, USceneComponent)

void UMoveComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	bCanEverTick = true;
	InitialPosition = GetOwner()->GetActorLocation();
}

void UMoveComponent::Tick(float DeltaTime)
{
	ElapsedTime += DeltaTime;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	FTransform Transform = Owner->GetActorTransform();

	FVector Pos = InitialPosition;
	Pos.Z += sin(ElapsedTime * Speed) * Height;
	Transform.SetLocation(Pos);

	FQuat DeltaRot(FVector(0, 0, 1), RotateSpeed * DeltaTime);
	FQuat NewRot = Transform.GetRotation() * DeltaRot;
	Transform.SetRotation(NewRot);

	Owner->SetActorTransform(Transform);
}
