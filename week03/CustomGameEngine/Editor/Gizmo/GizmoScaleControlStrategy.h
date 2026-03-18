#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"

class UGizmoComponent;
struct RenderObject;

class GizmoScaleControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoScaleControlStrategy(UGizmoComponent* controller);
	~GizmoScaleControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual TArray<FMatrix> GetRelativeMatrices() override { return RelativeMatrices; }
	virtual FString GetGeometryName() override { return "GizmoScale"; }

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	UGizmoComponent* Controller;

	TArray<FMatrix> RelativeMatrices;

	FVector InitialWorldPos;
	FVector InitialObjectScale;
	float InitialDragOffset = 0.0f;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};

