#pragma once

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

