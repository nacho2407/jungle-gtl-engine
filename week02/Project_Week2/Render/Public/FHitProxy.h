#pragma once

#include "CoreTypes.h"

#include "Component/EGizmoAxis.h"

class UPrimitiveComponent;

enum class EHitProxyType
{
	None,
	Primitive,
	GizmoAxis // @@@ AActor* HitActor = nullptr; 이거 전에 이미 Gizmo인지 Actor인지 검사하고 들어오는데 GizmoAxis를 나눌 필요가 있나?
};

struct FHitProxy
{
	EHitProxyType Type = EHitProxyType::None;
	UPrimitiveComponent* Primitive = nullptr;
	EGizmoAxis Axis = EGizmoAxis::None;
};
