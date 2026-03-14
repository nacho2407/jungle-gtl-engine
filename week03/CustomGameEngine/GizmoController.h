#pragma once
#include "UEngineTypes.h"
#include "FMatrix.h"
#include "UPicker.h"

enum class GizmoControllerAxis
{
	None,
	X,
	Y,
	Z,
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

	void SetHoverAxis(GizmoControllerAxis axis) { HoverAxis = axis; }
	bool IsDragging() const { return bDragging; }

	void BeginDrag(const Ray& ray);
	void UpdateDrag(const Ray& ray);
	void EndDrag();

	void AttachTo(USceneComponent* comp) { AttachedComponent = comp; }
	USceneComponent* GetAttachedComponent() const { return AttachedComponent; }

	FMatrix GetRelativeMatrix() const { return RelativeMatrix; }

	void SetAxis(GizmoControllerAxis axis) { CurrentAxis = axis; }

private:
	FMatrix RelativeMatrix;

	USceneComponent* AttachedComponent = nullptr;

	GizmoControllerAxis CurrentAxis = GizmoControllerAxis::None;
	GizmoControllerAxis HoverAxis = GizmoControllerAxis::None;
	bool bDragging = false;

	FVector InitialObjectLocation;
	float InitialDragOffset = 0.0f;

	RenderObject* GizmoTranslationX;
	RenderObject* GizmoTranslationY;
	RenderObject* GizmoTranslationZ;
	RenderObject* GizmoRotationX;
	RenderObject* GizmoRotationY;
	RenderObject* GizmoRotationZ;
	RenderObject* GizmoScaleX;
	RenderObject* GizmoScaleY;
	RenderObject* GizmoScaleZ;
};

