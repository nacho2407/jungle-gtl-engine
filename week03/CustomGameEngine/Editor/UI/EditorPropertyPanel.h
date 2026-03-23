#pragma once
#include "Editor/Editor.h"
#include "Component/PrimitiveComponent.h"	
#include "Component/SceneComponent.h"
#include "World.h"

class Editor;
class USceneComponent;

struct EditorPropertyPanel
{
	EditorPropertyPanel(Editor* parent);
	~EditorPropertyPanel();

	void Draw(USceneComponent* selectedComponent);
	void RemoveSelected(USceneComponent* selectedComponent);

private:
	Editor* editor;
};

