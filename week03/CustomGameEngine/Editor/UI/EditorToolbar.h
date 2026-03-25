#pragma once
#include "Editor/Gizmo.h"
#include "Component/GizmoComponent.h"
#include "Editor/Gizmo/GizmoRotationControlStrategy.h"
#include "Component/GridComp.h"
#include "EngineCore/ResourceManager.h"
#include "ImGui/imgui.h"
#include "Editor/Editor.h"

class UGridComp;

struct EditorToolbar
{
	EditorToolbar(Editor* parent);
	~EditorToolbar();

	Editor* editor;
	void Draw(UGridComp* Grid);
};