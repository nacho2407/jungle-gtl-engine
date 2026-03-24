#pragma once
#include <array>

#include "Editor/Editor.h"
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
	/**
	 * @brief 선택된 오브젝트의 이름 편집 버퍼를 동기화합니다.
	 *
	 * @param selectedComponent 현재 선택된 scene component
	 */
	void SyncNameBuffer(USceneComponent* selectedComponent);

	/**
	 * @brief 선택된 오브젝트의 이름 UI를 그립니다.
	 *
	 * @param selectedComponent 현재 선택된 scene component
	 */
	void DrawNameProperty(USceneComponent* selectedComponent);

	/**
	 * @brief 선택된 오브젝트의 transform UI를 그립니다.
	 *
	 * @param selectedComponent 현재 선택된 scene component
	 * @return transform 값이 변경되었는지 여부
	 */
	bool DrawTransformProperties(USceneComponent* selectedComponent);

private:
	Editor* editor;
	uint32 LastSelectedUUID = 0;
	std::array<char, 256> NameBuffer{};
};
