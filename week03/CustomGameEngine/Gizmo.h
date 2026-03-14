#pragma once
#include "ISingleton.h"
#include "FMatrix.h"

class URenderObject;
class USceneComponent;
struct Ray;

enum class GizmoAxis
{
	None,
	X,
	Y,
	Z
};

enum class TransformationType
{
	Location,
	Rotation,
	Scale
};

class Gizmo : public ISingleton<Gizmo>
{
	friend class ISingleton<Gizmo>;
public:
	URenderObject* DrawGizmo(USceneComponent* target, TransformationType type);

	USceneComponent* GetAttachedComponent() const { return AttachedComponent; }
	void AttachToComponent(USceneComponent* target) { AttachedComponent = target; }

	FMatrix GetRelativeMatrix() const { return RelativeMatrix; }

	void BeginDrag(GizmoAxis axis, const Ray& mouseRay);
	void UpdateDrag(const Ray& currentMouseRay);
	void EndDrag();
	bool IsDragging() const { return bIsDragging; }

private:
	URenderObject* DrawLocationGizmo(USceneComponent* target);
	URenderObject* DrawRotationGizmo(USceneComponent* target);
	URenderObject* DrawScaleGizmo(USceneComponent* target);

private:
	USceneComponent* AttachedComponent = nullptr;
	FMatrix RelativeMatrix;

	bool bIsDragging = false;
	GizmoAxis CurrentAxis = GizmoAxis::None;

	FVector InitialObjectLocation;
	float InitialDragOffset;
};
