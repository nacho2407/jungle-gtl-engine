#include "AABB.h"

#include "Engine/Math/Matrix.h"
#include "Engine/Geometry/Ray.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

FAABB::FAABB()
{
	Reset();
}

FAABB::FAABB(const FVector& InMin, const FVector& InMax)
	: Min(InMin), Max(InMax) {}

void FAABB::Reset()
{
	Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

bool FAABB::IsValid() const
{
	return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}

FAABB FAABB::ExpandedBy(float Radius) const
{
	// Sphere Sweep은 moving sphere 대신 moving center를 검사한다.
	// target AABB를 radius만큼 키우면 center segment 하나로 broad phase를 통일할 수 있다.
	const float SafeRadius = std::max(Radius, 0.0f);
	return ExpandedBy(FVector(SafeRadius, SafeRadius, SafeRadius));
}

FAABB FAABB::ExpandedBy(const FVector& Extent) const
{
	if (!IsValid())
	{
		return FAABB();
	}

	// 음수 확장은 bounds를 줄여 버리므로 query 입력은 0 이상으로만 사용한다.
	const FVector SafeExtent(
		std::max(Extent.X, 0.0f),
		std::max(Extent.Y, 0.0f),
		std::max(Extent.Z, 0.0f));

	return FAABB(Min - SafeExtent, Max + SafeExtent);
}

void FAABB::Expand(const FVector& Point)
{
	// Original:
	// Min.X = std::min(Point.X, Min.X);
	// Min.Y = std::min(Point.Y, Min.Y);
	// Min.Z = std::min(Point.Z, Min.Z);
	//
	// Max.X = std::max(Point.X, Max.X);
	// Max.Y = std::max(Point.Y, Max.Y);
	// Max.Z = std::max(Point.Z, Max.Z);

	const XMVector PointV = Point.ToXMVector();
	const XMVector MinV = Min.ToXMVector();
	const XMVector MaxV = Max.ToXMVector();

	Min = FVector(DirectX::XMVectorMin(MinV, PointV));
	Max = FVector(DirectX::XMVectorMax(MaxV, PointV));
}

void FAABB::Merge(const FAABB& Other)
{
	if (!Other.IsValid())
	{
		return;
	}

	Expand(Other.Min);
	Expand(Other.Max);
}

// FVector FAABB::GetCenter() const
//{
//	// Original:
//	// return (Min + Max) * 0.5f;
//
//	const XMVector Center = DirectX::XMVectorScale(
//		DirectX::XMVectorAdd(Min.ToXMVector(), Max.ToXMVector()),
//		0.5f);
//	return FVector(Center);
// }
//
// FVector FAABB::GetExtent() const
//{
//	// Original:
//	// return (Max - Min) * 0.5f;
//
//	const XMVector Extent = DirectX::XMVectorScale(
//		DirectX::XMVectorSubtract(Max.ToXMVector(), Min.ToXMVector()),
//		0.5f);
//	return FVector(Extent);
// }

bool FAABB::IntersectRay(const FRay& Ray, float& OutT) const
{
	if (!IsValid())
	{
		return false;
	}

	float TMin = 0.0f;
	float TMax = FLT_MAX;

	for (int Axis = 0; Axis < 3; Axis++)
	{
		const float Origin = (&Ray.Origin.X)[Axis];
		const float Direction = (&Ray.Direction.X)[Axis];
		const float BoxMin = Min[Axis];
		const float BoxMax = Max[Axis];

		if (std::fabs(Direction) < MathUtil::Epsilon)
		{
			if (Origin < Min[Axis] || Origin > Max[Axis])
			{
				return false;
			}
			continue;
		}

		const float InvD = (&Ray.InvD.X)[Axis];
		float T1 = (BoxMin - Origin) * InvD;
		float T2 = (BoxMax - Origin) * InvD;

		if (T1 > T2)
		{
			std::swap(T1, T2);
		}

		TMin = std::max(TMin, T1);
		TMax = std::min(TMax, T2);

		if (TMin > TMax)
		{
			return false;
		}
	}

	OutT = TMin;

	return true;
}

bool FAABB::IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const
{
	// Early Exit
	float t1 = (Min.X - Ray.Origin.X) / Ray.Direction.X;
	float t2 = (Max.X - Ray.Origin.X) / Ray.Direction.X;
	OutTMin = std::min(t1, t2);
	OutTMax = std::max(t1, t2);

	for (int i = 1; i < 3; ++i)
	{ // Y, Z 축 반복
		t1 = (Min[i] - Ray.Origin[i]) / Ray.Direction[i];
		t2 = (Max[i] - Ray.Origin[i]) / Ray.Direction[i];
		OutTMin = std::max(OutTMin, std::min(t1, t2));
		OutTMax = std::min(OutTMax, std::max(t1, t2));
	}

	return OutTMax >= OutTMin && OutTMax >= 0.0f;
}

bool FAABB::IntersectSegment(const FVector& Start, const FVector& End, float& OutTEnter, float& OutTExit) const
{
	// Segment는 T = 0에서 Start, T = 1에서 End다.
	// 처음부터 0~1로 제한해 두면 이동 구간 밖의 AABB는 후보로 들어오지 않는다.
	OutTEnter = 0.0f;
	OutTExit = 1.0f;

	if (!IsValid())
	{
		return false;
	}

	const FVector Move = End - Start;

	// Slab test다. 각 축에서 box 안에 있는 T 구간을 구한 뒤,
	// 세 축 모두에서 동시에 살아 있는 구간이 남는지 확인한다.
	for (int Axis = 0; Axis < 3; ++Axis)
	{
		const float Origin = Start[Axis];
		const float Direction = Move[Axis];

		if (std::fabs(Direction) < MathUtil::Epsilon)
		{
			// 이 축으로 움직이지 않으면 시작점이 slab 안에 있어야 끝까지 통과할 수 있다.
			if (Origin < Min[Axis] || Origin > Max[Axis])
			{
				return false;
			}
			continue;
		}

		// AABB 범위 안에 들어갈 조건
		// Min <= Start + Dir * t <= Max
		const float InvDirection = 1.0f / Direction;
		float T1 = (Min[Axis] - Origin) * InvDirection;
		float T2 = (Max[Axis] - Origin) * InvDirection;

		// Dir이 음수인 경우 고려
		if (T1 > T2)
		{
			std::swap(T1, T2);
		}

		// 지금까지 검사한 축들이 모두 허용하는 공통 시간 구간만 남긴다.
		OutTEnter = std::max(OutTEnter, T1);
		OutTExit = std::min(OutTExit, T2);
		if (OutTEnter > OutTExit)
		{
			return false;
		}
	}

	return true;
}

float FAABB::SquaredDistanceToPoint(const FVector& Point) const
{
	const float X = std::max(Min.X - Point.X, std::max(0.0f, Point.X - Max.X));
	const float Y = std::max(Min.Y - Point.Y, std::max(0.0f, Point.Y - Max.Y));
	const float Z = std::max(Min.Z - Point.Z, std::max(0.0f, Point.Z - Max.Z));
	return X * X + Y * Y + Z * Z;
}

bool FAABB::IntersectsSphere(const FVector& Center, float Radius) const
{
	if (!IsValid() || Radius <= 0.0f)
	{
		return false;
	}

	return SquaredDistanceToPoint(Center) <= Radius * Radius;
}

FAABB FAABB::TransformAABB(const FAABB& InLocalAABB, const FMatrix& InMatrix)
{
	const FVector& Min = InLocalAABB.Min;
	const FVector& Max = InLocalAABB.Max;

	const FVector Corners[8] = {
		FVector(Min.X, Min.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max.X, Max.Y, Max.Z),
	};

	FVector NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& Corner : Corners)
	{
		const FVector P = InMatrix.TransformPosition(Corner);

		NewMin.X = (P.X < NewMin.X) ? P.X : NewMin.X;
		NewMin.Y = (P.Y < NewMin.Y) ? P.Y : NewMin.Y;
		NewMin.Z = (P.Z < NewMin.Z) ? P.Z : NewMin.Z;

		NewMax.X = (P.X > NewMax.X) ? P.X : NewMax.X;
		NewMax.Y = (P.Y > NewMax.Y) ? P.Y : NewMax.Y;
		NewMax.Z = (P.Z > NewMax.Z) ? P.Z : NewMax.Z;
	}

	return FAABB(NewMin, NewMax);
}

void FAABB::ExpandToInclude(const FAABB& Other)
{
	const XMVector MinV = Min.ToXMVector();
	const XMVector MaxV = Max.ToXMVector();
	const XMVector OtherMinV = Other.Min.ToXMVector();
	const XMVector OtherMaxV = Other.Max.ToXMVector();

	Min = FVector(DirectX::XMVectorMin(MinV, OtherMinV));
	Max = FVector(DirectX::XMVectorMax(MaxV, OtherMaxV));

	// Original:
	// Min.X = FMath::Min(Min.X, Other.Min.X);
	// Min.Y = FMath::Min(Min.Y, Other.Min.Y);
	// Min.Z = FMath::Min(Min.Z, Other.Min.Z);
	//
	// Max.X = FMath::Max(Max.X, Other.Max.X);
	// Max.Y = FMath::Max(Max.Y, Other.Max.Y);
	// Max.Z = FMath::Max(Max.Z, Other.Max.Z);
}

bool FAABB::NearlyEqualAABB(const FAABB& Other) const
{
	return Min.Equals(Other.Min) && Max.Equals(Other.Max);
}

bool FAABB::NearlyEqualAABB(const FAABB& A, const FAABB& B)
{
	return A.Min.Equals(B.Min) && A.Max.Equals(B.Max);
}
