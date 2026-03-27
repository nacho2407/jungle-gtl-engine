#pragma once

#include "Component/PrimitiveComponent.h"

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

	UMeshComponent() = default;
	~UMeshComponent() override = default;
};
