#include "GizmoTranslationControlStrategy.h"
#include "ResourceManager.h"
#include "UObject.h"
#include "RenderObject.h"
#include "GizmoController.h"
#include "USceneComponent.h"
#include "URender.h"

GizmoTranslationControlStrategy::GizmoTranslationControlStrategy(GizmoController* controller)
{
	GizmoX = new RenderObject();
	GizmoY = new RenderObject();
	GizmoZ = new RenderObject();

	Controller = controller;
}

GizmoTranslationControlStrategy::~GizmoTranslationControlStrategy()
{
	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoTranslationControlStrategy::Update()
{
	// No specific update logic for translation control
}

void GizmoTranslationControlStrategy::Draw()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	{
		GizmoX->Geometry = &resourceManager->GetGeometry("GizmoTranslationX");
		GizmoX->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoX->World = Controller->GetRelativeMatrix();
		GizmoX->bDepthEnabled = false;

		GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;
	}
	{
		GizmoY->Geometry = &resourceManager->GetGeometry("GizmoTranslationY");
		GizmoY->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoY->World = Controller->GetRelativeMatrix();
		GizmoY->bDepthEnabled = false;

		GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;
	}
	{
		GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoTranslationZ");
		GizmoZ->Material = &resourceManager->GetMaterial(L"ShaderW0.hlsl");

		GizmoZ->World = Controller->GetRelativeMatrix();
		GizmoZ->bDepthEnabled = false;

		GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
	}
}

void GizmoTranslationControlStrategy::BeginDrag(const Ray& ray)
{
	InitialObjectLocation = Controller->GetAttachedComponent()->GetRelativeLocation();

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
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	InitialDragOffset = Controller->GetClosestPointOnAxis(ray, worldOrigin, worldAxisDir);
}

void GizmoTranslationControlStrategy::UpdateDrag(const Ray& ray)
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
	FVector worldAxisPoint = Controller->GetRelativeMatrix().TransformPoint(axisDir);
	FVector worldAxisDir = (worldAxisPoint - worldOrigin).Normalize();

	float currentT = Controller->GetClosestPointOnAxis(ray, worldOrigin, worldAxisDir);
	float deltaT = currentT - InitialDragOffset;

	Controller->GetAttachedComponent()->SetRelativeLocation(Controller->GetAttachedComponent()->GetRelativeLocation() + worldAxisDir * deltaT);
}

void GizmoTranslationControlStrategy::EndDrag()
{
}

void GizmoTranslationControlStrategy::SetDrawEnable(bool bEnable)
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