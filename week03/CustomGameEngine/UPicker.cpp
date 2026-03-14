#include "UPicker.h"
#include "UCamera.h"
#include "Math/FVector4.h"
#include "UPrimitiveComponent.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "Logger.h"
UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UCamera* Camera, UScene* Scene)
{
	Ray ray = CreateRay(ScreenX, ScreenY, Camera);
	for (UPrimitiveComponent* Comp : Scene->PrimitiveContainer)
	{
		float Distance = 0.f;
		if (ray.Intersects(Comp, Distance))
		{
			UE_LOG("Hit Object at Distance");
			//TODO : Do Something
		}

	}
}

Ray UPicker::CreateRay(int ScreenX, int ScreenY, UCamera* Camera)
{
	//Screen Space -> NDC Space
	float ndcX = (2.0f * ScreenX) / Camera->ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * ScreenY) / Camera->ScreenHeight;
	//NDC -> View Space
	FVector4 Near = FVector4(ndcX, ndcY, 0.f, 1.f);
	FVector4 Far = FVector4(ndcX, ndcY, 1.f, 1.f);
	FMatrix InvProjMat = Camera->GetProjectionMatrix().Inverse();
	FVector4 NearView = Near * InvProjMat;
	FVector4 FarView = Far * InvProjMat;
	NearView = NearView * (1.0f / NearView.w);
	FarView = FarView * (1.0f / FarView.w);
	//View Space -> World Space
	FMatrix InvView = Camera->GetViewMatrix().Inverse();
	FVector4 NearWorld = NearView * InvView;
	FVector4 FarWorld = FarView * InvView;

	Ray ray;
	ray.Origin = FVector(NearWorld.x, NearWorld.y, NearWorld.z);
	ray.Direction = FVector(FarWorld.x - NearWorld.x, FarWorld.y - NearWorld.y, FarWorld.z - NearWorld.z).Normalize();

	return ray;
}

bool Ray::IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT)
{
	const float EPSILON = 1e-6f;
	FVector Edge1 = V1 - V0;
	FVector Edge2 = V2 - V0;

	FVector h = Cross(Direction, Edge2);
	float a = Dot(Edge1, h);

	// a가 0에 가깝다면 레이가 삼각형 평면과 평행함
	if (a > -EPSILON && a < EPSILON) return false;

	float f = 1.0f / a;
	FVector s = Origin - V0;
	float u = f * Dot(s, h);

	if (u < 0.0f || u > 1.0f) return false;

	FVector q = Cross(s, Edge1);
	float v = f * Dot(Direction, q);

	if (v < 0.0f || u + v > 1.0f) return false;

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

	const auto& Vertices = Comp->GetVertexResource()->Vertices;
	int VertexCount = Comp->GetVertexResource()->vertexCount;

	for (int i = 0; i + 2 < VertexCount; ++i)
	{
		FVector V0 = Comp->GetRelativeMatrix().TransformPoint(Vertices[i]);
		FVector V1 = Comp->GetRelativeMatrix().TransformPoint(Vertices[i + 1]);
		FVector V2 = Comp->GetRelativeMatrix().TransformPoint(Vertices[i + 2]);

		float t;
		if (IntersectsTriangle(V0, V1, V2, t))
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
