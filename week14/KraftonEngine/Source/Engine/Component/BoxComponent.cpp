// Copyright Epic Games, Inc. All Rights Reserved.
#include "BoxComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"

#include <cstring>
#include <cmath>

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	BoxExtent = InExtent;
	LocalExtents = BoxExtent;
	MarkWorldBoundsDirty();
	MarkRenderTransformDirty();
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
	FVector Scale = GetWorldScale();
	return FVector(BoxExtent.X * Scale.X, BoxExtent.Y * Scale.Y, BoxExtent.Z * Scale.Z);
}

void UBoxComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Center = GetWorldLocation();
	const FVector Ext = GetScaledBoxExtent();
	const FColor Color = GetShapeColor();

	// 8 corners
	FVector Corners[8];
	for (int32 i = 0; i < 8; ++i)
	{
		Corners[i] = Center + FVector(
			(i & 1) ? Ext.X : -Ext.X,
			(i & 2) ? Ext.Y : -Ext.Y,
			(i & 4) ? Ext.Z : -Ext.Z
		);
	}

	// 12 edges: bottom 4, top 4, vertical 4
	// Bottom (z = -Ext.Z): 0-1, 1-3, 3-2, 2-0
	Scene.AddDebugLine(Corners[0], Corners[1], Color);
	Scene.AddDebugLine(Corners[1], Corners[3], Color);
	Scene.AddDebugLine(Corners[3], Corners[2], Color);
	Scene.AddDebugLine(Corners[2], Corners[0], Color);

	// Top (z = +Ext.Z): 4-5, 5-7, 7-6, 6-4
	Scene.AddDebugLine(Corners[4], Corners[5], Color);
	Scene.AddDebugLine(Corners[5], Corners[7], Color);
	Scene.AddDebugLine(Corners[7], Corners[6], Color);
	Scene.AddDebugLine(Corners[6], Corners[4], Color);

	// Vertical: 0-4, 1-5, 2-6, 3-7
	Scene.AddDebugLine(Corners[0], Corners[4], Color);
	Scene.AddDebugLine(Corners[1], Corners[5], Color);
	Scene.AddDebugLine(Corners[2], Corners[6], Color);
	Scene.AddDebugLine(Corners[3], Corners[7], Color);
}

void UBoxComponent::UpdateWorldAABB() const
{
	FVector Center = GetWorldLocation();
	FVector ScaledExt = GetScaledBoxExtent();
	WorldAABBMinLocation = Center - ScaledExt;
	WorldAABBMaxLocation = Center + ScaledExt;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Box Extent", EPropertyType::Vec3, "Shape", &BoxExtent, 0.01f, 0.0f, 0.1f });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Box Extent") == 0)
	{
		SetBoxExtent(BoxExtent);
	}
}

void UBoxComponent::Serialize(FArchive& Ar)
{
	UShapeComponent::Serialize(Ar);
	Ar << BoxExtent;
}
