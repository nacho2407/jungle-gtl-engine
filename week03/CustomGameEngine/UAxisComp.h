#pragma once
#include "UPrimitiveComponent.h"

class UAxisComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UAxisComp, UPrimitiveComponent)
public:
	UAxisComp();
	virtual ~UAxisComp() override;
	virtual URenderObject* Render() override;
	virtual void Update(float DeltaTime) override;
};
