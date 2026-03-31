#include "ViewerImGui.h"

#if IS_OBJ_VIEWER

#include <d3d11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using namespace ViewerUI;

void FViewerImGui::Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* Context)
{
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(Hwnd);
    ImGui_ImplDX11_Init(Device, Context);
}

void FViewerImGui::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

LRESULT FViewerImGui::ForwardMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    return ImGui_ImplWin32_WndProcHandler(HWnd, Msg, WParam, LParam);
}

bool FViewerImGui::WantCaptureMouse() const
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

FViewerUIOutput FViewerImGui::Draw(const FViewerUIInput& Input)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    FViewerUIOutput Out;
    Out.bOpenRequested  = DrawLoadPanel(Input.MeshName, Input.FPS);
    DrawControlPanel(Input, Out);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return Out;
}

bool FViewerImGui::DrawLoadPanel(const char* MeshName, float FPS)
{
    ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.80f);
    ImGui::Begin("##LoadPanel", nullptr,
                 ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    const bool bOpenClicked = ImGui::Button("Open OBJ...", ImVec2(-1.f, 0.f));

    ImGui::Separator();

    if (MeshName && MeshName[0] != '\0')
        ImGui::TextUnformatted(MeshName);
    else
        ImGui::TextDisabled("No mesh loaded");

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", FPS);
    ImGui::Spacing();
    ImGui::TextDisabled("RMB drag : orbit");
    ImGui::TextDisabled("MMB drag : pan");
    ImGui::TextDisabled("Scroll   : zoom");

    ImGui::End();
    return bOpenClicked;
}

void FViewerImGui::DrawControlPanel(const FViewerUIInput& Input, FViewerUIOutput& Out)
{
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 170.f, 10.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(160.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.80f);
    ImGui::Begin("##ViewModePanel", nullptr,
                 ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    DrawViewModePanel(Input.CurrentViewMode, Out);

    ImGui::Separator();

    // Coordinate conversion toggle: left-handed Y-up OBJ to engine Z-up
    bool bConvert = Input.bConvertCoords;
    if (ImGui::Checkbox("Y-up to Z-up", &bConvert))
        Out.bConvertCoords = bConvert;
    else
        Out.bConvertCoords = Input.bConvertCoords;

    ImGui::Spacing();
    ImGui::TextUnformatted("Camera Alignment");

    DrawCameraPanel(Out);

    ImGui::End();
}

void FViewerImGui::DrawViewModePanel(EViewModeIndex Current, FViewerUIOutput& Out)
{
    ImGui::TextUnformatted("View Mode");
    int Selected = static_cast<int>(Current);
    if (ImGui::Combo("##Shading", &Selected, ViewModeLabels, IM_ARRAYSIZE(ViewModeLabels)))
        Current = static_cast<EViewModeIndex>(Selected);
    Out.SelectedViewMode = Current;
}

void FViewerImGui::DrawCameraPanel(FViewerUIOutput& Out)
{
    // Layout (cube-net cross):
    //       [ Top  ]
    //  [Left][Front][Right]
    //       [Bottom]
    //       [ Back ]

    const float Avail   = ImGui::GetContentRegionAvail().x;
    const float Spacing = ImGui::GetStyle().ItemSpacing.x;
    const float BtnW    = (Avail - Spacing * 2.f) / 3.f; // one third of row

    // Helper: draw a single button centered in the full row width
    auto CenteredBtn = [&](const char* Label, ECameraCommand Cmd)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (Avail - BtnW) * 0.5f);
        if (ImGui::Button(Label, ImVec2(BtnW, 0.f)))
            Out.CameraCommand = Cmd;
    };

    CenteredBtn("Top",    ECC_Up);

    if (ImGui::Button("Left",  ImVec2(BtnW, 0.f))) Out.CameraCommand = ECC_Left;
    ImGui::SameLine();
    if (ImGui::Button("Front", ImVec2(BtnW, 0.f))) Out.CameraCommand = ECC_Forward;
    ImGui::SameLine();
    if (ImGui::Button("Right", ImVec2(BtnW, 0.f))) Out.CameraCommand = ECC_Right;

    CenteredBtn("Bottom", ECC_Bottom);
    CenteredBtn("Back",   ECC_Back);
}

#endif // IS_OBJ_VIEWER
