#include "GizmoController.h"
#include "UObject.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "USceneComponent.h"
#include "URender.h"
#include "Logger.h"
#include "InputManager.h"
#include "UPicker.h"

GizmoController::GizmoController()
{
	GizmoTranslationX = new RenderObject();
	GizmoTranslationY = new RenderObject();
	GizmoTranslationZ = new RenderObject();
	GizmoRotationX = new RenderObject();
	GizmoRotationY = new RenderObject();
	GizmoRotationZ = new RenderObject();
	GizmoScaleX = new RenderObject();
	GizmoScaleY = new RenderObject();
	GizmoScaleZ = new RenderObject();

	FLevel::GetInstance().PushRenderObject(GizmoTranslationX);
	FLevel::GetInstance().PushRenderObject(GizmoTranslationY);
	FLevel::GetInstance().PushRenderObject(GizmoTranslationZ);
}

GizmoController::~GizmoController()
{
}

void GizmoController::Update()
{
	if (AttachedComponent == nullptr) return;

	FVector scale = AttachedComponent->GetRelativeScale3D();
	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(
		1.0f / scale.x,
		1.0f / scale.y,
		1.0f / scale.z);
	RelativeMatrix = invScaleMatrix * AttachedComponent->GetRelativeMatrix();
}

void GizmoController::UpdateRenderObject()
{
	if (AttachedComponent == nullptr) return;

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		GizmoTranslationX->Geometry = &resourceManager->GetGeometry("GizmoTranslationX");
		GizmoTranslationX->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoTranslationX->World = RelativeMatrix;
		GizmoTranslationX->bDepthEnabled = false;

		GizmoTranslationX->bIsSelected = HoverAxis == GizmoControllerAxis::X;
	}
	{
		GizmoTranslationY->Geometry = &resourceManager->GetGeometry("GizmoTranslationY");
		GizmoTranslationY->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoTranslationY->World = RelativeMatrix;
		GizmoTranslationY->bDepthEnabled = false;

		GizmoTranslationY->bIsSelected = HoverAxis == GizmoControllerAxis::Y;
	}
	{
		GizmoTranslationZ->Geometry = &resourceManager->GetGeometry("GizmoTranslationZ");
		GizmoTranslationZ->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoTranslationZ->World = RelativeMatrix;
		GizmoTranslationZ->bDepthEnabled = false;

		GizmoTranslationZ->bIsSelected = HoverAxis == GizmoControllerAxis::Z;
	}

	return;
}

float GetClosestPointOnAxis(const Ray& ray, const FVector& axisOrigin, const FVector& axisDir)
{
	FVector u = ray.Direction;
	FVector v = axisDir;
	FVector w = ray.Origin - axisOrigin;

	float a = Dot(u, u);
	float b = Dot(u, v);
	float c = Dot(v, v);
	float d = Dot(u, w);
	float e = Dot(v, w);

	float denominator = a * c - b * b;
	if (denominator < 1e-6f) return 0.0f;

	return (a * e - b * d) / denominator;
}

void GizmoController::BeginDrag(const Ray& ray)
{
	if (IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	bDragging = true;

	InitialObjectLocation = AttachedComponent->GetRelativeLocation();

	FVector axisDir;
	if (CurrentAxis == GizmoControllerAxis::X) axisDir = FVector(1, 0, 0);
	else if (CurrentAxis == GizmoControllerAxis::Y) axisDir = FVector(0, 0, 1);
	else if (CurrentAxis == GizmoControllerAxis::Z) axisDir = FVector(0, 1, 0);

	FVector worldOrigin = RelativeMatrix.TransformPoint(FVector(0, 0, 0));
	FVector worldAxisPoint = RelativeMatrix.TransformPoint(axisDir);
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	InitialDragOffset = GetClosestPointOnAxis(ray, worldOrigin, worldAxisDir);
}

void GizmoController::UpdateDrag(const Ray& ray)
{
	if (!IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	FVector axisDir;
	if (CurrentAxis == GizmoControllerAxis::X) axisDir = FVector(1, 0, 0);
	else if (CurrentAxis == GizmoControllerAxis::Y) axisDir = FVector(0, 0, 1);
	else if (CurrentAxis == GizmoControllerAxis::Z) axisDir = FVector(0, 1, 0);

	FVector worldOrigin = RelativeMatrix.TransformPoint(FVector(0, 0, 0));
	FVector worldAxisPoint = RelativeMatrix.TransformPoint(axisDir);
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	float currentT = GetClosestPointOnAxis(ray, worldOrigin, worldAxisDir);
	float deltaT = currentT - InitialDragOffset;

	FVector newLocation = AttachedComponent->GetRelativeLocation() + worldAxisDir * deltaT;
	AttachedComponent->SetRelativeLocation(newLocation);
}

void GizmoController::EndDrag()
{
	if (!IsDragging()) return;

	bDragging = false;
}