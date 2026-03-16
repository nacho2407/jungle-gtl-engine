#pragma once

class UCamera;
class Editor;

struct EditorControlPanel
{
	EditorControlPanel(Editor* editorDependency);
	~EditorControlPanel();

	void Draw(UCamera* camera);

	Editor* editor;
};
