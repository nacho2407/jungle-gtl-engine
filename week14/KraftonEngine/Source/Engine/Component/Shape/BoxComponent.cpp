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
