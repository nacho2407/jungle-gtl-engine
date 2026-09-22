#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"
#include "Math/Quaternion.h"
#include "Math/Matrix.h"

class UGizmoComponent;
class USceneComponent;
struct RenderObject;

class GizmoRotationControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoRotationControlStrategy(UGizmoComponent* controller);
	~GizmoRotationControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual TArray<FMatrix> GetRelativeMatrices() override { return RelativeMatrices; }
	virtual FString GetGeometryName() override { return "GizmoRotation"; }

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

	bool bEnableSnap = true;

private:
	UGizmoComponent* Controller;
	TArray<USceneComponent*> DragComponents;

	FMatrix BaseMatrix;
	TArray<FMatrix> RelativeMatrices;

	POINT InitialCursorPos;
	TArray<FQuat> InitialObjectQuaternions;

	float AccumulatedAngle = 0.0f;
	float SnapStep = 15.0f;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};
