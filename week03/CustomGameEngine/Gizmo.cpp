#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "UPicker.h"
#include "Logger.h"
#include "GizmoController.h"

Gizmo::Gizmo()
{
	Controller = new GizmoController();
}

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
	case TransformationType::Rotation:
		return UpdateRotationGizmo(target);
	case TransformationType::Scale:
		return UpdateScaleGizmo(target);
	}
}	

Gizmo::~Gizmo()
{
	delete Controller;
}

void Gizmo::OnBeforeRender()
{
	if (Controller)
	{
		Controller->Update();
		Controller->UpdateRenderObject();
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
