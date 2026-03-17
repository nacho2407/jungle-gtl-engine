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
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	GizmoX = new RenderObject();
	GizmoX->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoX->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoX->bDepthEnabled = false;
	GizmoX->Color = FColor(1.0f, 0.0f, 0.0f, 1.0f);

	GizmoY = new RenderObject();
	GizmoY->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoY->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoY->bDepthEnabled = false;
	GizmoY->Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);

	GizmoZ = new RenderObject();
	GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoZ->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoZ->bDepthEnabled = false;
	GizmoZ->Color = FColor(0.0f, 0.0f, 1.0f, 1.0f);

	Controller = controller;

	RelativeMatrices.resize(3);
	RelativeMatrices[0] = FMatrix::RotationYMatrix(-90.0f);
	RelativeMatrices[1] = FMatrix::RotationXMatrix(90.0f);
	RelativeMatrices[2] = FMatrix::IdentityMatrix();
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
	GizmoX->World = RelativeMatrices[0] * Controller->GetRelativeMatrix();
	GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;

	GizmoY->World = RelativeMatrices[1] * Controller->GetRelativeMatrix();
	GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;

	GizmoZ->World = Controller->GetRelativeMatrix();
	GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
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