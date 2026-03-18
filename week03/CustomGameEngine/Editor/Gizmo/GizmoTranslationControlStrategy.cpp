#include "GizmoTranslationControlStrategy.h"
#include "ResourceManager.h"
#include "Object.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "Logger.h"

GizmoTranslationControlStrategy::GizmoTranslationControlStrategy(UGizmoComponent* controller)
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	GizmoX = new RenderObject();
	GizmoX->Geometry = &resourceManager->GetGeometry("GizmoTranslation");
	GizmoX->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoX->bDepthEnabled = false;
	GizmoX->Color = FColor(1.0f, 0.0f, 0.0f, 1.0f);

	GizmoY = new RenderObject();
	GizmoY->Geometry = &resourceManager->GetGeometry("GizmoTranslation");
	GizmoY->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoY->bDepthEnabled = false;
	GizmoY->Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);
	
	GizmoZ = new RenderObject();
	GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoTranslation");
	GizmoZ->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoZ->bDepthEnabled = false;
	GizmoZ->Color = FColor(0.0f, 0.0f, 1.0f, 1.0f);

	Controller = controller;

	RelativeMatrices.resize(3);
	RelativeMatrices[0] = FMatrix::RotationYMatrix(90.0f) * FMatrix::TranslationMatrix(0.5f, 0.0f, 0.0f);
	RelativeMatrices[1] = FMatrix::RotationXMatrix(-90.0f) * FMatrix::TranslationMatrix(0.0f, 0.5f, 0.0f);
	RelativeMatrices[2] = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
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
}

void GizmoTranslationControlStrategy::UpdateRenderObject()
{

	GizmoX->World = RelativeMatrices[0] * Controller->GetRelativeMatrix();
	GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;

	GizmoY->World = RelativeMatrices[1] * Controller->GetRelativeMatrix();
	GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;

	GizmoZ->World = RelativeMatrices[2] * Controller->GetRelativeMatrix();
	GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
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
		FLevel::GetInstance().RegisterRenderObject(GizmoX);
		FLevel::GetInstance().RegisterRenderObject(GizmoY);
		FLevel::GetInstance().RegisterRenderObject(GizmoZ);
	}
	else
	{
		FLevel::GetInstance().UnregisterRenderObject(GizmoX);
		FLevel::GetInstance().UnregisterRenderObject(GizmoY);
		FLevel::GetInstance().UnregisterRenderObject(GizmoZ);
	}
}