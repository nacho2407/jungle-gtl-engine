#pragma once

#include "Component/SceneComponent.h"

#include "Source/Game/Component/CarDirtComponent.generated.h"
class UDirtComponent;

UCLASS()
class UCarDirtComponent : public USceneComponent
{
public:
	GENERATED_BODY()
	UCarDirtComponent() = default;
	~UCarDirtComponent() override = default;

	void BeginPlay() override;

private:
	int32 CountDirtChildren() const;
};
