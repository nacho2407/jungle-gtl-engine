#pragma once

#include "EngineTypes.h"
#include "Editor/Gizmo/GizmoControlStrategy.h"
#include "Math/Matrix.h"
#include "Editor/Picker.h"
#include "Component/SceneComponent.h"

enum class GizmoControllerAxis
{
	None,
	X,
	Y,
	Z,
};

enum class GizmoControllerType : int32
{
	Translation,
	Rotation,
	Scale,
	End
};

class URenderer;

class UGizmoComponent : public USceneComponent
{
	DECLARE_RTTI(UGizmoComponent, USceneComponent)
public:
	UGizmoComponent();
	virtual ~UGizmoComponent() override;

public:
	void Update(float deltaTime) override;
	void UpdateRenderObject();

	void BeginDrag(const Ray& ray);
	void UpdateDrag(const Ray& ray);
	void EndDrag();

	GizmoControllerType GetControlStrategyType() const { return Type; }
	IGizmoControlStrategy* GetControlStrategy() const { return CurrentStrategy; }
	void SetControlStrategy(GizmoControllerType type);

	bool IsLocalSpace() const { return bLocalSpace; }
	void SetLocalSpace(bool bLocal) { bLocalSpace = bLocal; }

	GizmoControllerAxis GetHoverAxis() const { return HoverAxis; }
	void SetHoverAxis(GizmoControllerAxis axis) { HoverAxis = axis; }
	bool IsDragging() const { return bDragging; }

	float GetClosestPointOnAxis(const Ray& ray, const FVector& axisOrigin, const FVector& axisDir);

	GizmoControllerAxis TryPick(Ray& ray);

	void AttachTo(USceneComponent* comp) {
		AttachedComponent = comp;

		if (comp)
		{
			SetDrawEnable(true);
		}
		else
		{
			SetDrawEnable(false);
		}
	}
	USceneComponent* GetAttachedComponent() const { return AttachedComponent; }

	void SetAxis(GizmoControllerAxis axis) { CurrentAxis = axis; }

	void SetDrawEnable(bool bEnable)
	{
		bDrawEnabled = bEnable;

		if (CurrentStrategy) CurrentStrategy->SetDrawEnable(bEnable);
	}

private:
	USceneComponent* AttachedComponent = nullptr;

	GizmoControllerType Type = GizmoControllerType::Translation;
	bool bLocalSpace = true;

	GizmoControllerAxis CurrentAxis = GizmoControllerAxis::None;
	GizmoControllerAxis HoverAxis = GizmoControllerAxis::None;
	bool bDragging = false;

	IGizmoControlStrategy* CurrentStrategy = nullptr;

	IGizmoControlStrategy* TranslationStrategy = nullptr;
	IGizmoControlStrategy* RotationStrategy = nullptr;
	IGizmoControlStrategy* ScaleStrategy = nullptr;

	float BaseScale = 0.07f;

	bool bDrawEnabled = false;
};

