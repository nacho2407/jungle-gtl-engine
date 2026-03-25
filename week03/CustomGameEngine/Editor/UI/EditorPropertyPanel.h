#pragma once
#include <array>

#include "Editor/Editor.h"
#include "Component/BillboardSubUVComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "World.h"

class Editor;
class USceneComponent;

struct EditorPropertyPanel
{
	EditorPropertyPanel(Editor* parent);
	~EditorPropertyPanel();

	void Draw(USceneComponent* selectedComponent);
	void RemoveSelected();

private:
	void SyncNameBuffer(USceneComponent* selectedComponent);

	void DrawNameProperty(USceneComponent* selectedComponent);

	bool DrawTransformProperties(USceneComponent* selectedComponent);
	void DrawSubUVProperties(UBillboardSubUVComp* subUVComponent);

private:
	Editor* editor;
	uint32 LastSelectedUUID = 0;
	std::array<char, 256> NameBuffer{};
};
