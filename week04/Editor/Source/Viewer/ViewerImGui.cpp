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
    DrawScalePanel(Input, Out);

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
    DrawCullModePanel(Input.SelectedCullMode, Out);

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

void FViewerImGui::DrawCullModePanel(ERasterizerCullMode CullMode, FViewerUIOutput& Out) 
{
    ImGui::TextUnformatted("Cull Mode");
    int Selected = static_cast<int>(CullMode);
    if (ImGui::Combo("##CullMode", &Selected, CullModeLabels, IM_ARRAYSIZE(CullModeLabels)))
    {
        CullMode = static_cast<ERasterizerCullMode>(Selected);
    }
    Out.SelectedCullMode = CullMode;
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

void FViewerImGui::DrawScalePanel(const FViewerUIInput& Input, FViewerUIOutput& Out)
{
    ImGui::SetNextWindowPos(ImVec2(10.f, 190.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.80f);
    ImGui::Begin("##ScalePanel", nullptr,
                 ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    // Header
    ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.f), "Model Scale");
    ImGui::Separator();
    ImGui::Spacing();

    float ScaleFactor[3] = { Input.ModelScale.X, Input.ModelScale.Y, Input.ModelScale.Z };
    const float AvailW   = ImGui::GetContentRegionAvail().x;
    const float LabelW   = 14.f; // colored axis label width
    const float DragW    = (AvailW - LabelW * 3.f - ImGui::GetStyle().ItemSpacing.x * 2.f) / 3.f;

    bool bChanged = false;

    // X — red
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.f, 0.35f, 0.35f, 1.f));
    ImGui::TextUnformatted("X");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 4.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.35f, 0.10f, 0.10f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0.50f, 0.18f, 0.18f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.65f, 0.22f, 0.22f, 1.f));
    ImGui::SetNextItemWidth(DragW);
    bChanged |= ImGui::DragFloat("##SX", &ScaleFactor[0], 0.001f, 0.00001f, 100.f, "%.4f");
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);

    // Y — green
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.35f, 1.f, 0.35f, 1.f));
    ImGui::TextUnformatted("Y");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 4.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.10f, 0.30f, 0.10f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0.18f, 0.45f, 0.18f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.60f, 0.22f, 1.f));
    ImGui::SetNextItemWidth(DragW);
    bChanged |= ImGui::DragFloat("##SY", &ScaleFactor[1], 0.001f, 0.00001f, 100.f, "%.4f");
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);

    // Z — blue
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.35f, 0.6f, 1.f, 1.f));
    ImGui::TextUnformatted("Z");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 4.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.10f, 0.15f, 0.35f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0.18f, 0.25f, 0.50f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.32f, 0.65f, 1.f));
    ImGui::SetNextItemWidth(DragW);
    bChanged |= ImGui::DragFloat("##SZ", &ScaleFactor[2], 0.001f, 0.00001f, 100.f, "%.4f");
    ImGui::PopStyleColor(3);

    // Absolute (uniform) scale
    ImGui::NewLine();
    ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);
    ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.9f, 0.6f, 1.f, 1.f));
    ImGui::TextUnformatted("Abs");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 4.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.22f, 0.10f, 0.35f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.35f, 0.18f, 0.50f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.45f, 0.22f, 0.65f, 1.f));
    ImGui::SetNextItemWidth(DragW);
    float AbsScale = Input.AbsoluteScale;
    if (ImGui::DragFloat("##SABS", &AbsScale, 0.001f, 0.00001f, 100.f, "%.4f"))
    {
        Out.AbsoluteScale  = AbsScale;
        Out.bIsScaleChanged = true;
    }
    ImGui::PopStyleColor(3);

    // Uniform scale reset button
    ImGui::Spacing();
    if (ImGui::Button("Reset", ImVec2(-1.f, 0.f)))
    {
        ScaleFactor[0] = ScaleFactor[1] = ScaleFactor[2] = 1.f;
        bChanged = true;
    }

    if (bChanged)
    {
        Out.ModelScale      = { ScaleFactor[0], ScaleFactor[1], ScaleFactor[2] };
        Out.AbsoluteScale   = AbsScale;
        Out.bIsScaleChanged = true;
    }

    ImGui::End();
}

#endif // IS_OBJ_VIEWER
