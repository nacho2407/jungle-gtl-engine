#include "GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "EngineCore/EngineTypes.h"
#include "Editor/Gizmo/GizmoTranslationControlStrategy.h"
#include "Editor/Gizmo/GizmoRotationControlStrategy.h"
#include "Editor/Gizmo/GizmoScaleControlStrategy.h"
#include "EngineCore/InputManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderObject.h"
#include "EngineCore/ResourceManager.h"

#include "ImGui/imgui.h"

UGizmoComponent::UGizmoComponent()
{
	TranslationStrategy = new GizmoTranslationControlStrategy(this);
	RotationStrategy = new GizmoRotationControlStrategy(this);
	ScaleStrategy = new GizmoScaleControlStrategy(this);
	CurrentStrategy = TranslationStrategy;
}

UGizmoComponent::~UGizmoComponent()
{
	delete ScaleStrategy;
	delete RotationStrategy;
	delete TranslationStrategy;
}

void UGizmoComponent::Update(float deltaTime)
{
	if (AttachedComponent == nullptr) return;

	UCameraComponent* camera = UCameraComponent::GetMainCamera();

	FVector mousePos = InputManager::GetInstance().MousePos;
	Ray ray = camera->ScreenPointToRay(mousePos.x, mousePos.y);

	if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON))
	{
		if (IsDragging())
		{
			UpdateDrag(ray);
		}
	}

	if (InputManager::GetInstance().IsKeyUp(VK_LBUTTON))
	{
		if (IsDragging())
		{
			EndDrag();
		}
	}

	if (!InputManager::GetInstance().IsKeyHold(VK_LBUTTON))
	{
		GizmoControllerAxis hoveredAxis = TryPick(ray);
		SetHoverAxis(hoveredAxis);
	}

	// Object 이름 변경 등 텍스트 입력 중에는 스페이스바를 눌러도 기즈모 모드가 바뀌지 않도록 합니다.
	bool bIsEditingText = ImGui::GetIO().WantTextInput;

	if (!bIsEditingText && InputManager::GetInstance().IsKeyDown(VK_SPACE))
	{
		Type = static_cast<GizmoControllerType>((static_cast<int32>(Type) + 1) % static_cast<int32>(GizmoControllerType::End));
		SetControlStrategy(Type);
	}

	SetRelativeLocation(AttachedComponent->GetRelativeLocation());
	SetRelativeQuaternion(AttachedComponent->GetRelativeQuaternion());

	float Scale = BaseScale;
	if (!camera->IsOrthogonal)
	{
		FVector CamPos = camera->GetRelativeLocation();
		FVector CompPos = AttachedComponent->GetRelativeLocation();
		float Dist = (CamPos - CompPos).Length();
		Scale *= Dist;
	}
	else
	{
		Scale *= 15.0f;
	}

	SetRelativeScale3D(FVector(Scale, Scale, Scale));

	if (!bLocalSpace && Type != GizmoControllerType::Scale)
	{
		SetRelativeQuaternion(FQuat());
	}

	CurrentStrategy->Update();

	UpdateRenderObject();
}

void UGizmoComponent::UpdateRenderObject()
{
	if (AttachedComponent == nullptr) return;
	if (CurrentStrategy == nullptr) return;

	CurrentStrategy->UpdateRenderObject();

	return;
}

float UGizmoComponent::GetClosestPointOnAxis(const Ray& ray, const FVector& axisOrigin, const FVector& axisDir)
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

void UGizmoComponent::BeginDrag(const Ray& ray)
{
	if (IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	bDragging = true;

	CurrentStrategy->BeginDrag(ray);
}

void UGizmoComponent::UpdateDrag(const Ray& ray)
{
	if (!IsDragging()) return;
	if (AttachedComponent == nullptr) return;

	CurrentStrategy->UpdateDrag(ray);
}

void UGizmoComponent::EndDrag()
{
	if (!IsDragging()) return;

	bDragging = false;

	CurrentStrategy->EndDrag();
}

void UGizmoComponent::SetControlStrategy(GizmoControllerType type)
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

GizmoControllerAxis UGizmoComponent::TryPick(Ray& ray)
{
	if (!bDrawEnabled)
	{
		return GizmoControllerAxis::None;
	}

	TArray<FMatrix> relativeMatrices = CurrentStrategy->GetRelativeMatrices(); // x, y, z: length is 3

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	bool bHit = false;
	float closestT = FLT_MAX;
	GizmoControllerAxis selectedAxis = GizmoControllerAxis::None;

	for (int i = 0; i < 3; ++i)
	{
		Ray localRay;
		localRay.Direction = (relativeMatrices[i] * GetRelativeMatrix()).Inverse().TransformVector(ray.Direction).Normalize();
		localRay.Origin = (relativeMatrices[i] * GetRelativeMatrix()).Inverse().TransformPoint(ray.Origin);

		FGeometry& geometry = resourceManager->GetGeometry(CurrentStrategy->GetGeometryName());
		
		bool bAxisHit = false;

		if (!geometry.Indices.empty())
		{
			for (size_t j = 0; j + 2 < geometry.Indices.size(); j += 3)
			{
				const FVector& v0 = geometry.Vertices[geometry.Indices[j]];
				const FVector& v1 = geometry.Vertices[geometry.Indices[j + 1]];
				const FVector& v2 = geometry.Vertices[geometry.Indices[j + 2]];

				float t;
				if (ray.IntersectsTriangle(v0, v1, v2, t, localRay))
				{
					if (t < closestT)
					{
						closestT = t;
						bAxisHit = true;
						bHit = true;
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j + 2 < geometry.Vertices.size(); j += 3)
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
						bAxisHit = true;
						bHit = true;
					}
				}
			}
		}

		if (bAxisHit)
		{
			if (i == 0) selectedAxis = GizmoControllerAxis::X;
			else if (i == 1) selectedAxis = GizmoControllerAxis::Y;
			else if (i == 2) selectedAxis = GizmoControllerAxis::Z;
		}
	}
	return selectedAxis;
}

REGISTER_CLASS(UGizmoComponent)