#pragma once
class UScene;

struct EditorSceneManagerPanel
{
public:
	EditorSceneManagerPanel();
	~EditorSceneManagerPanel();

	void Draw();

private:
	UScene* ActiveScene = nullptr;

};