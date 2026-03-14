#include "UPicker.h"
#include "UCamera.h"
#include "Math/FVector4.h"
#include "UPrimitiveComponent.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "Logger.h"
#include "Editor.h"

UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UCamera* Camera, UScene* Scene)
{
	GizmoAxis selectedAxis = PickGizmo(ScreenX, ScreenY, Camera, Scene);
	if (selectedAxis != GizmoAxis::None)
	{
		UE_LOG("Selected Gizmo Axis: %d", static_cast<int>(selectedAxis));
		Gizmo::GetInstance().BeginDrag(selectedAxis, CreateRay(ScreenX, ScreenY, Camera));
		return;
	}

	Ray ray = CreateRay(ScreenX, ScreenY, Camera);
	for (UPrimitiveComponent* Comp : Scene->PrimitiveContainer)
	{
		float Distance = 0.f;
		if (ray.Intersects(Comp, Distance))
		{
			UE_LOG("Hit Object at Distance : %f", Distance);
			EditorPtr->SelectComponent(Comp);
		}
		else
		{
			EditorPtr->SelectComponent(nullptr);
		}
	}
}

// TODO: Optimize later...
GizmoAxis UPicker::PickGizmo(int ScreenX, int ScreenY, UCamera* Camera, UScene* Scene)
{
	Ray ray = CreateRay(ScreenX, ScreenY, Camera);
	GizmoAxis SelectedAxis = GizmoAxis::None;

	USceneComponent* comp = Gizmo::GetInstance().GetAttachedComponent();
	if (comp == nullptr) return GizmoAxis::None;

	FMatrix gizmoMatrix = Gizmo::GetInstance().GetRelativeMatrix();
	
	FVector origin = gizmoMatrix.TransformPoint(FVector(0, 0, 0));

	FVector xDir = (gizmoMatrix.TransformPoint(FVector(1, 0, 0)) - origin).Normalize();
	FVector yDir = (gizmoMatrix.TransformPoint(FVector(0, 1, 0)) - origin).Normalize();
	FVector zDir = (gizmoMatrix.TransformPoint(FVector(0, 0, 1)) - origin).Normalize();

	FVector Axes[3] = { xDir, yDir, zDir };
	GizmoAxis AxisEnums[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };

	float MaxGizmoLength = 1.2f;
	float ThickPickRadius = 0.2f;

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
	//UE_LOG("Near World: (%f, %f, %f)", NearWorld.x, NearWorld.y, NearWorld.z);
	//UE_LOG("Far World: (%f, %f, %f)", FarWorld.x, FarWorld.y, FarWorld.z);
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

REGISTER_CLASS(UPicker)