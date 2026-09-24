#pragma once
#include "PrimitiveComponent.h"

class UAxisComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UAxisComp, UPrimitiveComponent)
public:
	UAxisComp();
	virtual ~UAxisComp() override;
	virtual void Update(float DeltaTime) override;

	virtual RenderObject* CreateRenderObject() override;
};

