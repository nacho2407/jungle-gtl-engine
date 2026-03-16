#include "GizmoController.h"
#include "UObject.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "USceneComponent.h"
#include "URender.h"
#include "Logger.h"
#include "InputManager.h"
#include "UPicker.h"
#include "UEngineTypes.h"
#include "GizmoTranslationControlStrategy.h"
#include "GizmoRotationControlStrategy.h"
#include "GizmoScaleControlStrategy.h"

GizmoController::GizmoController()
{
	TranslationStrategy = new GizmoTranslationControlStrategy(this);
	RotationStrategy = new GizmoRotationControlStrategy(this);
	ScaleStrategy = new GizmoScaleControlStrategy(this);
	CurrentStrategy = TranslationStrategy;
}

GizmoController::~GizmoController()
{
}

void GizmoController::Update()
{
	if (AttachedComponent == nullptr) return;

	if (InputManager::GetInstance().IsKeyDown(VK_SPACE))
	{
		Type = static_cast<GizmoControllerType>((static_cast<int32>(Type) + 1) % static_cast<int32>(GizmoControllerType::End));
		SetControlStrategy(Type);
	}

	FVector scale = AttachedComponent->GetRelativeScale3D();
	FMatrix invScaleMatrix = FMatrix::ScaleMatrix(
		1.0f / scale.x,
		1.0f / scale.y,
		1.0f / scale.z);
	RelativeMatrix = invScaleMatrix * AttachedComponent->GetRelativeMatrix();
}

void GizmoController::UpdateRenderObject()
{
	if (AttachedComponent == nullptr) return;
	if (CurrentStrategy == nullptr) return;

	CurrentStrategy->UpdateRenderObject();

	return;
}

float GizmoController::GetClosestPointOnAxis(const Ray& ray, const FVector& axisOrigin, const FVector& axisDir)
{
	FVector u = ray.Direction;
	FVector v = axisDir;
	FVector w = ray.Origin - axisOrigin;

	float a = Dot(u, u);
	float b = Dot(u, v);
	float c = Dot(v, v);
	float d = Dot(u, w);
	float e = Dot(v, w);

	float denominator = a * c - b * b;
	if (denominator < 1e-6f) return 0.0f;

	return (a * e - b * d) / denominator;
}

void GizmoController::BeginDrag(const Ray& ray)
{
	if (IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	bDragging = true;

	CurrentStrategy->BeginDrag(ray);
}

void GizmoController::UpdateDrag(const Ray& ray)
{
	if (!IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	CurrentStrategy->UpdateDrag(ray);
}

void GizmoController::EndDrag()
{
	if (!IsDragging()) return;

	bDragging = false;

	CurrentStrategy->EndDrag();
}

void GizmoController::SetControlStrategy(GizmoControllerType type)
{
	Type = type;
	CurrentStrategy->SetDrawEnable(false);
	switch (Type)
	{
	case GizmoControllerType::Translation:
		CurrentStrategy = TranslationStrategy;
		break;
	case GizmoControllerType::Rotation:
		CurrentStrategy = RotationStrategy;
		break;
	case GizmoControllerType::Scale:
		CurrentStrategy = ScaleStrategy;
		break;
	}
	CurrentStrategy->SetDrawEnable(bDrawEnabled);
}

GizmoControllerAxis GizmoController::TryPick(Ray& ray)
{
	if (!bDrawEnabled)
	{
		return GizmoControllerAxis::None;
	}

	FString geometryNames[3];
	switch (Type)
	{
	case GizmoControllerType::Translation:
		geometryNames[0] = "GizmoTranslationX";
		geometryNames[1] = "GizmoTranslationY";
		geometryNames[2] = "GizmoTranslationZ";
		break;
	case GizmoControllerType::Rotation:
		geometryNames[0] = "GizmoRotationX";
		geometryNames[1] = "GizmoRotationY";
		geometryNames[2] = "GizmoRotationZ";
		break;
	case GizmoControllerType::Scale:
		geometryNames[0] = "GizmoScaleX";
		geometryNames[1] = "GizmoScaleY";
		geometryNames[2] = "GizmoScaleZ";
		break;
	}

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	bool bHit = false;
	float closestT = FLT_MAX;

	for (int i = 0; i < 3; ++i)
	{
		FGeometry& geometry = resourceManager->GetGeometry(geometryNames[i]);
		for (int j = 0; j < geometry.Vertices.size(); j += 3)
		{
			const FVector& v0 = geometry.Vertices[j];
			const FVector& v1 = geometry.Vertices[j + 1];
			const FVector& v2 = geometry.Vertices[j + 2];
			FVector worldV0 = RelativeMatrix.TransformPoint(v0);
			FVector worldV1 = RelativeMatrix.TransformPoint(v1);
			FVector worldV2 = RelativeMatrix.TransformPoint(v2);

			float t;

			if (ray.IntersectsTriangle(worldV0, worldV1, worldV2, t, ray))
			{
				if (t < closestT)
				{
					closestT = t;
					bHit = true;
				}
			}
		}

		if (bHit)
		{
			if (i == 0) return GizmoControllerAxis::X;
			else if (i == 1) return GizmoControllerAxis::Y;
			else if (i == 2) return GizmoControllerAxis::Z;
		}
	}
	return GizmoControllerAxis::None;
}
