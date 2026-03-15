#include "GizmoScaleControlStrategy.h"
#include "GizmoController.h"
#include "ResourceManager.h"
#include "UObject.h"
#include "URender.h"
#include "RenderObject.h"
#include "USceneComponent.h"
#include "UPicker.h"
#include "Logger.h"

GizmoScaleControlStrategy::GizmoScaleControlStrategy(GizmoController* controller)
{
	GizmoX = new RenderObject();
	GizmoY = new RenderObject();
	GizmoZ = new RenderObject();

	Controller = controller;
}

GizmoScaleControlStrategy::~GizmoScaleControlStrategy()
{
	FLevel::GetInstance().RemoveRenderObject(GizmoX);
	FLevel::GetInstance().RemoveRenderObject(GizmoY);
	FLevel::GetInstance().RemoveRenderObject(GizmoZ);

	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoScaleControlStrategy::Update()
{
}

void GizmoScaleControlStrategy::UpdateRenderObject()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		GizmoX->Geometry = &resourceManager->GetGeometry("GizmoScaleX");
		GizmoX->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoX->World = Controller->GetRelativeMatrix();
		GizmoX->bDepthEnabled = false;

		GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;
	}
	{
		GizmoY->Geometry = &resourceManager->GetGeometry("GizmoScaleY");
		GizmoY->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoY->World = Controller->GetRelativeMatrix();
		GizmoY->bDepthEnabled = false;

		GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;
	}
	{
		GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoScaleZ");
		GizmoZ->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoZ->World = Controller->GetRelativeMatrix();
		GizmoZ->bDepthEnabled = false;

		GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
	}
}

void GizmoScaleControlStrategy::BeginDrag(const Ray& ray)
{
	InitialObjectScale = Controller->GetAttachedComponent()->GetRelativeScale3D();

	FVector axisDir;
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(1, 0, 0));
		break;
	case GizmoControllerAxis::Y:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(0, 0, 1));
		break;
	case GizmoControllerAxis::Z:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(0, 1, 0));
		break;
	}

	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector(0, 0, 0));
	FVector worldAxisPoint = Controller->GetRelativeMatrix().TransformPoint(axisDir);
	InitialWorldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	InitialDragOffset = Controller->GetClosestPointOnAxis(ray, worldOrigin, InitialWorldAxisDir);
}

void GizmoScaleControlStrategy::UpdateDrag(const Ray& ray)
{
	FVector axisDir;
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(1, 0, 0));
		break;
	case GizmoControllerAxis::Y:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(0, 0, 1));
		break;
	case GizmoControllerAxis::Z:
		axisDir = Controller->GetRelativeMatrix().TransformVector(FVector(0, 1, 0));
		break;
	}

	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector(0, 0, 0));

	float currentT = Controller->GetClosestPointOnAxis(ray, worldOrigin, InitialWorldAxisDir);

	float deltaT = currentT - InitialDragOffset;

	FVector newScale = InitialObjectScale;
	float Sensitivity = 1.0f;

	if (Controller->GetHoverAxis() == GizmoControllerAxis::X) newScale.x += deltaT * Sensitivity;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Y) newScale.z += deltaT * Sensitivity;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Z) newScale.y += deltaT * Sensitivity;

	Controller->GetAttachedComponent()->SetRelativeScale3D(newScale);
}

void GizmoScaleControlStrategy::EndDrag()
{
}

void GizmoScaleControlStrategy::SetDrawEnable(bool bEnable)
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