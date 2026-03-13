#pragma once
#include "UPrimitiveComponent.h"

class UCubeComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UCubeComp, UPrimitiveComponent)
public:
	UCubeComp();
	virtual ~UCubeComp() override;
};
