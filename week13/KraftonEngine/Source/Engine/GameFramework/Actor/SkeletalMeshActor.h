#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/SkeletalMeshActor.generated.h"
class USkeletalMeshComponent;

UCLASS()
class ASkeletalMeshActor : public AActor
{
public:
	GENERATED_BODY()
	ASkeletalMeshActor() = default;

	void BeginPlay() override;

	void PostDuplicate() override;

	void InitDefaultComponents(const FString& SkeletalMeshFileName = "Content/Data/Samba Dancing (10).fbx");


protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};