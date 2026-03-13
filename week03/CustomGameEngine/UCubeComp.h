#pragma once
#include "UPrimitiveComponent.h"

class UCubeComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UCubeComp, UPrimitiveComponent)
public:
	UCubeComp();
	virtual ~UCubeComp() override;
	virtual URenderObject* Render() override;
	virtual void Update(float DeltaTime) override;
};
