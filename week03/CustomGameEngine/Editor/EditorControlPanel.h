#pragma once

class UCameraComponent;
class Editor;

struct EditorControlPanel
{
	EditorControlPanel(Editor* editorDependency);
	~EditorControlPanel();

	void Draw(UCameraComponent* camera);

	Editor* editor;
};
