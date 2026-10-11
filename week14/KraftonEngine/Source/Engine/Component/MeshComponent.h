#pragma once

#include "Component/PrimitiveComponent.h"


#include "Source/Engine/Component/MeshComponent.generated.h"

UCLASS()
class UMeshComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UMeshComponent() = default;
	~UMeshComponent() override = default;
};
