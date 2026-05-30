#include "AggregateGeom.h"

#include "Math/MathUtils.h"

#include <algorithm>

namespace
{
float SelectMinScale(const FVector& Scale)
{
	float Result = Scale.X;
	float AbsResult = FMath::Abs(Scale.X);

	const float AbsY = FMath::Abs(Scale.Y);
	if (AbsY < AbsResult)
	{
		AbsResult = AbsY;
		Result = Scale.Y;
	}

	const float AbsZ = FMath::Abs(Scale.Z);
	if (AbsZ < AbsResult)
	{
		Result = Scale.Z;
	}

	return Result;
}

FBoundingBox TransformBoundingBox(const FBoundingBox& Bounds, const FTransform& Transform)
{
	if (!Bounds.IsValid())
	{
		return FBoundingBox();
	}

	FVector Corners[8];
	Bounds.GetCorners(Corners);

	FBoundingBox Result;
	for (const FVector& Corner : Corners)
	{
		Result.Expand(Transform.TransformPosition(Corner));
	}

	return Result;
}

void AccumulateBoundingBox(FBoundingBox& Target, const FBoundingBox& Source)
{
	if (!Source.IsValid())
	{
		return;
	}

	Target.Expand(Source.Min);
	Target.Expand(Source.Max);
}

FVector ClosestPointOnBox(const FVector& Point, const FVector& HalfExtent)
{
	return FVector(
		FMath::Clamp(Point.X, -HalfExtent.X, HalfExtent.X),
		FMath::Clamp(Point.Y, -HalfExtent.Y, HalfExtent.Y),
		FMath::Clamp(Point.Z, -HalfExtent.Z, HalfExtent.Z));
}

FVector ClosestPointOnAABB(const FVector& Point, const FBoundingBox& Bounds)
{
	return FVector(
		FMath::Clamp(Point.X, Bounds.Min.X, Bounds.Max.X),
		FMath::Clamp(Point.Y, Bounds.Min.Y, Bounds.Max.Y),
		FMath::Clamp(Point.Z, Bounds.Min.Z, Bounds.Max.Z));
}

float SignedVolumeOfTriangle(const FVector& A, const FVector& B, const FVector& C)
{
	return A.Dot(B.Cross(C)) / 6.0f;
}

float GetBoxVolume(const FVector& Extent)
{
	return FMath::Abs(Extent.X * Extent.Y * Extent.Z);
}
}

EAggCollisionShape FKSphylElem::StaticShapeType = EAggCollisionShape::Sphyl;

FKSphylElem::~FKSphylElem() = default;

FBoundingBox FKSphereElem::CalcAABB(const FTransform& BoneTM, float Scale) const
{
	const float RadiusScale = FMath::Abs(Scale);
	const FVector WorldCenter = BoneTM.TransformPosition(Center * Scale);
	const FVector Extent(Radius * RadiusScale, Radius * RadiusScale, Radius * RadiusScale);
	return FBoundingBox(WorldCenter - Extent, WorldCenter + Extent);
}

void FKSphereElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	float DeltaRadius = DeltaSize.X;
	if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
	{
		DeltaRadius = DeltaSize.Y;
	}
	if (FMath::Abs(DeltaSize.Z) > FMath::Abs(DeltaRadius))
	{
		DeltaRadius = DeltaSize.Z;
	}

	Radius = std::max(Radius + DeltaRadius, MinSize);
}

FKSphereElem FKSphereElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	const FVector CombinedScale = Scale3D * RelativeTM.Scale;
	const float RadiusScale = CombinedScale.GetAbsMin();

	FKSphereElem ScaledSphere = *this;
	ScaledSphere.Radius *= RadiusScale;
	ScaledSphere.Center = RelativeTM.TransformPosition(Center) * Scale3D;
	return ScaledSphere;
}

float FKSphereElem::GetScaledVolume(const FVector& Scale3D) const
{
	const float RadiusScale = Scale3D.GetAbsMin();
	const float ScaledRadius = Radius * RadiusScale;
	return (4.0f / 3.0f) * FMath::Pi * ScaledRadius * ScaledRadius * ScaledRadius;
}

float FKSphereElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
{
	const FKSphereElem ScaledSphere = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FVector WorldCenter = BodyToWorldTM.TransformPositionNoScale(ScaledSphere.Center);
	const float DistToEdge = (WorldCenter - WorldPosition).Length() - ScaledSphere.Radius;
	return DistToEdge > FMath::KINDA_SMALL_NUMBER ? DistToEdge : 0.0f;
}

float FKSphereElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FKSphereElem ScaledSphere = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FVector WorldCenter = BodyToWorldTM.TransformPositionNoScale(ScaledSphere.Center);
	const FVector CenterToPoint = WorldPosition - WorldCenter;
	const float DistToCenter = CenterToPoint.Length();
	const float DistToEdge = std::max(DistToCenter - ScaledSphere.Radius, 0.0f);

	Normal = DistToCenter > FMath::KINDA_SMALL_NUMBER ? CenterToPoint / DistToCenter : FVector::ZeroVector;
	ClosestWorldPosition = WorldPosition - Normal * DistToEdge;

	return DistToEdge;
}

FBoundingBox FKBoxElem::CalcAABB(const FTransform& BoneTM, float Scale) const
{
	const FVector HalfExtent(0.5f * X * Scale, 0.5f * Y * Scale, 0.5f * Z * Scale);
	const FBoundingBox LocalBox(-HalfExtent, HalfExtent);
	const FTransform BoxTM = FTransform(Center * Scale, Rotation) * BoneTM;
	return TransformBoundingBox(LocalBox, BoxTM);
}

void FKBoxElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	X = std::max(X + 2.0f * DeltaSize.X, MinSize);
	Y = std::max(Y + 2.0f * DeltaSize.Y, MinSize);
	Z = std::max(Z + 2.0f * DeltaSize.Z, MinSize);
}

FKBoxElem FKBoxElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	const FVector CombinedScale = (Scale3D * RelativeTM.Scale).GetAbs();

	FKBoxElem ScaledBox = *this;
	ScaledBox.X *= CombinedScale.X;
	ScaledBox.Y *= CombinedScale.Y;
	ScaledBox.Z *= CombinedScale.Z;
	ScaledBox.SetTransform(GetTransform() * RelativeTM);
	ScaledBox.Center = ScaledBox.Center * Scale3D;
	return ScaledBox;
}

float FKBoxElem::GetScaledVolume(const FVector& Scale3D) const
{
	const FVector ScaleAbs = Scale3D.GetAbs();
	return GetBoxVolume(FVector(X * ScaleAbs.X, Y * ScaleAbs.Y, Z * ScaleAbs.Z));
}

float FKBoxElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
{
	const FKBoxElem ScaledBox = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FTransform LocalToWorldTM = GetTransform() * BodyToWorldTM;
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	const FVector LocalPositionAbs = LocalPosition.GetAbs();
	const FVector HalfExtent(ScaledBox.X * 0.5f, ScaledBox.Y * 0.5f, ScaledBox.Z * 0.5f);
	const FVector Delta = LocalPositionAbs - HalfExtent;
	const FVector Errors(std::max(Delta.X, 0.0f), std::max(Delta.Y, 0.0f), std::max(Delta.Z, 0.0f));
	const float Error = Errors.Length();

	return Error > FMath::KINDA_SMALL_NUMBER ? Error : 0.0f;
}

float FKBoxElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FKBoxElem ScaledBox = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FTransform LocalToWorldTM = GetTransform() * BodyToWorldTM;
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	const FVector HalfExtent(ScaledBox.X * 0.5f, ScaledBox.Y * 0.5f, ScaledBox.Z * 0.5f);
	const FVector ClosestLocalPosition = ClosestPointOnBox(LocalPosition, HalfExtent);
	const FVector LocalDelta = LocalPosition - ClosestLocalPosition;
	const float Error = LocalDelta.Length();

	ClosestWorldPosition = LocalToWorldTM.TransformPositionNoScale(ClosestLocalPosition);
	Normal = Error > FMath::KINDA_SMALL_NUMBER
		? LocalToWorldTM.TransformVectorNoScale(LocalDelta / Error)
		: FVector::ZeroVector;

	return Error > FMath::KINDA_SMALL_NUMBER ? Error : 0.0f;
}

FBoundingBox FKSphylElem::CalcAABB(const FTransform& BoneTM, float Scale) const
{
	const FTransform SphylTM = FTransform(Center * Scale, Rotation) * BoneTM;
	const FVector Axis = SphylTM.TransformVectorNoScale(FVector::UpVector);
	const FVector AbsAxis = Axis.GetAbs();
	const FVector AbsDist = AbsAxis * (Scale * Length * 0.5f);
	const FVector RadiusExtent(Scale * Radius, Scale * Radius, Scale * Radius);
	const FVector SphylCenter = BoneTM.TransformPosition(Center * Scale);
	return FBoundingBox(SphylCenter - AbsDist - RadiusExtent, SphylCenter + AbsDist + RadiusExtent);
}

void FKSphylElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	float DeltaRadius = DeltaSize.X;
	if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
	{
		DeltaRadius = DeltaSize.Y;
	}

	const float OldRadius = Radius;
	Radius = std::max(Radius + DeltaRadius, MinSize);
	Length = std::max(Length + DeltaSize.Z + OldRadius - Radius, 0.0f);
}

FKSphylElem FKSphylElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	const FVector CombinedScale = (Scale3D * RelativeTM.Scale).GetAbs();

	FKSphylElem ScaledSphyl = *this;
	ScaledSphyl.Radius = GetScaledRadius(CombinedScale);
	ScaledSphyl.Length = GetScaledCylinderLength(CombinedScale);
	ScaledSphyl.Center = RelativeTM.TransformPosition(Center) * Scale3D;
	ScaledSphyl.Rotation = (GetTransform() * RelativeTM).GetRotator();
	return ScaledSphyl;
}

float FKSphylElem::GetScaledRadius(const FVector& Scale3D) const
{
	const FVector ScaleAbs = Scale3D.GetAbs();
	const float RadiusScale = std::max(ScaleAbs.X, ScaleAbs.Y);
	return FMath::Clamp(Radius * RadiusScale, 0.1f, GetScaledHalfLength(ScaleAbs));
}

float FKSphylElem::GetScaledCylinderLength(const FVector& Scale3D) const
{
	return std::max(0.1f, (GetScaledHalfLength(Scale3D) - GetScaledRadius(Scale3D)) * 2.0f);
}

float FKSphylElem::GetScaledHalfLength(const FVector& Scale3D) const
{
	return std::max((Length + Radius * 2.0f) * FMath::Abs(Scale3D.Z) * 0.5f, 0.1f);
}

float FKSphylElem::GetScaledVolume(const FVector& Scale3D) const
{
	const float ScaledRadius = GetScaledRadius(Scale3D);
	const float ScaledLength = GetScaledCylinderLength(Scale3D);
	const float CylinderVolume = FMath::Pi * ScaledRadius * ScaledRadius * ScaledLength;
	const float SphereVolume = (4.0f / 3.0f) * FMath::Pi * ScaledRadius * ScaledRadius * ScaledRadius;
	return CylinderVolume + SphereVolume;
}

float FKSphylElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
{
	const FKSphylElem ScaledSphyl = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FTransform LocalToWorldTM = GetTransform() * BodyToWorldTM;
	const FVector LocalPositionAbs = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition).GetAbs();
	const FVector Target(LocalPositionAbs.X, LocalPositionAbs.Y, std::max(LocalPositionAbs.Z - ScaledSphyl.Length * 0.5f, 0.0f));
	const float Error = std::max(Target.Length() - ScaledSphyl.Radius, 0.0f);

	return Error > FMath::KINDA_SMALL_NUMBER ? Error : 0.0f;
}

float FKSphylElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FKSphylElem ScaledSphyl = GetFinalScaled(BodyToWorldTM.Scale, FTransform());
	const FTransform LocalToWorldTM = GetTransform() * BodyToWorldTM;
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	const float HalfLength = 0.5f * ScaledSphyl.Length;
	const float TargetZ = FMath::Clamp(LocalPosition.Z, -HalfLength, HalfLength);
	const FVector WorldSphere = LocalToWorldTM.TransformPositionNoScale(FVector(0.0f, 0.0f, TargetZ));
	const FVector SphereToPoint = WorldPosition - WorldSphere;
	const float DistToCenter = SphereToPoint.Length();
	const float DistToEdge = std::max(DistToCenter - ScaledSphyl.Radius, 0.0f);

	Normal = DistToCenter > FMath::KINDA_SMALL_NUMBER ? SphereToPoint / DistToCenter : FVector::ZeroVector;
	ClosestWorldPosition = WorldPosition - Normal * DistToEdge;
	return DistToCenter > FMath::KINDA_SMALL_NUMBER ? DistToEdge : 0.0f;
}

void FKConvexElem::Reset()
{
	VertexData.clear();
	IndexData.clear();
	ElemBox = FBoundingBox();
}

void FKConvexElem::UpdateElemBox()
{
	ElemBox = FBoundingBox();
	for (const FVector& Vertex : VertexData)
	{
		ElemBox.Expand(Vertex);
	}
}

FBoundingBox FKConvexElem::CalcAABB(const FTransform& BoneTM, const FVector& Scale3D) const
{
	const FTransform ScaleTM(FVector::ZeroVector, FQuat::Identity, Scale3D);
	const FTransform LocalToWorld = GetTransform() * ScaleTM * BoneTM;
	return TransformBoundingBox(ElemBox, LocalToWorld);
}

void FKConvexElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	FTransform ScaledTransform = GetTransform();
	ScaledTransform.Scale.X = std::max(ScaledTransform.Scale.X + DeltaSize.X, MinSize);
	ScaledTransform.Scale.Y = std::max(ScaledTransform.Scale.Y + DeltaSize.Y, MinSize);
	ScaledTransform.Scale.Z = std::max(ScaledTransform.Scale.Z + DeltaSize.Z, MinSize);
	SetTransform(ScaledTransform);
}

float FKConvexElem::GetScaledVolume(const FVector& Scale3D) const
{
	if (IndexData.size() >= 3)
	{
		float Volume = 0.0f;
		for (std::size_t Index = 0; Index + 2 < IndexData.size(); Index += 3)
		{
			const int32 I0 = IndexData[Index];
			const int32 I1 = IndexData[Index + 1];
			const int32 I2 = IndexData[Index + 2];
			if (I0 >= 0 && I1 >= 0 && I2 >= 0
				&& static_cast<std::size_t>(I0) < VertexData.size()
				&& static_cast<std::size_t>(I1) < VertexData.size()
				&& static_cast<std::size_t>(I2) < VertexData.size())
			{
				Volume += SignedVolumeOfTriangle(VertexData[I0] * Scale3D, VertexData[I1] * Scale3D, VertexData[I2] * Scale3D);
			}
		}
		return FMath::Abs(Volume);
	}

	const FVector Extent = ElemBox.GetExtent() * Scale3D;
	return FMath::Abs(Extent.X * 2.0f * Extent.Y * 2.0f * Extent.Z * 2.0f);
}

float FKConvexElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const
{
	const FBoundingBox WorldBox = CalcAABB(BodyToWorldTM, BodyToWorldTM.Scale);
	if (!WorldBox.IsValid())
	{
		return 0.0f;
	}

	return (WorldPosition - ClosestPointOnAABB(WorldPosition, WorldBox)).Length();
}

float FKConvexElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FBoundingBox WorldBox = CalcAABB(BodyToWorldTM, BodyToWorldTM.Scale);
	if (!WorldBox.IsValid())
	{
		ClosestWorldPosition = WorldPosition;
		Normal = FVector::ZeroVector;
		return 0.0f;
	}

	ClosestWorldPosition = ClosestPointOnAABB(WorldPosition, WorldBox);
	const FVector Delta = WorldPosition - ClosestWorldPosition;
	const float Distance = Delta.Length();
	Normal = Distance > FMath::KINDA_SMALL_NUMBER ? Delta / Distance : FVector::ZeroVector;
	return Distance > FMath::KINDA_SMALL_NUMBER ? Distance : 0.0f;
}

FBoundingBox FKAggregateGeom::CalcAABB(const FTransform& Transform) const
{
	const float ScaleFactor = SelectMinScale(Transform.Scale);
	FTransform BoneTM = Transform;
	BoneTM.Scale = FVector::OneVector;

	FBoundingBox Box;
	for (const FKSphereElem& SphereElem : SphereElems)
	{
		AccumulateBoundingBox(Box, SphereElem.CalcAABB(BoneTM, ScaleFactor));
	}
	for (const FKBoxElem& BoxElem : BoxElems)
	{
		AccumulateBoundingBox(Box, BoxElem.CalcAABB(BoneTM, ScaleFactor));
	}
	for (const FKSphylElem& SphylElem : SphylElems)
	{
		AccumulateBoundingBox(Box, SphylElem.CalcAABB(BoneTM, ScaleFactor));
	}
	for (const FKConvexElem& ConvexElem : ConvexElems)
	{
		AccumulateBoundingBox(Box, ConvexElem.CalcAABB(BoneTM, Transform.Scale));
	}

	return Box;
}

float FKAggregateGeom::GetScaledVolume(const FVector& Scale3D) const
{
	float Volume = 0.0f;

	for (const FKSphereElem& SphereElem : SphereElems)
	{
		if (SphereElem.GetContributeToMass())
		{
			Volume += SphereElem.GetScaledVolume(Scale3D);
		}
	}
	for (const FKBoxElem& BoxElem : BoxElems)
	{
		if (BoxElem.GetContributeToMass())
		{
			Volume += BoxElem.GetScaledVolume(Scale3D);
		}
	}
	for (const FKSphylElem& SphylElem : SphylElems)
	{
		if (SphylElem.GetContributeToMass())
		{
			Volume += SphylElem.GetScaledVolume(Scale3D);
		}
	}
	for (const FKConvexElem& ConvexElem : ConvexElems)
	{
		if (ConvexElem.GetContributeToMass())
		{
			Volume += ConvexElem.GetScaledVolume(Scale3D * ConvexElem.GetTransform().Scale);
		}
	}

	return Volume;
}
