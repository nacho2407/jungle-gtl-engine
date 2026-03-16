#pragma once

#include "UPrimitiveComponent.h"

class UPlaneComp : public UPrimitiveComponent {
	DECLARE_RTTI(UPlaneComp, UPrimitiveComponent)
public:
	UPlaneComp();
	virtual ~UPlaneComp() override;
};