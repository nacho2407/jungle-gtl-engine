#pragma once
#include "RenderPass.h"

// 깊이 무시 디버그 와이어(본 등)를 항상 위에 그리는 패스.
// 같은 RTV에 EditorLineBatcher 다음으로 한 번 더 flush — DSV는 머티리얼이 AlwaysOnTop으로 강제.
class FEditorOverlayRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;
};
