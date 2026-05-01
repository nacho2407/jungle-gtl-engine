// Copyright Epic Games, Inc. All Rights Reserved.
#include "SphereComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Math/MathUtils.h"

#include <cstring>
#include <cmath>
#include <algorithm>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = InRadius;
	LocalExtents = FVector(SphereRadius, SphereRadius, SphereRadius);
	MarkWorldBoundsDirty();
	MarkRenderTransformDirty();
}

float USphereComponent::GetScaledSphereRadius() const
{
	FVector Scale = GetWorldScale();
	return SphereRadius * std::min({ Scale.X, Scale.Y, Scale.Z });
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
}

void USphereComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Center = GetWorldLocation();
	const float Radius = GetScaledSphereRadius();
	const FColor Color = GetShapeColor();
	constexpr int32 Segments = 24;

	// XY plane circle
	AddWireCircle(Scene, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), Radius, Segments, Color);
	// XZ plane circle
	AddWireCircle(Scene, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Segments, Color);
	// YZ plane circle
	AddWireCircle(Scene, Center, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Segments, Color);
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

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Sphere Radius", EPropertyType::Float, &SphereRadius, 0.01f, 10000.0f, 1.0f });
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Sphere Radius") == 0)
	{
		SetSphereRadius(SphereRadius);
	}
}

void USphereComponent::Serialize(FArchive& Ar)
{
	UShapeComponent::Serialize(Ar);
	Ar << SphereRadius;
}
