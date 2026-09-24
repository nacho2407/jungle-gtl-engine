#pragma once
#include "PrimitiveComponent.h"

class USphereComp : public UPrimitiveComponent
{
	DECLARE_RTTI(USphereComp, UPrimitiveComponent)
public:
	USphereComp();
	virtual void Update(float DeltaTime) override;
	virtual ~USphereComp() override;
};
