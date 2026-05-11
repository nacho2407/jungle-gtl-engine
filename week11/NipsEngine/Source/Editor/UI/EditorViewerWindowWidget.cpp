#include "EditorViewerWindowWidget.h"
#include "Editor/EditorEngine.h"
#include "Viewport/ViewportLayout.h"
#include "imgui.h"

namespace
{
void SetOpaqueBlendStateCallback(const ImDrawList*, const ImDrawCmd* Cmd)
{
    ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(Cmd->UserCallbackData);
    if (!DeviceContext)
        return;

    const float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
}
}

void FEditorViewerWindowWidget::Render(float DeltaTime)
{
    if (!EditorEngine)
        return;

    auto& SceneViewport = EditorEngine->GetViewer().GetViewport();
    const FViewportRect& Rect = SceneViewport.GetRect();

    if (Rect.Width <= 0 || Rect.Height <= 0)
        return;

    ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
    if (!SRV)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("Viewer");

    ImVec2 ScreenPos = ImGui::GetCursorScreenPos();
    ImVec2 Size = ImGui::GetContentRegionAvail();
    Size.x = std::max(Size.x, 1.0f);
    Size.y = std::max(Size.y, 1.0f);

    POINT pt = { (LONG)ScreenPos.x, (LONG)ScreenPos.y };
    if (EditorEngine->GetWindow())
    {
        pt = EditorEngine->GetWindow()->ScreenToClientPoint(pt);
    }

    FViewportRect NewRect = { (int32)pt.x, (int32)pt.y, (int32)Size.x, (int32)Size.y };
    EditorEngine->GetViewer().SetRect(NewRect);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ID3D11DeviceContext* DC =
        EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

    DrawList->AddCallback(SetOpaqueBlendStateCallback, DC);

    ImGui::Image((ImTextureID)SRV, ImVec2(
                                       (float)Rect.Width,
                                       (float)Rect.Height));

    DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::End();
    ImGui::PopStyleVar();
}