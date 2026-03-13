#pragma once

class USceneComponent;

struct EditorPropertyPanel
{
	EditorPropertyPanel();
	~EditorPropertyPanel();

	void Draw(USceneComponent* selectedComponent);
};

