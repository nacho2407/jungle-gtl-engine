#pragma once

#include "Singleton.h"

struct Ray;
class GizmoController;
class UCameraComponent;

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
	void OnBeforeRender(UCameraComponent* Camera);

	GizmoController* GetController() const { return Controller; }

private:
	GizmoController* Controller = nullptr;
};
