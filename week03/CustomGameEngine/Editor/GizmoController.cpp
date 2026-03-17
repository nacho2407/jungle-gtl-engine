#include "GizmoController.h"
#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "EngineTypes.h"
#include "GizmoRotationControlStrategy.h"
#include "GizmoScaleControlStrategy.h"
#include "GizmoTranslationControlStrategy.h"
#include "InputManager.h"
#include "Logger.h"
#include "Object.h"
#include "Picker.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "ResourceManager.h"

GizmoController::GizmoController()
{
	TranslationStrategy = new GizmoTranslationControlStrategy(this);
	RotationStrategy = new GizmoRotationControlStrategy(this);
	ScaleStrategy = new GizmoScaleControlStrategy(this);
	CurrentStrategy = TranslationStrategy;
}

GizmoController::~GizmoController()
{
	delete ScaleStrategy;
	delete RotationStrategy;
	delete TranslationStrategy;
}

void GizmoController::Update(UCameraComponent* Camera)
{
	if (AttachedComponent == nullptr) return;

	if (InputManager::GetInstance().IsKeyDown(VK_SPACE))
	{
		Type = static_cast<GizmoControllerType>((static_cast<int32>(Type) + 1) % static_cast<int32>(GizmoControllerType::End));
		SetControlStrategy(Type);
	}

	FVector CamPos = Camera->GetRelativeLocation();
	FVector CompPos = AttachedComponent->GetRelativeLocation();
	float Dist = (CamPos - CompPos).Length();
	float Scale = Dist * BaseScale;
	RelativeMatrix = FMatrix::ScaleMatrix(Scale, Scale, Scale) * FMatrix::TranslationMatrix(AttachedComponent->GetRelativeLocation());

	//RelativeMatrix = FMatrix::TranslationMatrix(AttachedComponent->GetRelativeLocation());

	if (bLocalSpace)
	{
		FMatrix rotMatrix = AttachedComponent->GetRelativeQuaternion().ToMatrix();
		RelativeMatrix = rotMatrix * RelativeMatrix;
	}
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

	TArray<FMatrix> relativeMatrices = CurrentStrategy->GetRelativeMatrices(); // x, y, z: length is 3

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	bool bHit = false;
	float closestT = FLT_MAX;

	for (int i = 0; i < 3; ++i)
	{
		Ray localRay;
		localRay.Direction = (relativeMatrices[i] * RelativeMatrix).Inverse().TransformVector(ray.Direction).Normalize();
		localRay.Origin = (relativeMatrices[i] * RelativeMatrix).Inverse().TransformPoint(ray.Origin);

		FGeometry& geometry = resourceManager->GetGeometry(CurrentStrategy->GetGeometryName());
		for (int j = 0; j < geometry.Vertices.size(); j += 3)
		{
			const FVector& v0 = geometry.Vertices[j];
			const FVector& v1 = geometry.Vertices[j + 1];
			const FVector& v2 = geometry.Vertices[j + 2];

			float t;

			if (ray.IntersectsTriangle(v0, v1, v2, t, localRay))
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
