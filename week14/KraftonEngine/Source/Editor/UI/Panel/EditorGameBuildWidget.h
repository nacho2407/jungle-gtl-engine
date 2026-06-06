#pragma once

#include "Editor/Build/EditorGameBuilder.h"

class UEditorEngine;

/**
 * @brief 에디터 game build 창
 */
class FEditorGameBuildWidget
{
public:
	/**
	 * @brief editor engine 참조를 설정합니다
	 *
	 * @param InEditorEngine build 전 PIE 종료와 저장에 사용할 editor engine
	 */
	void Initialize(UEditorEngine* InEditorEngine);

	/**
	 * @brief worker thread를 종료합니다
	 */
	void Shutdown();

	void Render(float DeltaTime);
	void Open() { bOpen = true; }

private:
	void StartBuildFromUI();

private:
	UEditorEngine* EditorEngine = nullptr;
	FEditorGameBuilder Builder;
	FEditorGameBuildSettings Settings;
	bool bOpen = false;
};
