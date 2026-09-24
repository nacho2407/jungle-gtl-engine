#include "GizmoRotationControlStrategy.h"
#include "Component/GizmoComponent.h"
#include "EngineCore/ResourceManager.h"
#include "Component/Object.h"
#include "Renderer/Renderer.h"
#include "Renderer/Level.h"
#include "Scene.h"
#include "Component/SceneComponent.h"
#include "EngineCore/Logger.h"

GizmoRotationControlStrategy::GizmoRotationControlStrategy(UGizmoComponent* controller)
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	GizmoX = new RenderObject();
	GizmoX->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoX->Material = &resourceManager->GetMaterial(L"Shaders/ShaderW0.hlsl");
	GizmoX->bDepthEnabled = false;
	GizmoX->ShowFlag = EEngineShowFlags::SF_EditorPrimitives;
	GizmoX->Color = FColor(1.0f, 0.0f, 0.0f, 1.0f);

	GizmoY = new RenderObject();
	GizmoY->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoY->Material = &resourceManager->GetMaterial(L"Shaders/ShaderW0.hlsl");
	GizmoY->bDepthEnabled = false;
	GizmoY->ShowFlag = EEngineShowFlags::SF_EditorPrimitives;
	GizmoY->Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);

	GizmoZ = new RenderObject();
	GizmoZ->Geometry = &resourceManager->GetGeometry("GizmoRotation");
	GizmoZ->Material = &resourceManager->GetMaterial(L"Shaders/ShaderW0.hlsl");
	GizmoZ->bDepthEnabled = false;
	GizmoZ->ShowFlag = EEngineShowFlags::SF_EditorPrimitives;
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
	DragComponents = Controller->GetControlledComponents();
	if (DragComponents.empty() && Controller->GetAttachedComponent() != nullptr)
	{
		DragComponents.push_back(Controller->GetAttachedComponent());
	}

	InitialObjectQuaternions.clear();
	for (USceneComponent* component : DragComponents)
	{
		if (component == nullptr)
		{
			continue;
		}

		InitialObjectQuaternions.push_back(component->GetRelativeQuaternion());
	}
	AccumulatedAngle = 0.0f;
	GetCursorPos(&InitialCursorPos);
	ShowCursor(false);
}

void GizmoRotationControlStrategy::UpdateDrag(const Ray& ray)
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

	POINT currPos;
	GetCursorPos(&currPos);

	int deltaX = currPos.x - InitialCursorPos.x;
	int deltaY = currPos.y - InitialCursorPos.y;

	UCameraComponent* camera = UCameraComponent::GetMainCamera();
	FVector screenOrigin = camera->WorldToScreen(Controller->GetAttachedComponent()->GetRelativeLocation());
	FVector screenAxisEnd = camera->WorldToScreen(Controller->GetAttachedComponent()->GetRelativeLocation() + axisDir);
	FVector screenAxisVector = (screenAxisEnd - screenOrigin).Normalize();

	FVector screenTangent = FVector(-screenAxisVector.y, screenAxisVector.x, 0.0f);

	if (deltaX == 0 && deltaY == 0) return;

	FQuat deltaQuat;

	float sensitivity = 0.01f;
	FVector mouseDeltaVec = FVector((float)deltaX, (float)deltaY, 0.0f);
	float effectiveDelta = mouseDeltaVec.Dot(screenTangent);

	float deltaAngle = -effectiveDelta * sensitivity;

	AccumulatedAngle += deltaAngle;

	float applyAngle = AccumulatedAngle;
	if (bEnableSnap)
	{
		float snapStepRad = SnapStep * (MathHelper::PI / 180.0f);
		applyAngle = roundf(AccumulatedAngle / snapStepRad) * snapStepRad;
	}

	deltaQuat = FQuat::FromAxisAngle(worldAxis, applyAngle);

	for (int i = 0; i < DragComponents.size() && i < InitialObjectQuaternions.size(); ++i)
	{
		USceneComponent* component = DragComponents[i];
		if (component == nullptr)
		{
			continue;
		}

		FQuat currQuat = InitialObjectQuaternions[i];
		if (Controller->IsLocalSpace())
		{
			currQuat = currQuat * deltaQuat;
		}
		else
		{
			currQuat = deltaQuat * currQuat;
		}

		component->SetRelativeQuaternion(currQuat);
	}

	SetCursorPos(InitialCursorPos.x, InitialCursorPos.y);
}

void GizmoRotationControlStrategy::EndDrag()
{
	ShowCursor(true);
}

void GizmoRotationControlStrategy::SetDrawEnable(bool bEnable)
{
	if (Controller == nullptr || Controller->Scene == nullptr) return;

	if (bEnable)
	{
		Controller->Scene->Level.RegisterRenderObject(GizmoX);
		Controller->Scene->Level.RegisterRenderObject(GizmoY);
		Controller->Scene->Level.RegisterRenderObject(GizmoZ);
	}
	else
	{
		Controller->Scene->Level.UnregisterRenderObject(GizmoX);
		Controller->Scene->Level.UnregisterRenderObject(GizmoY);
		Controller->Scene->Level.UnregisterRenderObject(GizmoZ);
	}
}
