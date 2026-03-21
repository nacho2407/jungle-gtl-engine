#pragma once
#include "World.h"
#include "Editor/Editor.h"
#include "Scene.h"
#include "Component/SceneComponent.h"
#include "SceneManager.h"

class UScene;
class Editor;

struct EditorSceneManagerPanel
{
public:
	EditorSceneManagerPanel();
	~EditorSceneManagerPanel();

	void Draw(Editor* editor);

private:
	UScene* ActiveScene = nullptr;

};