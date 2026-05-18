#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/BoxActor.generated.h"
class UBoxComponent;

UCLASS()
class ABoxActor : public AActor
{
public:
	GENERATED_BODY()
	ABoxActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

private:
	UBoxComponent* BoxComponent = nullptr;
};
