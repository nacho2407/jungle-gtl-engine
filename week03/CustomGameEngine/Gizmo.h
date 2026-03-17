#pragma once
#include "Singleton.h"
#include "Geometry.h"
#include "Matrix.h"

struct Ray;
class URenderer;
class RenderObject;
class USceneComponent;
class GizmoController;
class UCamera;
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
	void UpdateRenderObject();

public:
	void OnBeforeRender(UCamera* Camera);

	GizmoController* GetController() const { return Controller; }

private:
	GizmoController* Controller = nullptr;
};
