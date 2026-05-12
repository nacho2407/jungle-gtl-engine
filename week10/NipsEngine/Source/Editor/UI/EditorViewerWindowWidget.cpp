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

    ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
    if (!SRV)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("Viewer");

    ImVec2 FullSize = ImGui::GetContentRegionAvail();

    float LeftWidth = FullSize.x * 0.25f;
    float RightWidth = FullSize.x - LeftWidth;

    // =====================================================
    // LEFT: Skeleton Tree (dummy bone data)
    // =====================================================
    ImGui::BeginChild("SkeletonPanel", ImVec2(LeftWidth, 0), true);

    ImGui::Text("Skeleton");

    if (ImGui::TreeNode("Root"))
    {
        if (ImGui::TreeNode("Pelvis"))
        {
            ImGui::BulletText("Spine_01");
            ImGui::BulletText("Spine_02");
            ImGui::BulletText("Chest");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Head"))
        {
            ImGui::BulletText("Neck");
            ImGui::BulletText("Head");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Left Arm"))
        {
            ImGui::BulletText("UpperArm_L");
            ImGui::BulletText("LowerArm_L");
            ImGui::BulletText("Hand_L");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Right Arm"))
        {
            ImGui::BulletText("UpperArm_R");
            ImGui::BulletText("LowerArm_R");
            ImGui::BulletText("Hand_R");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Legs"))
        {
            ImGui::BulletText("Thigh_L");
            ImGui::BulletText("Calf_L");
            ImGui::BulletText("Foot_L");

            ImGui::BulletText("Thigh_R");
            ImGui::BulletText("Calf_R");
            ImGui::BulletText("Foot_R");

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // =====================================================
    // RIGHT: Viewport
    // =====================================================
    ImGui::BeginChild("ViewportPanel", ImVec2(RightWidth, 0), false);

    ImVec2 ScreenPos = ImGui::GetCursorScreenPos();
    ImVec2 Size = ImGui::GetContentRegionAvail();

    Size.x = std::max(Size.x, 1.0f);
    Size.y = std::max(Size.y, 1.0f);

    POINT pt = { (LONG)ScreenPos.x, (LONG)ScreenPos.y };

    FViewportRect NewRect;
    NewRect.X = (int32)pt.x;
    NewRect.Y = (int32)pt.y;
    NewRect.Width = (int32)Size.x;
    NewRect.Height = (int32)Size.y;

    SceneViewport.SetRect(NewRect);

	if (auto* Client = SceneViewport.GetClient())
    {
        Client->SetViewportSize((float)NewRect.Width, (float)NewRect.Height);
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ID3D11DeviceContext* DC =
        EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

    DrawList->AddCallback(SetOpaqueBlendStateCallback, DC);

    // Render viewport
    ImGui::Image((ImTextureID)SRV, Size);

    DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}