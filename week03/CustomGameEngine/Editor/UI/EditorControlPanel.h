#pragma once

class UCameraComponent;
class Editor;
class UGridComp;

struct EditorControlPanel
{
	EditorControlPanel(Editor* parent);
	~EditorControlPanel();

	void Draw(UCameraComponent* camera, UGridComp* Gird);

	Editor* editor;
};
