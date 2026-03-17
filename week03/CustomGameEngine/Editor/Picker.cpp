#include "Picker.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Editor.h"
#include "Gizmo.h"
#include "GizmoController.h"
#include "Renderer/Geometry.h"
#include "Scene.h"
#include "Math/Vector4.h"
#include "Logger.h"

UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UCameraComponent* Camera, UScene* Scene)
{
	UE_LOG("Screen Clicked at: (%d, %d)", ScreenX, ScreenY);

	Ray ray = CreateRay(ScreenX, ScreenY, Camera);
	GizmoControllerAxis selectedAxis =
		Gizmo::GetInstance().GetController()->TryPick(ray);
	if (selectedAxis != GizmoControllerAxis::None)
	{
		Gizmo::GetInstance().GetController()->SetAxis(selectedAxis);

		Ray ray = CreateRay(ScreenX, ScreenY, Camera);
		Gizmo::GetInstance().GetController()->BeginDrag(ray);
		UE_LOG("Gizmo picked!");
		return;
	}

	UPrimitiveComponent* ClosestComp = nullptr;
	float MinDistance = FLT_MAX;
	for (int dx = -1;dx <= 1;++dx)
	{
		for (int dy = -1; dy <= 1;++dy)
		{
			Ray ray = CreateRay(ScreenX + dx, ScreenY + dy, Camera);
			for (USceneComponent* sc : Scene->SceneComponents)
			{
				UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(sc);
				if (Comp == nullptr || !(Comp->Pickable))
					continue;

				float Distance = 0.f;
				if (ray.Intersects(Comp, Distance))
				{
					if (Distance < MinDistance)
					{
						MinDistance = Distance;
						ClosestComp = Comp;
					}
				}
			}
		}
	}

	if (ClosestComp)
	{
		UE_LOG("Hit Object at Distance : %f", MinDistance);
		EditorPtr->SelectComponent(ClosestComp);
		Scene->IsCompSelected = true;
	}
	else
	{
		EditorPtr->SelectComponent(nullptr);
		Scene->IsCompSelected = false;
	}
}

// TODO: Optimize later...
GizmoControllerAxis UPicker::PickGizmo(int ScreenX, int ScreenY, UCameraComponent* Camera, UScene* Scene)
{
	Ray ray = CreateRay(ScreenX, ScreenY, Camera);
	GizmoControllerAxis SelectedAxis = GizmoControllerAxis::None;
	GizmoController* Controller = Gizmo::GetInstance().GetController();

	USceneComponent* comp = Controller->GetAttachedComponent();
	if (comp == nullptr) return SelectedAxis;

	FMatrix gizmoMatrix = Controller->GetRelativeMatrix();

	//for (int i = 0; i < 4; ++i)
	//{
	//	UE_LOG("%f %f %f %f", gizmoMatrix[i][0], gizmoMatrix[i][1], gizmoMatrix[i][2], gizmoMatrix[i][3]);
	//}

	FVector origin = gizmoMatrix.TransformPoint(FVector(0, 0, 0));

	FVector xDir = (gizmoMatrix.TransformPoint(FVector(1, 0, 0)) - origin).Normalize();
	FVector yDir = (gizmoMatrix.TransformPoint(FVector(0, 0, 1)) - origin).Normalize();
	FVector zDir = (gizmoMatrix.TransformPoint(FVector(0, 1, 0)) - origin).Normalize();

	FVector Axes[3] = { xDir, yDir, zDir };
	GizmoControllerAxis AxisEnums[3] = { GizmoControllerAxis::X, GizmoControllerAxis::Y, GizmoControllerAxis::Z };

	float distanceToCamera = (origin - Camera->Position).Length();

	float dynamicRadius = distanceToCamera * 0.02f;

	float MaxGizmoLength = 1.2f;
	float ThickPickRadius = dynamicRadius;
	if (dynamicRadius < 0.1f) ThickPickRadius = 0.1f;
	else if (dynamicRadius > 0.5f) ThickPickRadius = 0.5f;

	float ClosestT = FLT_MAX;

	for (int i = 0; i < 3; ++i)
	{
		FVector u = ray.Direction;
		FVector v = Axes[i];
		FVector w = ray.Origin - origin;
		float a = Dot(u, u); // 항상 1 (정규화됨)
		float b = Dot(u, v);
		float c = Dot(v, v); // 항상 1 (정규화됨)
		float d = Dot(u, w);
		float e = Dot(v, w);

		float D = a * c - b * b;
		float sc, tc;

		// 평행 방어
		if (D < 1e-6f) {
			sc = 0.0f;
			tc = (b > c ? d / b : e / c);
		}
		else {
			sc = (b * e - c * d) / D; // Ray 파라미터 (카메라에서부터의 거리)
			tc = (a * e - b * d) / D; // 축 파라미터 (월드 원점에서부터의 거리)
		}

		// 2. 축의 길이 제한 (0 ~ 기즈모 화살표 끝부분까지만 클릭 가능하게)
		if (tc < 0.0f) tc = 0.0f;
		else if (tc > MaxGizmoLength) tc = MaxGizmoLength;

		// Ray는 카메라 뒤(음수)로 갈 수 없음
		if (sc < 0.0f) continue;

		// 3. 최단 거점을 구하여 두 지점 사이의 실제 거리를 계산
		FVector PointOnRay = ray.Origin + ray.Direction * sc;
		FVector PointOnAxis = origin + Axes[i] * tc;

		float Distance = (PointOnRay - PointOnAxis).Length();

		// 4. 거리가 마우스 피킹 두께 내에 들어오고, 카메라에서 더 가까운 축이라면 갱신
		if (Distance <= ThickPickRadius)
		{
			if (sc < ClosestT)
			{
				ClosestT = sc;
				SelectedAxis = AxisEnums[i];
			}
		}
	}

	return SelectedAxis;
}

Ray UPicker::CreateRay(int ScreenX, int ScreenY, UCameraComponent* Camera)
{
	//Screen Space -> NDC Space
	float ndcX = (2.0f * (static_cast<float>(ScreenX) + 0.5f)) / Camera->ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * (static_cast<float>(ScreenY) + 0.5f)) / Camera->ScreenHeight;
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