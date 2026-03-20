#pragma once
class UScene;

struct EditorSceneManagerPanel
{
	EditorSceneManagerPanel();
	~EditorSceneManagerPanel();

	void Draw();

private:
	UScene* ActiveScene = nullptr;
};