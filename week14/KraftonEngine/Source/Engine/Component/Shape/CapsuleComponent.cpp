// Copyright Epic Games, Inc. All Rights Reserved.
#include "CapsuleComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Math/MathUtils.h"

#include <cstring>
#include <cmath>
#include <algorithm>

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight)
{
	CapsuleRadius = InRadius;
	CapsuleHalfHeight = (std::max)(InHalfHeight, InRadius);
	LocalExtents = FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
	NotifyPhysicsBodyDirty();
	MarkWorldBoundsDirty();
	MarkRenderTransformDirty();
}

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	FVector Scale = GetWorldScale();
	return CapsuleRadius * (std::max)(Scale.X, Scale.Y);
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	FVector Scale = GetWorldScale();
	return CapsuleHalfHeight * Scale.Z;
}

void UCapsuleComponent::UpdateWorldAABB() const
{
	FVector Center = GetWorldLocation();
	float R = GetScaledCapsuleRadius();
	float HH = GetScaledCapsuleHalfHeight();
	float SegmentHalfLength = (std::max)(0.0f, HH - R);
	FVector Axis = GetUpVector();
	if (Axis.Length() <= 1.0e-4f)
	{
		Axis = FVector::ZAxisVector;
	}
	Axis.Normalize();

	const FVector Extents(
		R + std::abs(Axis.X) * SegmentHalfLength,
		R + std::abs(Axis.Y) * SegmentHalfLength,
		R + std::abs(Axis.Z) * SegmentHalfLength
	);
	WorldAABBMinLocation = Center - Extents;
	WorldAABBMaxLocation = Center + Extents;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "CapsuleRadius") == 0 || strcmp(PropertyName, "CapsuleHalfHeight") == 0
		|| strcmp(PropertyName, "Capsule Radius") == 0 || strcmp(PropertyName, "Capsule Half Height") == 0)
	{
		SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	}
}
