#include "Ray.h"
#include "Component/PrimitiveComponent.h"
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

/**
 * 입력된 컴포넌트의 Geometry를 가져온 뒤, Ray를 컴포넌트의 로컬 공간으로 변환하여
 * 모든 삼각형과 교차 테스트를 수행합니다. 인덱스 버퍼가 있으면 Indices 기준으로,
 * 없으면 Vertices를 3개씩 묶어 삼각형으로 간주합니다.
 * 
 * \param Comp
 * \param OutDistance 교차했을 때, 그 월드 공간 거리를 참조로 반환
 * \return 
 */
bool Ray::Intersects(UPrimitiveComponent* Comp, float& OutDistance)
{
	if (Comp == nullptr || Comp->GetGeometry() == nullptr)
	{
		return false;
	}

	bool bHit = false;
	float ClosestT = FLT_MAX;

	FGeometry* Geometry = Comp->GetGeometry();
	const auto& Vertices = Geometry->Vertices;
	const auto& Indices = Geometry->Indices;

	Ray LocalRay;
	LocalRay.Direction = Comp->GetRelativeMatrix().Inverse().TransformVector(Direction);
	LocalRay.Origin = Comp->GetRelativeMatrix().Inverse().TransformPoint(Origin);
	LocalRay.Direction = LocalRay.Direction.Normalize();

	if (!Indices.empty())
	{
		for (size_t i = 0; i + 2 < Indices.size(); i += 3)
		{
			const FVector& V0 = Vertices[Indices[i]];
			const FVector& V1 = Vertices[Indices[i + 1]];
			const FVector& V2 = Vertices[Indices[i + 2]];

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
	}
	else
	{
		for (size_t i = 0; i + 2 < Vertices.size(); i += 3)
		{
			const FVector& V0 = Vertices[i];
			const FVector& V1 = Vertices[i + 1];
			const FVector& V2 = Vertices[i + 2];

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
	}

	if (bHit)
	{
		//교차했다면 로컬 공간의 교차 지점을 월드 공간으로 변환하여 원점과의 거리를 계산
		const FVector localHitPoint = LocalRay.Origin + LocalRay.Direction * ClosestT;
		const FVector worldHitPoint = Comp->GetRelativeMatrix().TransformPoint(localHitPoint);
		OutDistance = (worldHitPoint - Origin).Length();
	}
	return bHit;
}
