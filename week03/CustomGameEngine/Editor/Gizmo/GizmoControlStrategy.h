#pragma once

#include "EngineTypes.h"

struct Ray;
struct FMatrix;

class IGizmoControlStrategy
{
public:
	virtual ~IGizmoControlStrategy() = default;

	virtual void Update() = 0;
	virtual void UpdateRenderObject() = 0;

	virtual TArray<FMatrix> GetRelativeMatrices() = 0;
	virtual FString GetGeometryName() = 0;

	virtual void BeginDrag(const Ray& ray) = 0;
	virtual void UpdateDrag(const Ray& ray) = 0;
	virtual void EndDrag() = 0;

	virtual void SetDrawEnable(bool bEnable) = 0;
};

