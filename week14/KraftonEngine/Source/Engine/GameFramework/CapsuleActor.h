#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/CapsuleActor.generated.h"
class UCapsuleComponent;

UCLASS()
class ACapsuleActor : public AActor
{
public:
	GENERATED_BODY()
	ACapsuleActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

private:
	UCapsuleComponent* CapsuleComponent = nullptr;
};
