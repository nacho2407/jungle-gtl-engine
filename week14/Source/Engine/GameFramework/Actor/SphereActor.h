#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/SphereActor.generated.h"
class USphereComponent;

UCLASS()
class ASphereActor : public AActor
{
public:
	GENERATED_BODY()
	ASphereActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;
	void BeginPlay() override;

	USphereComponent* GetSphereComponent() const { return SphereComponent; }

private:
	USphereComponent* SphereComponent = nullptr;
};
