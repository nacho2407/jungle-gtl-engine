#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "UPicker.h"
#include "Logger.h"

URenderObject* Gizmo::DrawGizmo(USceneComponent* target, TransformationType type)
{
	switch (type)
	{
	case TransformationType::Location:
		return DrawLocationGizmo(target);

	default:
		break;
	}
}

URenderObject* Gizmo::DrawLocationGizmo(USceneComponent* target)
{
	UObject* obj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* renderObj = Cast<URenderObject>(obj);
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	VertexResource resource = resourceManager->GetVertexResource("GizmoTranslation");

	renderObj->SetObject(resource.VertexBuffer,
		resourceManager->GetVertexShader(L"ShaderW0.hlsl"),
		resourceManager->GetPixelShader(L"ShaderW0.hlsl"),
		resourceManager->GetInputLayout(L"ShaderW0.hlsl"),
		resource.vertexCount,
		resource.offset,
		resource.stride);

	RenderObjectConstants constants;
	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(1.0f / target->RelativeScale3D.x, 1.0f / target->RelativeScale3D.y, 1.0f / target->RelativeScale3D.z);
	constants.Model = invScaleMatrix * target->GetRelativeMatrix();
	RelativeMatrix = constants.Model;
	renderObj->SetConstants(constants);
	renderObj->bDepthEnabled = false;

	return renderObj;
}

URenderObject* Gizmo::DrawRotationGizmo(USceneComponent* target)
{
	return nullptr;
}

URenderObject* Gizmo::DrawScaleGizmo(USceneComponent* target)
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
	InitialObjectLocation = AttachedComponent->RelativeLocation;

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

	AttachedComponent->RelativeLocation = AttachedComponent->RelativeLocation + worldAxisDir * deltaT;
}

void Gizmo::EndDrag()
{
	UE_LOG("End Dragging Gizmo on Axis: %d", static_cast<int>(CurrentAxis));

	bIsDragging = false;
	CurrentAxis = GizmoAxis::None;
}
