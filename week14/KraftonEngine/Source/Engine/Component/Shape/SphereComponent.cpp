// Copyright Epic Games, Inc. All Rights Reserved.
#include "SphereComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Math/MathUtils.h"

#include <cstring>
#include <cmath>
#include <algorithm>

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = InRadius;
	LocalExtents = FVector(SphereRadius, SphereRadius, SphereRadius);
	MarkWorldBoundsDirty();
}

float USphereComponent::GetScaledSphereRadius() const
{
	FVector Scale = GetWorldScale();
	return SphereRadius * std::max({ Scale.X, Scale.Y, Scale.Z });
}

void USphereComponent::UpdateWorldAABB() const
{
	FVector Center = GetWorldLocation();
	float R = GetScaledSphereRadius();
	WorldAABBMinLocation = Center - FVector(R, R, R);
	WorldAABBMaxLocation = Center + FVector(R, R, R);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "SphereRadius") == 0 || strcmp(PropertyName, "Sphere Radius") == 0)
	{
		SetSphereRadius(SphereRadius);
	}
}
