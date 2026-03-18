#include "Picker.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor.h"
#include "Gizmo.h"
#include "Renderer/Geometry.h"
#include "Scene.h"
#include "Math/Vector4.h"
#include "Logger.h"
#include <algorithm>

UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UScene* Scene)
{
	Ray ray = CreateRay(ScreenX, ScreenY);

	UGizmoComponent* controller = Gizmo::GetInstance().GetController();
	if (controller)
	{
		GizmoControllerAxis selectedAxis = controller->TryPick(ray);
		if (selectedAxis != GizmoControllerAxis::None)
		{
			controller->SetAxis(selectedAxis);
			controller->BeginDrag(ray);
			return;
		}
	}

	TArray<TPair<float, USceneComponent*>> RayHits;

	for (USceneComponent* sc : Scene->SceneComponents)
	{
		UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(sc);
		if (Comp == nullptr || !(Comp->Pickable))
			continue;

		float Distance = 0.f;
		if (ray.Intersects(Comp, Distance))
		{
			RayHits.push_back({ Distance, Comp });
		}
	}

	if (!RayHits.empty())
	{
		// RayHits 안에 이미 선택된 오브젝트가 있다면 거리순으로 정렬해 그 다음 것을 선택
		std::sort(RayHits.begin(), RayHits.end());
		int nextIndex = 0;
		for (int i = 0; i < RayHits.size(); ++i) {
			if (RayHits[i].second == EditorPtr->GetSelectedComponent()) {
				nextIndex = (i + 1) % RayHits.size();
				break;
			}
		}
		EditorPtr->SelectComponent(RayHits[nextIndex].second);
		Scene->IsCompSelected = true;
	}
	else
	{
		EditorPtr->SelectComponent(nullptr);
		Scene->IsCompSelected = false;
	}
}

Ray UPicker::CreateRay(int ScreenX, int ScreenY)
{
	//Screen Space -> NDC Space
	float ndcX = (2.0f * (static_cast<float>(ScreenX) + 0.5f)) / CameraPtr->ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * (static_cast<float>(ScreenY) + 0.5f)) / CameraPtr->ScreenHeight;
	//NDC -> View Space
	FVector4 Near = FVector4(ndcX, ndcY, 1.f, 1.f);
	FVector4 Far = FVector4(ndcX, ndcY, 0.f, 1.f);
	FMatrix InvProjMat = CameraPtr->GetProjectionMatrix().Inverse();
	FVector4 NearView = Near * InvProjMat;
	FVector4 FarView = Far * InvProjMat;
	NearView = NearView * (1.0f / NearView.w);
	FarView = FarView * (1.0f / FarView.w);
	//View Space -> World Space
	FMatrix InvView = CameraPtr->GetViewMatrix().Inverse();
	FVector4 NearWorld = NearView * InvView;
	FVector4 FarWorld = FarView * InvView;


	//FVector ViewDir = FVector(NearView.x, NearView.y, NearView.z).Normalize();

	Ray ray;
	ray.Origin = FVector(NearWorld.x, NearWorld.y, NearWorld.z);
	ray.Direction = FVector((FarWorld.x - ray.Origin.x),
		(FarWorld.y - ray.Origin.y),
		(FarWorld.z - ray.Origin.z)).Normalize();

	return ray;

	//// 1. 픽셀 중앙을 향하도록 +0.5f 보정 적용
	//float ndcX = (2.0f * (static_cast<float>(ScreenX) + 0.5f)) / Camera->ScreenWidth - 1.0f;
	//float ndcY = 1.0f - (2.0f * (static_cast<float>(ScreenY) + 0.5f)) / Camera->ScreenHeight;

	//FVector4 Near = FVector4(ndcX, ndcY, 0.f, 1.f);
	//FMatrix InvProjMat = Camera->GetProjectionMatrix().Inverse();

	//FVector4 NearView = Near * InvProjMat;
	//NearView = NearView * (1.0f / NearView.w);

	//FMatrix InvView = Camera->GetViewMatrix().Inverse();
	//FVector4 NearWorld = NearView * InvView;

	//Ray ray;
	//// 레이의 시작점은 여전히 NearWorld를 사용
	//ray.Origin = FVector(NearWorld.x, NearWorld.y, NearWorld.z);

	//// 2. FarWorld를 배제하고 View 좌표계 원점(0,0,0)을 기준으로 즉각적인 View 방향 도출
	//FVector ViewDir = FVector(NearView.x, NearView.y, NearView.z).Normalize();

	//// 3. Row-Major 방식에 맞춰 View 방향 벡터를 World 방향 벡터로 변환 (w=0 역할)
	//ray.Direction = InvView.TransformVector(ViewDir).Normalize();

	//return ray;
}

Ray Ray::CreateRay(int x, int y, UCameraComponent* camera)
{
	//Screen Space -> NDC Space
	float ndcX = (2.0f * (static_cast<float>(x) + 0.5f)) / camera->ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * (static_cast<float>(y) + 0.5f)) / camera->ScreenHeight;
	//NDC -> View Space
	FVector4 Near = FVector4(ndcX, ndcY, 1.f, 1.f);
	FVector4 Far = FVector4(ndcX, ndcY, 0.f, 1.f);
	FMatrix InvProjMat = camera->GetProjectionMatrix().Inverse();
	FVector4 NearView = Near * InvProjMat;
	FVector4 FarView = Far * InvProjMat;
	NearView = NearView * (1.0f / NearView.w);
	FarView = FarView * (1.0f / FarView.w);
	//View Space -> World Space
	FMatrix InvView = camera->GetViewMatrix().Inverse();
	FVector4 NearWorld = NearView * InvView;
	FVector4 FarWorld = FarView * InvView;


	//FVector ViewDir = FVector(NearView.x, NearView.y, NearView.z).Normalize();

	Ray ray;
	ray.Origin = FVector(NearWorld.x, NearWorld.y, NearWorld.z);
	ray.Direction = FVector((FarWorld.x - ray.Origin.x),
		(FarWorld.y - ray.Origin.y),
		(FarWorld.z - ray.Origin.z)).Normalize();

	return ray;
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


//UPrimitiveComponent* ClosestComp = nullptr;
//float MinDistance = FLT_MAX;
//for (int dx = -1; dx <= 1; ++dx)
//{
//	for (int dy = -1; dy <= 1; ++dy)
//	{
//		Ray ray = CreateRay(ScreenX + dx, ScreenY + dy, Camera);
//		for (UPrimitiveComponent* Comp : Scene->PrimitiveContainer)
//		{
//			float Distance = 0.f;
//			if (ray.Intersects(Comp, Distance))
//			{
//				if (Distance < MinDistance)
//				{
//					MinDistance = Distance;
//					ClosestComp = Comp;
//				}
//			}
//		}
//	}
//}

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

REGISTER_CLASS(UPicker)