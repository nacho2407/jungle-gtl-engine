// Copyright Epic Games, Inc. All Rights Reserved.
#include "CapsuleComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Math/MathUtils.h"

#include <cstring>
#include <cmath>
#include <algorithm>

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight)
{
	CapsuleRadius = InRadius;
	CapsuleHalfHeight = (std::max)(InHalfHeight, InRadius);
	LocalExtents = FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
	MarkWorldBoundsDirty();
	MarkRenderTransformDirty();
}

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	FVector Scale = GetWorldScale();
	return CapsuleRadius * std::min(Scale.X, Scale.Y);
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	FVector Scale = GetWorldScale();
	return CapsuleHalfHeight * Scale.Z;
}

namespace
{
	void AddWireCircle(FScene& Scene, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments, const FColor& Color)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
			Scene.AddDebugLine(Prev, Next, Color);
			Prev = Next;
		}
	}

	void AddWireHalfCircle(FScene& Scene, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments, float StartAngle, const FColor& Color)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + (AxisA * cosf(StartAngle) + AxisB * sinf(StartAngle)) * Radius;

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = StartAngle + Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
			Scene.AddDebugLine(Prev, Next, Color);
			Prev = Next;
		}
	}
}

void UCapsuleComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Center = GetWorldLocation();
	const float R = GetScaledCapsuleRadius();
	const float HH = GetScaledCapsuleHalfHeight();
	const float CylinderHalf = HH - R;
	constexpr int32 Segments = 24;

	const FColor Color = GetShapeColor();
	const FVector TopCenter = Center + FVector(0.0f, 0.0f, CylinderHalf);
	const FVector BotCenter = Center - FVector(0.0f, 0.0f, CylinderHalf);

	// Top and bottom horizontal circles (XY plane)
	AddWireCircle(Scene, TopCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), R, Segments, Color);
	AddWireCircle(Scene, BotCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), R, Segments, Color);

	// 4 vertical lines connecting top and bottom circles
	Scene.AddDebugLine(TopCenter + FVector(R, 0.0f, 0.0f), BotCenter + FVector(R, 0.0f, 0.0f), Color);
	Scene.AddDebugLine(TopCenter - FVector(R, 0.0f, 0.0f), BotCenter - FVector(R, 0.0f, 0.0f), Color);
	Scene.AddDebugLine(TopCenter + FVector(0.0f, R, 0.0f), BotCenter + FVector(0.0f, R, 0.0f), Color);
	Scene.AddDebugLine(TopCenter - FVector(0.0f, R, 0.0f), BotCenter - FVector(0.0f, R, 0.0f), Color);

	constexpr int32 HalfSegments = 12;

	// Top half-circle caps (upper hemisphere)
	// XZ plane — upward half-circle
	AddWireHalfCircle(Scene, TopCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), R, HalfSegments, 0.0f, Color);
	// YZ plane — upward half-circle
	AddWireHalfCircle(Scene, TopCenter, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), R, HalfSegments, 0.0f, Color);

	// Bottom half-circle caps (lower hemisphere)
	// XZ plane — downward half-circle
	AddWireHalfCircle(Scene, BotCenter, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), R, HalfSegments, FMath::Pi, Color);
	// YZ plane — downward half-circle
	AddWireHalfCircle(Scene, BotCenter, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), R, HalfSegments, FMath::Pi, Color);
}

void UCapsuleComponent::UpdateWorldAABB() const
{
	FVector Center = GetWorldLocation();
	float R = GetScaledCapsuleRadius();
	float HH = GetScaledCapsuleHalfHeight();
	WorldAABBMinLocation = Center - FVector(R, R, HH);
	WorldAABBMaxLocation = Center + FVector(R, R, HH);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Capsule Radius", EPropertyType::Float, "Shape", &CapsuleRadius, 0.01f, 10000.0f, 1.0f });
	OutProps.push_back({ "Capsule Half Height", EPropertyType::Float, "Shape", &CapsuleHalfHeight, 0.01f, 10000.0f, 1.0f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Capsule Radius") == 0 || strcmp(PropertyName, "Capsule Half Height") == 0)
	{
		SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	}
}

void UCapsuleComponent::Serialize(FArchive& Ar)
{
	UShapeComponent::Serialize(Ar);
	Ar << CapsuleRadius;
	Ar << CapsuleHalfHeight;
}
