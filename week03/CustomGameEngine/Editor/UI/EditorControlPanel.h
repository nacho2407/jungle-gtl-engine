#pragma once

class UCameraComponent;
class Editor;
class UGridComp;
class URenderer;
struct EditorControlPanel
{
	EditorControlPanel(Editor* parent, URenderer* Renderer);
	~EditorControlPanel();

	void Draw(UCameraComponent* camera, UGridComp* Gird);

	Editor* editor;
	URenderer* Renderer;
};
