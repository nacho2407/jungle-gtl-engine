#include "GizmoTranslationControlStrategy.h"
#include "ResourceManager.h"
#include "Object.h"
#include "GizmoController.h"
#include "Component/SceneComponent.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "Logger.h"

GizmoTranslationControlStrategy::GizmoTranslationControlStrategy(GizmoController* controller)
{
	GizmoX = new RenderObject();
	GizmoY = new RenderObject();
	GizmoZ = new RenderObject();

	Controller = controller;
}

GizmoTranslationControlStrategy::~GizmoTranslationControlStrategy()
{
	SetDrawEnable(false);
	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoTranslationControlStrategy::Update()
{
	// No specific update logic for translation control
}

void GizmoTranslationControlStrategy::UpdateRenderObject()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	USceneComponent* attachedComponent = Controller->GetAttachedComponent();

	{
		GizmoX->Geometry = &resourceManager->GetGeometry("GizmoTranslationX");
		GizmoX->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoX->World = Controller->GetRelativeMatrix();
		GizmoX->bDepthEnabled = false;

		GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;
	}
	{
		GizmoY->Geometry = &resourceManager->GetGeometry("GizmoTranslationY");
		GizmoY->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoY->World = Controller->GetRelativeMatrix();
		GizmoY->bDepthEnabled = false;

		GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;
	}
	{
		GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoTranslationZ");
		GizmoZ->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");

		GizmoZ->World = Controller->GetRelativeMatrix();
		GizmoZ->bDepthEnabled = false;

		GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
	}
}

void GizmoTranslationControlStrategy::BeginDrag(const Ray& ray)
{
	InitialObjectLocation = Controller->GetAttachedComponent()->GetRelativeLocation();

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

	FVector axisDir = worldAxis;
	if (Controller->IsLocalSpace())
	{
		axisDir = Controller->GetRelativeMatrix().TransformVector(worldAxis).Normalize();
	}
	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector::Zero());

	InitialWorldPos = worldOrigin;
	InitialDragOffset = Controller->GetClosestPointOnAxis(ray, worldOrigin, axisDir);
}

void GizmoTranslationControlStrategy::UpdateDrag(const Ray& ray)
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

	FVector axisDir = worldAxis;
	if (Controller->IsLocalSpace())
	{
		axisDir = Controller->GetRelativeMatrix().TransformVector(worldAxis).Normalize();
	}
	FVector worldOrigin = Controller->GetRelativeMatrix().TransformPoint(FVector(0, 0, 0));

	float currentT = Controller->GetClosestPointOnAxis(ray, InitialWorldPos, axisDir);
	float deltaT = currentT - InitialDragOffset;

	FVector newPos = InitialObjectLocation + axisDir * deltaT;
	Controller->GetAttachedComponent()->SetRelativeLocation(newPos);
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