#pragma once

#include "CoreTypes.h"

#include "Component/EGizmoAxis.h"

class UPrimitiveComponent;

enum class EHitProxyType
{
	None,
	Primitive,
	GizmoAxis // @@@ AActor* HitActor = nullptr; 이거 전에 이미 Gizmo인지 Actor인지 검사하고 들어오는데 GizmoAxis를 나눌 필요가 있나?
			  // *** 이거 작성할 당시에는 Hit Proxy로 Gizmo도 처리하는 줄 알았음...
};

struct FHitProxy
{
	EHitProxyType Type = EHitProxyType::None;
	UPrimitiveComponent* Primitive = nullptr;
	EGizmoAxis Axis = EGizmoAxis::None;
};
