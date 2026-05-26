// Copyright Epic Games, Inc. All Rights Reserved.
#include "BoxComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Math/Quat.h"

#include <cstring>
#include <cmath>

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	BoxExtent = InExtent;
	LocalExtents = BoxExtent;
	MarkWorldBoundsDirty();
	MarkRenderTransformDirty();
	// PhysX shape 도 새 extent 로 재구성 — 런타임에 SetBoxExtent 했을 때 콜리전/overlap
	// 영역이 안 바뀌던 버그 수정. bComponentHasBegunPlay 가 false 면 NotifyPhysicsBodyDirty
	// 가 no-op 라 BeginPlay 전 setter 호출은 영향 없음.
	NotifyPhysicsBodyDirty();
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
	FVector Scale = GetWorldScale();
	return FVector(BoxExtent.X * Scale.X, BoxExtent.Y * Scale.Y, BoxExtent.Z * Scale.Z);
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "BoxExtent") == 0 || strcmp(PropertyName, "Box Extent") == 0)
	{
		SetBoxExtent(BoxExtent);
	}
}
