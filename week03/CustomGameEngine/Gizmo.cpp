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
