#pragma once
#include "IGizmoControlStrategy.h"
#include "FVector.h"

class GizmoController;
class RenderObject;

class GizmoRotationControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoRotationControlStrategy(GizmoController* controller);
	~GizmoRotationControlStrategy();

public:
	virtual void Update() override;
	virtual void Draw() override;

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	GizmoController* Controller;

	FVector InitialTouchPoint;
	FVector InitialObjectRotation;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};
