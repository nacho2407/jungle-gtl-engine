#pragma once
#include "Editor/Editor.h"
#include "Component/PrimitiveComponent.h"	
#include "Component/SceneComponent.h"
#include "World.h"
#include "ImGuiLayout.h"

class Editor;
class USceneComponent;

struct EditorPropertyPanel
{
	EditorPropertyPanel(Editor* parent);
	~EditorPropertyPanel();

	void Draw(USceneComponent* selectedComponent, ImGuiWindowLayout& layout);
	void RemoveSelected(USceneComponent* selectedComponent);

private:
	Editor* editor;
};

