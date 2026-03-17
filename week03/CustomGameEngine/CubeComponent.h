#pragma once

#include "PrimitiveComponent.h"

class UCubeComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UCubeComp, UPrimitiveComponent)
public:
	UCubeComp();
	virtual ~UCubeComp() override;
	virtual void Update(float DeltaTime) override;
};
