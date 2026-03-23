#pragma once
#include "Editor/Editor.h"
#include "SceneManager.h"
#include "World.h"
#include "Scene.h"
#include "Component/SceneComponent.h"

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