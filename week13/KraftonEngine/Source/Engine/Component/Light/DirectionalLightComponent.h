#pragma once
#include "Component/Light/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void Serialize(FArchive& Ar) override;


protected:
	FVector Direction;
};