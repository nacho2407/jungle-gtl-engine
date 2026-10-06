#pragma once
#include "Component/Light/LightComponent.h"


#include "Source/Engine/Component/Light/AmbientLightComponent.generated.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent
{
public:
	GENERATED_BODY()
	UAmbientLightComponent();

	virtual ELightComponentType GetLightType() const override { return ELightComponentType::Ambient; }
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
};