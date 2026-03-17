#pragma once

#include "Singleton.h"

struct Ray;
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
