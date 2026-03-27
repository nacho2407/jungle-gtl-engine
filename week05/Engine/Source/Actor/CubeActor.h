#pragma once

#include "Actor.h"

class UPrimitiveComponent;
class URandomColorComponent;

class ENGINE_API ACubeActor : public AActor
{
public:
	DECLARE_RTTI(ACubeActor, AActor)

	void PostSpawnInitialize() override;

	UPrimitiveComponent* GetPrimitiveComponent() const { return PrimitiveComponent; }
	URandomColorComponent* GetRandomColorComponent() const { return RandomColorComponent; }

	void SetUseRandomColor(bool bEnable) { bUseRandomColor = bEnable; }
	bool GetUseRandomColor() const { return bUseRandomColor; }

private:
	UPrimitiveComponent* PrimitiveComponent = nullptr;
	URandomColorComponent* RandomColorComponent = nullptr;
	bool bUseRandomColor = true;
};
