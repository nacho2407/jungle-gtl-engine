#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"
#include "Math/Quaternion.h"

class GizmoController;
class RenderObject;

class GizmoRotationControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoRotationControlStrategy(GizmoController* controller);
	~GizmoRotationControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	GizmoController* Controller;

	FVector InitialTouchPoint;
	FQuat InitialObjectQuaternion;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};
