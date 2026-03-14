#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"

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


	renderObj->Geometry = &resourceManager->GetGeometry("GizmoTranslation");
	renderObj->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");
	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(1.0f / target->RelativeScale3D.x, 1.0f / target->RelativeScale3D.y, 1.0f / target->RelativeScale3D.z);
	renderObj->World = invScaleMatrix * target->GetRelativeMatrix();
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
