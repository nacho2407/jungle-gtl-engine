#include "GizmoRotationControlStrategy.h"
#include "GizmoController.h"
#include "ResourceManager.h"
#include "Object.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "Component/SceneComponent.h"
#include "Logger.h"

GizmoRotationControlStrategy::GizmoRotationControlStrategy(GizmoController* controller)
{
	GizmoX = new RenderObject();
	GizmoY = new RenderObject();
	GizmoZ = new RenderObject();

	Controller = controller;
}

GizmoRotationControlStrategy::~GizmoRotationControlStrategy()
{
	SetDrawEnable(false);
	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoRotationControlStrategy::Update()
{
}

void GizmoRotationControlStrategy::UpdateRenderObject()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		GizmoX->Geometry = &resourceManager->GetGeometry("GizmoRotationX");
		GizmoX->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoX->World = Controller->GetRelativeMatrix();
		GizmoX->bDepthEnabled = false;

		GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;
	}
	{
		GizmoY->Geometry = &resourceManager->GetGeometry("GizmoRotationY");
		GizmoY->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoY->World = Controller->GetRelativeMatrix();
		GizmoY->bDepthEnabled = false;

		GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;
	}
	{
		GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoRotationZ");
		GizmoZ->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoZ->World = Controller->GetRelativeMatrix();
		GizmoZ->bDepthEnabled = false;

		GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
	}
}

void GizmoRotationControlStrategy::BeginDrag(const Ray& ray)
{
	InitialTouchPoint = ray.Origin;
	InitialObjectQuaternion = Controller->GetAttachedComponent()->GetRelativeQuaternion();

	ShowCursor(false);
}

void GizmoRotationControlStrategy::UpdateDrag(const Ray& ray)
{
	FVector currTouchPoint = ray.Origin;
	float diff = currTouchPoint.z - InitialTouchPoint.z;

	FQuat currQuat = InitialObjectQuaternion;
	FQuat deltaQuat;

	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		deltaQuat = FQuat::FromAxisAngle(FVector::Forward(), diff * 20.0f);
		break;
	case GizmoControllerAxis::Y:
		deltaQuat = FQuat::FromAxisAngle(FVector::Right(), diff * 20.0f);
		break;
	case GizmoControllerAxis::Z:
		deltaQuat = FQuat::FromAxisAngle(FVector::Up(), diff * 20.0f);
		break;
	}

	if (Controller->IsLocalSpace())
	{
		currQuat = currQuat * deltaQuat;
	}
	else
	{
		currQuat = deltaQuat * currQuat;
	}

	Controller->GetAttachedComponent()->SetRelativeQuaternion(currQuat);
}

void GizmoRotationControlStrategy::EndDrag()
{
	ShowCursor(true);
}

void GizmoRotationControlStrategy::SetDrawEnable(bool bEnable)
{
	if (bEnable)
	{
		FLevel::GetInstance().PushRenderObject(GizmoX);
		FLevel::GetInstance().PushRenderObject(GizmoY);
		FLevel::GetInstance().PushRenderObject(GizmoZ);
	}
	else
	{
		FLevel::GetInstance().RemoveRenderObject(GizmoX);
		FLevel::GetInstance().RemoveRenderObject(GizmoY);
		FLevel::GetInstance().RemoveRenderObject(GizmoZ);
	}
}