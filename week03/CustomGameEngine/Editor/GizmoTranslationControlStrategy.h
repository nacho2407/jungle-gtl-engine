#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"

class GizmoController;
class RenderObject;

class GizmoTranslationControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoTranslationControlStrategy(GizmoController* controller);
	~GizmoTranslationControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	GizmoController* Controller;

	FVector InitialObjectLocation;
	FVector InitialWorldPos;
	float InitialDragOffset = 0.0f;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};

