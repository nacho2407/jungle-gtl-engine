#pragma once
#include "PrimitiveComponent.h"

class ENGINE_API USkyComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(USkyComponent, UPrimitiveComponent)

	void Initialize();
	FBoxSphereBounds GetWorldBounds() const override;
};