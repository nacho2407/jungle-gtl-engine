#pragma once
#include "ISingleton.h"
#include "FGeometry.h"
#include "FMatrix.h"

struct Ray;
class URenderer;
class RenderObject;
class USceneComponent;
class GizmoController;

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
	static void Initialize();

	void UpdateRenderObject();

public:
	void OnBeforeRender();

	GizmoController* GetController() const { return Controller; }

private:
	GizmoController* Controller = nullptr;
};
