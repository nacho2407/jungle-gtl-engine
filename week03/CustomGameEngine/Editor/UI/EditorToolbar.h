#pragma once
#include "Editor/Gizmo.h"
#include "Component/GizmoComponent.h"
#include "Editor/Gizmo/GizmoRotationControlStrategy.h"
#include "ImGuiLayout.h"

struct EditorToolbar
{
	EditorToolbar();
	~EditorToolbar();

	void Draw(ImGuiWindowLayout& layout);
};