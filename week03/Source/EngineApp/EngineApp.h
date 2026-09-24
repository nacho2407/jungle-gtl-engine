#pragma once

#include <windows.h>
#include <functional>
#include "EngineCore/EngineTypes.h"
#include "Editor/Picker.h"
class URenderer;
class Editor;
class UCameraComponent;
class UScene;
class EngineApp
{
public:
	EngineApp();
	~EngineApp();

public:
	bool Initialize(HINSTANCE hInstance);
	void Run();
	void Finalize();

private:
	/**
	 * @brief 일반 프레임의 업데이트 및 입력 처리를 수행합니다.
	 * 
	 *        화면 리사이징 시에도 화면이 정상적으로 업데이트 되도록하는 과정에서 기존 업데이트 함수를 Tick과 렌더로 나누었습니다.
	 *
	 * @param deltaTime 현재 프레임 델타 타임
	 * 
	 * @param bAllowPicking 피킹 입력 처리 여부
	 */
	void TickGameFrame(float deltaTime, bool bAllowPicking);

	/**
	 * @brief 현재 월드 상태를 화면에 렌더링합니다.
	 */
	void RenderCurrentFrame();

	/**
	 * @brief 크기 조정 중 최소 렌더 경로를 수행합니다.
	 */
	void RenderResizeFrame();

	void CreateEngineWindow();
	void ReleaseEngineWindow();
	void ProcessOnResizeCallback(int width, int height);

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND HWnd = nullptr;
	URenderer* Renderer = nullptr;
	Editor* EditorInst = nullptr;
	UPicker Picker;
	TArray<std::function<void(int width, int height)>> OnResizeCallback;

	UScene* LastActiveScene = nullptr;
	bool bIsResizing = false;
	static constexpr UINT ResizeRenderTimerId = 1;
};

