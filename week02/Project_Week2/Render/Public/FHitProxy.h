#pragma once

#include "CoreTypes.h"

#include "Component/EGizmoAxis.h"

class UPrimitiveComponent;

enum EHitProxyType
{
	None,
	Primitive,
	GizmoAxis
};

struct FHitProxy
{
	EHitProxyType Type = EHitProxyType::None;
	UPrimitiveComponent* Primitive = nullptr;
	EGizmoAxis Axis = EGizmoAxis::None;
};
