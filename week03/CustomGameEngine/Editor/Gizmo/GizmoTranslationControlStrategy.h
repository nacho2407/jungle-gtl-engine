#pragma once

#include "GizmoControlStrategy.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"

class UGizmoComponent;
struct RenderObject;

class GizmoTranslationControlStrategy : public IGizmoControlStrategy
{
public:
	GizmoTranslationControlStrategy(UGizmoComponent* controller);
	~GizmoTranslationControlStrategy() override;

public:
	virtual void Update() override;
	virtual void UpdateRenderObject() override;

	virtual TArray<FMatrix> GetRelativeMatrices() override { return RelativeMatrices; }
	virtual FString GetGeometryName() override { return "GizmoTranslation"; }

	virtual void BeginDrag(const Ray& ray) override;
	virtual void UpdateDrag(const Ray& ray) override;
	virtual void EndDrag() override;

	virtual void SetDrawEnable(bool bEnable) override;

private:
	UGizmoComponent* Controller;

	TArray<FMatrix> RelativeMatrices;

	FVector InitialObjectLocation;
	FVector InitialWorldPos;
	float InitialDragOffset = 0.0f;

	RenderObject* GizmoX;
	RenderObject* GizmoY;
	RenderObject* GizmoZ;
};

