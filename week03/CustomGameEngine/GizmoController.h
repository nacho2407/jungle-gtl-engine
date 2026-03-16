#pragma once
#include "UEngineTypes.h"
#include "FMatrix.h"
#include "UPicker.h"
#include "IGizmoControlStrategy.h"

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
class USceneComponent;

class GizmoController
{
public:
	GizmoController();
	~GizmoController();

public:
	void Update();
	void UpdateRenderObject();

	void BeginDrag(const Ray& ray);
	void UpdateDrag(const Ray& ray);
	void EndDrag();

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

	FMatrix GetRelativeMatrix() const { return RelativeMatrix; }

	void SetAxis(GizmoControllerAxis axis) { CurrentAxis = axis; }

	void SetDrawEnable(bool bEnable)
	{
		bDrawEnabled = bEnable;

		if (CurrentStrategy) CurrentStrategy->SetDrawEnable(bEnable);
	}

private:
	FMatrix RelativeMatrix;

	USceneComponent* AttachedComponent = nullptr;

	GizmoControllerType Type = GizmoControllerType::Translation;

	GizmoControllerAxis CurrentAxis = GizmoControllerAxis::None;
	GizmoControllerAxis HoverAxis = GizmoControllerAxis::None;
	bool bDragging = false;

	IGizmoControlStrategy* CurrentStrategy = nullptr;

	IGizmoControlStrategy* TranslationStrategy = nullptr;
	IGizmoControlStrategy* RotationStrategy = nullptr;
	IGizmoControlStrategy* ScaleStrategy = nullptr;
	FVector InitialObjectLocation;
	float InitialDragOffset = 0.0f;

	bool bDrawEnabled = false;
};

