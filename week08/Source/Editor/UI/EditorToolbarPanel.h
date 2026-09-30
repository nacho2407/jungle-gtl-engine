// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
class UEditorEngine;
class FLevelViewportLayout;
class FLevelEditorViewportClient;

// FEditorToolbarPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorToolbarPanel
{
public:
    FEditorToolbarPanel() = default;
    ~FEditorToolbarPanel() = default;

    void Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice);
    void Release();

    float GetDesiredHeight() const { return ToolbarHeight; }

    void RenderPaneToolbar(FLevelViewportLayout* Layout, int32 SlotIndex, FLevelEditorViewportClient* ViewportClient);

private:
    bool DrawIconButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* FallbackLabel,
                        ImU32 TintColor) const;
    void PushPopupStyle() const;
    void PopPopupStyle() const;

    UEditorEngine* Editor = nullptr;
    ID3D11ShaderResourceView* PlayIcon = nullptr;
    ID3D11ShaderResourceView* StopIcon = nullptr;

    float ToolbarHeight = 44.0f;
    float IconSize = 18.0f;
    float ButtonSpacing = 6.0f;
    float PopupPadding = 12.0f;
};
