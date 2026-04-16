#pragma once

#include "GameFramework/AActor.h"

class UExponentialHeightFogComponent;

class AExponentialHeightFog : public AActor
{
public:
	DECLARE_CLASS(AExponentialHeightFog, AActor)
	AExponentialHeightFog() = default;

	void InitDefaultComponents();
	UExponentialHeightFogComponent* GetExponentialHeightFogComponent() const { return ExponentialHeightFogComponent; }

private:
	UExponentialHeightFogComponent* ExponentialHeightFogComponent = nullptr;
};
