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

	FVector worldAxis;
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		worldAxis = FVector::Forward();
		break;
	case GizmoControllerAxis::Y:
		worldAxis = FVector::Right();
		break;
	case GizmoControllerAxis::Z:
		worldAxis = FVector::Up();
		break;
	}

	FVector axisDir = Controller->GetRelativeMatrix().TransformVector(worldAxis).Normalize();
	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector::Zero());

	InitialWorldPos = worldOrigin;
	InitialDragOffset = Controller->GetClosestPointOnAxis(ray, worldOrigin, axisDir);
}

void GizmoScaleControlStrategy::UpdateDrag(const Ray& ray)
{
	FVector worldAxis;
	switch (Controller->GetHoverAxis())
	{
	case GizmoControllerAxis::X:
		worldAxis = FVector::Forward();
		break;
	case GizmoControllerAxis::Y:
		worldAxis = FVector::Right();
		break;
	case GizmoControllerAxis::Z:
		worldAxis = FVector::Up();
		break;
	}

	FVector axisDir = Controller->GetRelativeMatrix().TransformVector(worldAxis).Normalize();
	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector::Zero());

	float currentT = Controller->GetClosestPointOnAxis(ray, InitialWorldPos, axisDir);
	float deltaT = currentT - InitialDragOffset;

	FVector newScale = InitialObjectScale;

	if (Controller->GetHoverAxis() == GizmoControllerAxis::X) newScale.z += deltaT;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Y) newScale.x += deltaT;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Z) newScale.y += deltaT;

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