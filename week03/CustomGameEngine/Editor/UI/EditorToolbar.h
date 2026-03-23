#pragma once
#include "Editor/Gizmo.h"
#include "Component/GizmoComponent.h"
#include "Editor/Gizmo/GizmoRotationControlStrategy.h"

class UGridComp;

struct EditorToolbar
{
	EditorToolbar();
	~EditorToolbar();

	void Draw(UGridComp* Grid);
};