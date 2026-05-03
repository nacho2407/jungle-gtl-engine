#pragma once

#include "Component/SceneComponent.h"

class UDirtComponent;

class UCarDirtComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCarDirtComponent, USceneComponent)

	UCarDirtComponent() = default;
	~UCarDirtComponent() override = default;

	void BeginPlay() override;

private:
	void EnsureDirtComponents();
	int32 CountDirtChildren() const;
	UDirtComponent* CreateDirtChild(int32 Index);
};
