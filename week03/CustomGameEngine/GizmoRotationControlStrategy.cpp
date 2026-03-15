#include "GizmoRotationControlStrategy.h"
#include "GizmoController.h"
#include "ResourceManager.h"
#include "UObject.h"
#include "URender.h"
#include "RenderObject.h"
#include "USceneComponent.h"
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
	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoRotationControlStrategy::Update()
{
}

void GizmoRotationControlStrategy::Draw()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		GizmoX->Geometry = &resourceManager->GetGeometry("GizmoRotationX");
		GizmoX->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoX->World = Controller->GetRelativeMatrix();
		GizmoX->bDepthEnabled = false;

		GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;
	}
	{
		GizmoY->Geometry = &resourceManager->GetGeometry("GizmoRotationY");
		GizmoY->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoY->World = Controller->GetRelativeMatrix();
		GizmoY->bDepthEnabled = false;

		GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;
	}
	{
		GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoRotationZ");
		GizmoZ->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoZ->World = Controller->GetRelativeMatrix();
		GizmoZ->bDepthEnabled = false;

		GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
	}
}

void GizmoRotationControlStrategy::BeginDrag(const Ray& ray)
{
	FVector axisDir;
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		axisDir = FVector(1, 0, 0);
		break;
	case GizmoControllerAxis::Y:
		axisDir = FVector(0, 0, 1);
		break;
	case GizmoControllerAxis::Z:
		axisDir = FVector(0, 1, 0);
		break;
	}

	InitialTouchPoint = ray.Origin;
}

void GizmoRotationControlStrategy::UpdateDrag(const Ray& ray)
{
	FVector currTouchPoint = ray.Origin;
	float diff = currTouchPoint.y - InitialTouchPoint.y;

	UE_LOG("Diff: %f", diff);

	FVector currRotation = Controller->GetAttachedComponent()->GetRelativeRotation();
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		currRotation.x = InitialObjectRotation.x + diff * 1000.0f;
		break;
	case GizmoControllerAxis::Y:
		currRotation.z = InitialObjectRotation.y + diff * 1000.0f;
		break;
	case GizmoControllerAxis::Z:
		currRotation.y = InitialObjectRotation.z + diff * 1000.0f;
		break;
	}

	Controller->GetAttachedComponent()->SetRelativeRotation(currRotation);
}

void GizmoRotationControlStrategy::EndDrag()
{
	// Rotation drag finalization logic can be implemented here
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