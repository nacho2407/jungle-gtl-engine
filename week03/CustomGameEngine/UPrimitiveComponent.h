#pragma once
#include "USceneComponent.h"

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent() override;
};
