#pragma once

#include "GameFramework/AActor.h"

class UCapsuleComponent;

class ACapsuleActor : public AActor
{
public:
	DECLARE_CLASS(ACapsuleActor, AActor)

	ACapsuleActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

private:
	UCapsuleComponent* CapsuleComponent = nullptr;
};
