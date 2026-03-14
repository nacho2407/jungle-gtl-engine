#pragma once
#include "ISingleton.h"
#include "FGeometry.h"
#include "FMatrix.h"

struct Ray;
class URenderer;
class URenderObject;
class USceneComponent;
class GizmoController;

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
private:
	Gizmo();
	~Gizmo();

public:
	void Render(URenderer* renderer);

	GizmoController* GetController() const { return Controller; }

	void SetControllerTarget(USceneComponent* target);

	bool IsDragging() const { return bIsDragging; }

private:
	USceneComponent* AttachedComponent = nullptr;
	FMatrix RelativeMatrix;

	GizmoController* Controller = nullptr;

	bool bIsDragging = false;
	GizmoAxis CurrentAxis = GizmoAxis::None;

	FVector InitialObjectLocation;
	float InitialDragOffset;
};
