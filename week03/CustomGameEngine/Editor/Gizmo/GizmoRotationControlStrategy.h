#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"
#include "Math/Quaternion.h"

class UGizmoComponent;
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

private:
	UGizmoComponent* Controller;

	TArray<FMatrix> RelativeMatrices;

	FVector InitialTouchPoint;
	FQuat InitialObjectQuaternion;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};
