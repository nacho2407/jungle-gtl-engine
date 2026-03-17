#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"

class GizmoController;
class RenderObject;

class GizmoScaleControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoScaleControlStrategy(GizmoController* controller);
	~GizmoScaleControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	GizmoController* Controller;

	FVector InitialWorldPos;
	FVector InitialObjectScale;
	float InitialDragOffset = 0.0f;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};

