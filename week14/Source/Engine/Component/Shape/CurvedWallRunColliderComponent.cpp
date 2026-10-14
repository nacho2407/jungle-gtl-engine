#include "CurvedWallRunColliderComponent.h"

#include "Math/MathUtils.h"
#include "Math/Matrix.h"

#include <algorithm>
#include <cmath>
#include <cstring>

UCurvedWallRunColliderComponent::UCurvedWallRunColliderComponent()
{
	SanitizeShapeValues();
}

void UCurvedWallRunColliderComponent::SetCurveShape(
	float InRadius,
	float InHalfHeight,
	float InThickness,
	float InArcAngleDegrees,
	int32 InSegmentCount)
{
	Radius = InRadius;
	HalfHeight = InHalfHeight;
	Thickness = InThickness;
	ArcAngleDegrees = InArcAngleDegrees;
	SegmentCount = InSegmentCount;

	SanitizeShapeValues();
	MarkWorldBoundsDirty();
	MarkRenderStateDirty();
}

float UCurvedWallRunColliderComponent::GetScaledRadius() const
{
	const FVector Scale = GetWorldScale();
	const float HorizontalScale = (std::max)(std::abs(Scale.X), std::abs(Scale.Y));
	return Radius * HorizontalScale;
}

float UCurvedWallRunColliderComponent::GetScaledHalfHeight() const
{
	const FVector Scale = GetWorldScale();
	return HalfHeight * std::abs(Scale.Z);
}

float UCurvedWallRunColliderComponent::GetScaledThickness() const
{
	const FVector Scale = GetWorldScale();
	const float HorizontalScale = (std::max)(std::abs(Scale.X), std::abs(Scale.Y));
	return Thickness * HorizontalScale;
}

float UCurvedWallRunColliderComponent::GetArcAngleRadians() const
{
	return ArcAngleDegrees * FMath::DegToRad;
}

int32 UCurvedWallRunColliderComponent::GetSafeSegmentCount() const
{
	return (std::max)(1, (std::min)(SegmentCount, 128));
}

void UCurvedWallRunColliderComponent::UpdateWorldAABB() const
{
	// 원호 전체를 직접 샘플링해서 회전된 곡선 collider의 월드 AABB를 계산.
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const float LocalHalfThickness = Thickness * 0.5f;
	const float LocalHalfHeight = HalfHeight;
	const float ArcRadians = GetArcAngleRadians();
	const int32 Samples = (std::max)(GetSafeSegmentCount(), 4);

	bool bInitialized = false;
	FVector MinLocation = FVector::ZeroVector;
	FVector MaxLocation = FVector::ZeroVector;

	auto IncludePoint = [&](const FVector& LocalPoint)
	{
		const FVector WorldPoint = WorldMatrix.TransformPositionWithW(LocalPoint);
		if (!bInitialized)
		{
			MinLocation = WorldPoint;
			MaxLocation = WorldPoint;
			bInitialized = true;
			return;
		}

		MinLocation.X = (std::min)(MinLocation.X, WorldPoint.X);
		MinLocation.Y = (std::min)(MinLocation.Y, WorldPoint.Y);
		MinLocation.Z = (std::min)(MinLocation.Z, WorldPoint.Z);
		MaxLocation.X = (std::max)(MaxLocation.X, WorldPoint.X);
		MaxLocation.Y = (std::max)(MaxLocation.Y, WorldPoint.Y);
		MaxLocation.Z = (std::max)(MaxLocation.Z, WorldPoint.Z);
	};

	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Samples);
		const float Angle = -ArcRadians * 0.5f + ArcRadians * Alpha;
		const float CosAngle = std::cos(Angle);
		const float SinAngle = std::sin(Angle);

		for (float RadiusOffset : { -LocalHalfThickness, LocalHalfThickness })
		{
			const float SampleRadius = (std::max)(0.0f, Radius + RadiusOffset);
			IncludePoint(FVector(CosAngle * SampleRadius, SinAngle * SampleRadius, -LocalHalfHeight));
			IncludePoint(FVector(CosAngle * SampleRadius, SinAngle * SampleRadius, LocalHalfHeight));
		}
	}

	WorldAABBMinLocation = MinLocation;
	WorldAABBMaxLocation = MaxLocation;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UCurvedWallRunColliderComponent::PostEditProperty(const char* PropertyName)
{
	UShapeComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Radius") == 0
		|| strcmp(PropertyName, "HalfHeight") == 0
		|| strcmp(PropertyName, "Half Height") == 0
		|| strcmp(PropertyName, "Thickness") == 0
		|| strcmp(PropertyName, "ArcAngleDegrees") == 0
		|| strcmp(PropertyName, "Arc Angle Degrees") == 0
		|| strcmp(PropertyName, "SegmentCount") == 0
		|| strcmp(PropertyName, "Segment Count") == 0
		|| strcmp(PropertyName, "bEnablePhysicalPanels") == 0
		|| strcmp(PropertyName, "Enable Physical Panels") == 0)
	{
		SanitizeShapeValues();
		MarkWorldBoundsDirty();
		MarkRenderStateDirty();
	}
}

void UCurvedWallRunColliderComponent::SanitizeShapeValues()
{
	// 에디터 입력과 scene load 값을 모두 같은 기준으로 보정.
	Radius = (std::max)(0.05f, Radius);
	HalfHeight = (std::max)(0.05f, HalfHeight);
	Thickness = (std::max)(0.02f, Thickness);
	ArcAngleDegrees = FMath::Clamp(std::abs(ArcAngleDegrees), 1.0f, 360.0f);
	SegmentCount = GetSafeSegmentCount();

	const float LocalExtent = Radius + Thickness * 0.5f;
	LocalExtents = FVector(LocalExtent, LocalExtent, HalfHeight);
}
