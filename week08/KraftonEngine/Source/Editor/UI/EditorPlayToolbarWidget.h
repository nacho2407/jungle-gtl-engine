#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
class UEditorEngine;

// 뷰포트 상단 Play/Stop 툴바. FLevelViewportLayout이 소유하며,
// 뷰포트 윈도우 상단 고정 높이만큼의 영역을 차지해 렌더된다.
class FEditorPlayToolbarWidget
{
public:
	FEditorPlayToolbarWidget() = default;
	~FEditorPlayToolbarWidget() = default;

	void Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice);
	void Release();

	// 레이아웃이 예약해야 할 툴바 높이 (패딩 포함).
	float GetDesiredHeight() const { return ToolbarHeight; }

	// 지정 영역(ImGui Cursor 위치 기준)에 Play/Stop 버튼을 렌더.
	// 호출 전에 ImGui::SetCursorScreenPos로 원하는 위치를 지정하고 호출하면 된다.
	void Render(float Width);

private:
	UEditorEngine* Editor = nullptr;
	ID3D11ShaderResourceView* PlayIcon = nullptr;
	ID3D11ShaderResourceView* StopIcon = nullptr;

	float ToolbarHeight = 28.0f;
	float IconSize = 16.0f;
	float ButtonSpacing = 4.0f;
};
