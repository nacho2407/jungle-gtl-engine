#pragma once

#include "GameFramework/AActor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
	DECLARE_CLASS(ASkeletalMeshActor, AActor)

	ASkeletalMeshActor() = default;

	void BeginPlay() override;

	void InitDefaultComponents(const FString& SkeletalMeshFileName = "Data/Samba Dancing (10).fbx");

private:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};