#include "GizmoScaleControlStrategy.h"
#include "Component/GizmoComponent.h"
#include "ResourceManager.h"
#include "Object.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "Component/SceneComponent.h"
#include "Editor/Picker.h"
#include "Logger.h"

GizmoScaleControlStrategy::GizmoScaleControlStrategy(UGizmoComponent* controller)
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	GizmoX = new RenderObject();
	GizmoX->Geometry = &resourceManager->GetGeometry("GizmoScale");
	GizmoX->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoX->bDepthEnabled = false;
	GizmoX->Color = FColor(1.0f, 0.0f, 0.0f, 1.0f);

	GizmoY = new RenderObject();
	GizmoY->Geometry = &resourceManager->GetGeometry("GizmoScale");
	GizmoY->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoY->bDepthEnabled = false;
	GizmoY->Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);
	
	GizmoZ = new RenderObject();
	GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoScale");
	GizmoZ->Material = &resourceManager->GetMaterial(L"Shader/ShaderW0.hlsl");
	GizmoZ->bDepthEnabled = false;
	GizmoZ->Color = FColor(0.0f, 0.0f, 1.0f, 1.0f);

	Controller = controller;

	RelativeMatrices.resize(3);
	RelativeMatrices[0] = FMatrix::RotationYMatrix(90.0f) * FMatrix::TranslationMatrix(0.5f, 0.0f, 0.0f);
	RelativeMatrices[1] = FMatrix::RotationXMatrix(-90.0f) * FMatrix::TranslationMatrix(0.0f, 0.5f, 0.0f);
	RelativeMatrices[2] = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
}

GizmoScaleControlStrategy::~GizmoScaleControlStrategy()
{
	SetDrawEnable(false);
	delete GizmoX;
	delete GizmoY;
	delete GizmoZ;
}

void GizmoScaleControlStrategy::Update()
{
}

void GizmoScaleControlStrategy::UpdateRenderObject()
{
	GizmoX->World = RelativeMatrices[0] * Controller->GetRelativeMatrix();
	GizmoX->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::X;

	GizmoY->World = RelativeMatrices[1] * Controller->GetRelativeMatrix();
	GizmoY->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Y;

	GizmoZ->World = RelativeMatrices[2] * Controller->GetRelativeMatrix();
	GizmoZ->bIsSelected = Controller->GetHoverAxis() == GizmoControllerAxis::Z;
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

	if (Controller->GetHoverAxis() == GizmoControllerAxis::X) newScale.x += deltaT;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Y) newScale.y += deltaT;
	else if (Controller->GetHoverAxis() == GizmoControllerAxis::Z) newScale.z += deltaT;

	Controller->GetAttachedComponent()->SetRelativeScale3D(newScale);
}

void GizmoScaleControlStrategy::EndDrag()
{
}

void GizmoScaleControlStrategy::SetDrawEnable(bool bEnable)
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