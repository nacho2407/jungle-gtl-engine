#include "AABB.h"

#include <algorithm>
#include <cfloat>

#include "Utils.h"

FAABB::FAABB()
{
	Reset();
}

FAABB::FAABB(const FVector& InMin, const FVector& InMax)
	: Min(InMin), Max(InMax)
{
}

void FAABB::Reset()
{
	Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

bool FAABB::IsValid() const
{
	return Min.X <= Max.X
		&& Min.Y <= Max.Y
		&& Min.Z <= Max.Z;
}

void FAABB::Expand(const FVector& Point)
{
	Min.X = std::min(Point.X, Min.X);
	Min.Y = std::min(Point.Y, Min.Y);
	Min.Z = std::min(Point.Z, Min.Z);
	
	Max.X = std::max(Point.X, Max.X);
	Max.Y = std::max(Point.Y, Max.Y);
	Max.Z = std::max(Point.Z, Max.Z);
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

FVector FAABB::GetCenter() const
{
	return (Min + Max) * 0.5f;
}

FVector FAABB::GetExtent() const
{
	return (Max - Min) * 0.5f;
}

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
		const float BoxMin = (&Min.X)[Axis];
		const float BoxMax = (&Max.X)[Axis];
		
		if (std::fabs(Direction) < Epsilon)
		{
			if (Origin < BoxMin || Origin > BoxMax)
			{
				return false;
			}
			continue;
		}
		
		const float InvD = 1.0f / Direction;
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
