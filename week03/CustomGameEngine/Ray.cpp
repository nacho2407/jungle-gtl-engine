#include "Ray.h"
#include "Renderer/Geometry.h"
#include "Math/Vector4.h"

Ray::Ray(FVector from, FVector dir)
{
	Origin = from;
	Direction = dir.Normalize();
}

bool Ray::IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT, const Ray& ray)
{
	const float EPSILON = 1e-8f;
	FVector Edge1 = V1 - V0;
	FVector Edge2 = V2 - V0;

	FVector h = Cross(ray.Direction, Edge2);
	float a = Dot(Edge1, h);

	float tolerance = 0.02f;

	// a가 0에 가깝다면 레이가 삼각형 평면과 평행함
	if (a > -EPSILON && a < EPSILON) return false;

	float f = 1.0f / a;
	FVector s = ray.Origin - V0;
	float u = f * Dot(s, h);

	if (u < -tolerance || u > 1.0f + tolerance) return false;

	FVector q = Cross(s, Edge1);
	float v = f * Dot(ray.Direction, q);

	if (v < -tolerance || u + v > 1.0f + tolerance) return false;

	// t 값을 계산하여 레이의 교점 위치 확인
	float t = f * Dot(Edge2, q);

	if (t > EPSILON) // 레이의 진행 방향에 교점이 있음
	{
		OutT = t;
		return true;
	}

	return false;
}

bool Ray::Intersects(UPrimitiveComponent* Comp, float& OutDistance)
{
	bool bHit = false;
	float ClosestT = FLT_MAX;

	const auto& Vertices = Comp->GetGeometry()->Vertices;
	int VertexCount = Comp->GetGeometry()->VertexCount;

	Ray LocalRay;
	LocalRay.Direction = Comp->GetRelativeMatrix().Inverse().TransformVector(Direction);
	LocalRay.Origin = Comp->GetRelativeMatrix().Inverse().TransformPoint(Origin);

	for (int i = 0; i + 2 < VertexCount; i += 3)
	{
		FVector V0 = Vertices[i];
		FVector V1 = Vertices[i + 1];
		FVector V2 = Vertices[i + 2];

		float t;
		if (IntersectsTriangle(V0, V1, V2, t, LocalRay))
		{
			if (t < ClosestT)
			{
				ClosestT = t;
				bHit = true;
			}
		}
	}

	if (bHit)
	{
		OutDistance = ClosestT;
	}
	return bHit;
}