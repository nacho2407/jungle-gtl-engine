#pragma once

#include "GameFramework/AActor.h"

class UBoxComponent;

class ABoxActor : public AActor
{
public:
	DECLARE_CLASS(ABoxActor, AActor)

	ABoxActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

private:
	UBoxComponent* BoxComponent = nullptr;
};
