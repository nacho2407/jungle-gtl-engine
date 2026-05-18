#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/SkeletalMeshActor.generated.h"
class USkeletalMeshComponent;

UCLASS()
class ASkeletalMeshActor : public AActor
{
public:
	GENERATED_BODY()
	ASkeletalMeshActor() = default;

	void BeginPlay() override;

	void InitDefaultComponents(const FString& SkeletalMeshFileName = "Data/Samba Dancing (10).fbx");

private:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};