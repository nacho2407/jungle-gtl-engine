#pragma once

#include "Singleton.h"

struct Ray;
class UGizmoComponent;
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
	UGizmoComponent* GetController() const { return Controller; }

	void CreateGizmoController();

private:
	UGizmoComponent* Controller = nullptr;
};
