#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "UPicker.h"
#include "Logger.h"

void Gizmo::Initialize()
{
	GetInstance().renderObj = new RenderObject();
}

RenderObject* Gizmo::GetRenderObject()
{
	return renderObj;
}

RenderObject* Gizmo::UpdateRenderObject(USceneComponent* target, TransformationType type)
{
	switch (type)
	{
	case TransformationType::Location:
		return UpdateLocationGizmo(target);

	default:
		break;
	}
}

RenderObject* Gizmo::UpdateLocationGizmo(USceneComponent* target)
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	renderObj->Geometry = &resourceManager->GetGeometry("GizmoTranslation");
	renderObj->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");
	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(
		1.0f / target->GetRelativeScale3D().x, 
		1.0f / target->GetRelativeScale3D().y, 
		1.0f / target->GetRelativeScale3D().z
	);
	renderObj->World = invScaleMatrix * target->GetRelativeMatrix();
	renderObj->bDepthEnabled = false;

	return renderObj;
}

RenderObject* Gizmo::UpdateRotationGizmo(USceneComponent* target)
{
	return nullptr;
}

RenderObject* Gizmo::UpdateScaleGizmo(USceneComponent* target)
{
	return nullptr;
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

void Gizmo::BeginDrag(GizmoAxis axis, const Ray& mouseRay)
{
	if (AttachedComponent == nullptr) return;

	bIsDragging = true;
	CurrentAxis = axis;
	InitialObjectLocation = AttachedComponent->GetRelativeLocation();

	FVector axisDir;
	if (axis == GizmoAxis::X) axisDir = FVector(1, 0, 0);
	else if (axis == GizmoAxis::Y) axisDir = FVector(0, 1, 0);
	else if (axis == GizmoAxis::Z) axisDir = FVector(0, 0, 1);

	FVector worldOrigin = RelativeMatrix.TransformPoint(FVector(0, 0, 0));
	FVector worldAxisPoint = RelativeMatrix.TransformPoint(axisDir);
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	InitialDragOffset = GetClosestPointOnAxis(mouseRay, worldOrigin, worldAxisDir);

	UE_LOG("Begin Dragging Gizmo on Axis: %d, Initial Offset: %f", static_cast<int>(axis), InitialDragOffset);
}

void Gizmo::UpdateDrag(const Ray& currentMouseRay)
{
	if (!bIsDragging || !AttachedComponent) return;

	FVector axisDir;
	if (CurrentAxis == GizmoAxis::X) axisDir = FVector(1, 0, 0);
	else if (CurrentAxis == GizmoAxis::Y) axisDir = FVector(0, 1, 0);
	else if (CurrentAxis == GizmoAxis::Z) axisDir = FVector(0, 0, 1);

	FVector worldOrigin = RelativeMatrix.TransformPoint(FVector(0, 0, 0));
	FVector worldAxisPoint = RelativeMatrix.TransformPoint(axisDir);
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	float currentT = GetClosestPointOnAxis(currentMouseRay, worldOrigin, worldAxisDir);
	float deltaT = currentT - InitialDragOffset;

	FVector newLocation = AttachedComponent->GetRelativeLocation() + worldAxisDir * deltaT;
	AttachedComponent->SetRelativeLocation(newLocation);
}

void Gizmo::EndDrag()
{
	UE_LOG("End Dragging Gizmo on Axis: %d", static_cast<int>(CurrentAxis));

	bIsDragging = false;
	CurrentAxis = GizmoAxis::None;
}
