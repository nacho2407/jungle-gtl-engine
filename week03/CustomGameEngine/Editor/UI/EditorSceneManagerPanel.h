#pragma once
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