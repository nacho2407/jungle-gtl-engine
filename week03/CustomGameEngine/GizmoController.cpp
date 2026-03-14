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
}

GizmoController::~GizmoController()
{
}

void GizmoController::Update()
{
	if (AttachedComponent == nullptr) return;

	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(
		1.0f / AttachedComponent->RelativeScale3D.x,
		1.0f / AttachedComponent->RelativeScale3D.y,
		1.0f / AttachedComponent->RelativeScale3D.z);
	RelativeMatrix = invScaleMatrix * AttachedComponent->GetRelativeMatrix();
}

void GizmoController::Draw(URenderer* renderer)
{
	if (AttachedComponent == nullptr) return;

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		UObject* obj = FObjectFactory::ConstructObject(URenderObject::GetClass());
		URenderObject* renderObj = Cast<URenderObject>(obj);

		renderObj->Geometry = &resourceManager->GetGeometry("GizmoTranslationX");
		renderObj->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		renderObj->World = RelativeMatrix;
		renderObj->bDepthEnabled = false;

		renderObj->bIsSelected = HoverAxis == GizmoControllerAxis::X;

		renderer->PushRenderObject(renderObj);
	}
	{
		UObject* obj = FObjectFactory::ConstructObject(URenderObject::GetClass());
		URenderObject* renderObj = Cast<URenderObject>(obj);

		renderObj->Geometry = &resourceManager->GetGeometry("GizmoTranslationY");
		renderObj->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		renderObj->World = RelativeMatrix;
		renderObj->bDepthEnabled = false;

		renderObj->bIsSelected = HoverAxis == GizmoControllerAxis::Y;

		renderer->PushRenderObject(renderObj);
	}
	{
		UObject* obj = FObjectFactory::ConstructObject(URenderObject::GetClass());
		URenderObject* renderObj = Cast<URenderObject>(obj);

		renderObj->Geometry = &resourceManager->GetGeometry("GizmoTranslationZ");
		renderObj->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		renderObj->World = RelativeMatrix;
		renderObj->bDepthEnabled = false;

		renderObj->bIsSelected = HoverAxis == GizmoControllerAxis::Z;

		renderer->PushRenderObject(renderObj);
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

	InitialObjectLocation = AttachedComponent->RelativeLocation;

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

	AttachedComponent->RelativeLocation = AttachedComponent->RelativeLocation + worldAxisDir * deltaT;
}

void GizmoController::EndDrag()
{
	if (!IsDragging()) return;

	bDragging = false;
}