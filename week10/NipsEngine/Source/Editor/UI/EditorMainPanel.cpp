

#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Core/Paths.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "WICTextureLoader.h"

#include "Render/Renderer/Renderer.h"
#include "Render/Resource/Shader.h"
#include "Engine/Input/InputSystem.h"
#include "Math/Utils.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <cmath>
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

const char* GetViewportTypeName(EEditorViewportType Type)
{
    switch (Type)
    {
    case EVT_Perspective:
        return "Perspective";
    case EVT_OrthoTop:
        return "Top";
    case EVT_OrthoBottom:
        return "Bottom";
    case EVT_OrthoFront:
        return "Front";
    case EVT_OrthoBack:
        return "Back";
    case EVT_OrthoLeft:
        return "Left";
    case EVT_OrthoRight:
        return "Right";
    default:
        return "Viewport";
    }
}

const char* GetLightingModelName(ELightingModel Key)
{
    switch (Key)
    {
    case ELightingModel::Gouraud:   return "Gouraud";
    case ELightingModel::Lambert:   return "Lambert";
    case ELightingModel::BlinnPhong: return "BlinnPhong";
    default:                                     return "Unlit";
    }
}

const char* GetViewModeName(EViewMode Mode)
{
    switch (Mode)
    {
    case EViewMode::Lit_Gouraud:
        return "Lit (Gouraud)";
    case EViewMode::Lit_Lambert:
        return "Lit (Lambert)";
    case EViewMode::Lit_BlinnPhong:
        return "Lit (Blinn-Phong)";
    case EViewMode::Unlit:
        return "Unlit";
	case EViewMode::Heatmap:
		return "Heatmap";
    case EViewMode::Wireframe:
        return "Wireframe";
	case EViewMode::Depth: 
		return "Depth";
	case EViewMode::Normal:
		return "Normal";
	default:
        return "Lit";
    }
}

const char* GetLightCullModeName(ELightCullMode Mode)
{
    switch (Mode)
    {
    case ELightCullMode::Clustered: return "Clustered";
	case ELightCullMode::Tiled:		return "Tiled";
    case ELightCullMode::None:      return "None (All Lights)";
    default:                        return "Clustered";
    }
}

const char* GetViewportSlotName(int32 Index)
{
    switch (Index)
    {
    case 0:
        return "Viewport 0";
    case 1:
        return "Viewport 1";
    case 2:
        return "Viewport 2";
    case 3:
        return "Viewport 3";
    default:
        return "Viewport";
    }
}

const wchar_t* GetViewportToolIconFileName(FEditorMainPanel::EViewportToolIcon Icon)
{
    switch (Icon)
    {
    case FEditorMainPanel::EViewportToolIcon::Menu: return L"Menu.png";
    case FEditorMainPanel::EViewportToolIcon::Select: return L"Select.png";
    case FEditorMainPanel::EViewportToolIcon::Translate: return L"Translate.png";
    case FEditorMainPanel::EViewportToolIcon::Rotate: return L"Rotate.png";
    case FEditorMainPanel::EViewportToolIcon::Scale: return L"Scale.png";
    case FEditorMainPanel::EViewportToolIcon::TranslateSnap: return L"Translate_Snap.png";
    case FEditorMainPanel::EViewportToolIcon::RotateSnap: return L"Rotate_Snap.png";
    case FEditorMainPanel::EViewportToolIcon::ScaleSnap: return L"Scale_Snap.png";
    case FEditorMainPanel::EViewportToolIcon::WorldSpace: return L"WorldSpace.png";
    case FEditorMainPanel::EViewportToolIcon::LocalSpace: return L"LocalSpace.png";
    case FEditorMainPanel::EViewportToolIcon::Camera: return L"Camera.png";
    case FEditorMainPanel::EViewportToolIcon::Setting: return L"Setting.png";
    default: return L"";
    }
}

const wchar_t* GetViewportLayoutIconFileName(EEditorViewportLayoutMode Mode)
{
    switch (Mode)
    {
    case EEditorViewportLayoutMode::OnePane: return L"ViewportLayout_OnePane.png";
    case EEditorViewportLayoutMode::TwoPanesHoriz: return L"ViewportLayout_TwoPanesHoriz.png";
    case EEditorViewportLayoutMode::TwoPanesVert: return L"ViewportLayout_TwoPanesVert.png";
    case EEditorViewportLayoutMode::ThreePanesLeft: return L"ViewportLayout_ThreePanesLeft.png";
    case EEditorViewportLayoutMode::ThreePanesRight: return L"ViewportLayout_ThreePanesRight.png";
    case EEditorViewportLayoutMode::ThreePanesTop: return L"ViewportLayout_ThreePanesTop.png";
    case EEditorViewportLayoutMode::ThreePanesBottom: return L"ViewportLayout_ThreePanesBottom.png";
    case EEditorViewportLayoutMode::FourPanes2x2: return L"ViewportLayout_FourPanes2x2.png";
    case EEditorViewportLayoutMode::FourPanesLeft: return L"ViewportLayout_FourPanesLeft.png";
    case EEditorViewportLayoutMode::FourPanesRight: return L"ViewportLayout_FourPanesRight.png";
    case EEditorViewportLayoutMode::FourPanesTop: return L"ViewportLayout_FourPanesTop.png";
    case EEditorViewportLayoutMode::FourPanesBottom: return L"ViewportLayout_FourPanesBottom.png";
    default: return L"";
    }
}

const char* GetViewportLayoutLabel(EEditorViewportLayoutMode Mode)
{
    switch (Mode)
    {
    case EEditorViewportLayoutMode::OnePane: return "One Pane";
    case EEditorViewportLayoutMode::TwoPanesHoriz: return "Two Panes Horiz";
    case EEditorViewportLayoutMode::TwoPanesVert: return "Two Panes Vert";
    case EEditorViewportLayoutMode::ThreePanesLeft: return "Three Panes Left";
    case EEditorViewportLayoutMode::ThreePanesRight: return "Three Panes Right";
    case EEditorViewportLayoutMode::ThreePanesTop: return "Three Panes Top";
    case EEditorViewportLayoutMode::ThreePanesBottom: return "Three Panes Bottom";
    case EEditorViewportLayoutMode::FourPanes2x2: return "Four Panes 2x2";
    case EEditorViewportLayoutMode::FourPanesLeft: return "Four Panes Left";
    case EEditorViewportLayoutMode::FourPanesRight: return "Four Panes Right";
    case EEditorViewportLayoutMode::FourPanesTop: return "Four Panes Top";
    case EEditorViewportLayoutMode::FourPanesBottom: return "Four Panes Bottom";
    default: return "Layout";
    }
}

FVector ComputePlacementLocation(FEditorViewportClient* Client, float LocalX, float LocalY)
{
    if (!Client)
    {
        return FVector(0.0f, 0.0f, 0.0f);
    }

    FViewportCamera* Camera = Client->GetCamera();
    const FSceneViewport* Viewport = Client->GetViewport();
    if (!Camera || !Viewport)
    {
        return FVector(0.0f, 0.0f, 0.0f);
    }

    const FViewportRect& Rect = Viewport->GetRect();
    if (Rect.Width <= 0 || Rect.Height <= 0)
    {
        return Camera->GetLocation() + Camera->GetEffectiveForward().GetSafeNormal() * 10.0f;
    }

    const FRay Ray = Camera->DeprojectScreenToWorld(
        LocalX,
        LocalY,
        static_cast<float>(Rect.Width),
        static_cast<float>(Rect.Height));

    if (std::fabs(Ray.Direction.Z) > 0.0001f)
    {
        const float T = -Ray.Origin.Z / Ray.Direction.Z;
        if (T > 0.0f)
        {
            return Ray.Origin + Ray.Direction * T;
        }
    }

    return Ray.Origin + Ray.Direction.GetSafeNormal() * 10.0f;
}
} // namespace
void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    IO.MouseDrawCursor = false;

    ImGuiStyle& Style = ImGui::GetStyle();
    Style.WindowRounding = 6.0f;
    Style.FrameRounding = 6.0f;
    Style.GrabRounding = 6.0f;
    Style.PopupRounding = 6.0f;
    Style.TabRounding = 6.0f;
    Style.ScrollbarRounding = 6.0f;
    Style.WindowBorderSize = 1.0f;
    Style.FrameBorderSize = 0.0f;
    Style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
    Style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.56f, 0.62f, 1.0f);
    Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    Style.Colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
    Style.Colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.13f, 0.98f);
    Style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.23f, 0.27f, 1.0f);
    Style.Colors[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.19f, 0.22f, 1.0f);
    Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.29f, 1.0f);
    Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
    Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.15f, 1.0f);
    Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
    Style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.26f, 0.95f);
    Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.29f, 0.35f, 1.0f);
    Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_Header] = ImVec4(0.19f, 0.22f, 0.27f, 1.0f);
    Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.28f, 0.35f, 1.0f);
    Style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);
    Style.Colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.26f, 0.33f, 1.0f);
    Style.Colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_CheckMark] = ImVec4(0.32f, 0.61f, 0.93f, 1.0f);
    Style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.61f, 0.93f, 1.0f);
    Style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.39f, 0.69f, 0.97f, 1.0f);

    Window = InWindow;
    EditorEngine = InEditorEngine;

    // 1차: malgun.ttf — 한글 + 기본 라틴 (주 폰트)
    ImFontGlyphRangesBuilder KoreanBuilder;
    KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesKorean());
    KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesDefault());

    KoreanBuilder.BuildRanges(&FontGlyphRanges);

    IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 16.0f, nullptr, FontGlyphRanges.Data);

    ImFontConfig icon_config;
    icon_config.MergeMode = true;  // 중요: 앞서 로드한 맑은 고딕에 폰트를 병합합니다.
    icon_config.PixelSnapH = true; // 아이콘을 픽셀 경계에 맞춰 선명하게 렌더링

    // 추가할 특수 기호의 유니코드 범위 설정 (▶, ⏸, ■)
    // ▶ (U+25B6), ⏸ (U+23F8), ■ (U+25A0)
    static const ImWchar icon_ranges[] = {
        0x23F8, 0x23F8, // ⏸
        0x25A0, 0x25A0, // ■
        0x25B6, 0x25B6, // ▶
        0,              // 배열의 끝을 알리는 0
    };

    IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", 16.0f, &icon_config, icon_ranges);

    // 2차: msyh.ttc — 한자 전체를 malgun이 없는 글리프에만 병합 (fallback)
    ImFontConfig MergeConfig;
    MergeConfig.MergeMode = true;
    IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 16.0f, &MergeConfig,
                                 IO.Fonts->GetGlyphRangesChineseFull());

    ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
    ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());
    LoadViewportToolIcons(InRenderer.GetFD3DDevice().GetDevice());

    ConsoleWidget.Initialize(InEditorEngine);
    ControlWidget.Initialize(InEditorEngine);
    MaterialWidget.Initialize(InEditorEngine);
    PropertyWidget.Initialize(InEditorEngine);
    SceneWidget.Initialize(InEditorEngine);
    ViewportOverlayWidget.Initialize(InEditorEngine);
    StatWidget.Initialize(InEditorEngine);
    PlayStreamWidget.Initialize(InEditorEngine);
    ToolbarWidget.Initialize(InEditorEngine);
    ToolbarWidget.SetViewportOverlayWidget(&ViewportOverlayWidget);
    ToolbarWidget.SetSceneWidget(&SceneWidget);
    ToolbarWidget.SetPlayStreamWidget(&PlayStreamWidget);
    ToolbarWidget.SetPanelVisibilityRefs(&bShowConsole, &bShowControl, &bShowProperty, &bShowSceneManager,
                                         &bShowMaterialEditor, &bShowStatProfiler);
}

void FEditorMainPanel::Release()
{
    ReleaseViewportToolIcons();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void FEditorMainPanel::LoadViewportToolIcons(ID3D11Device* Device)
{
    if (!Device)
    {
        return;
    }

    const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/ToolIcons/");
    for (int32 i = 0; i < static_cast<int32>(EViewportToolIcon::Count); ++i)
    {
        ID3D11ShaderResourceView*& SRV = ViewportToolIcons[i];
        if (SRV)
        {
            continue;
        }

        const std::wstring IconPath = IconDir + GetViewportToolIconFileName(static_cast<EViewportToolIcon>(i));
        DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, &SRV);
    }

    if (!AddActorIconSRV)
    {
        const std::wstring AddActorIconPath = IconDir + L"Add_Actor.png";
        DirectX::CreateWICTextureFromFile(Device, AddActorIconPath.c_str(), nullptr, &AddActorIconSRV);
    }

    const std::wstring LayoutIconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");
    for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
    {
        ID3D11ShaderResourceView*& SRV = ViewportLayoutIcons[i];
        if (SRV)
        {
            continue;
        }

        const std::wstring IconPath = LayoutIconDir + GetViewportLayoutIconFileName(static_cast<EEditorViewportLayoutMode>(i));
        DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, &SRV);
    }
}

void FEditorMainPanel::ReleaseViewportToolIcons()
{
    for (int32 i = 0; i < static_cast<int32>(EViewportToolIcon::Count); ++i)
    {
        if (ViewportToolIcons[i])
        {
            ViewportToolIcons[i]->Release();
            ViewportToolIcons[i] = nullptr;
        }
    }
    for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
    {
        if (ViewportLayoutIcons[i])
        {
            ViewportLayoutIcons[i]->Release();
            ViewportLayoutIcons[i] = nullptr;
        }
    }
    if (AddActorIconSRV)
    {
        AddActorIconSRV->Release();
        AddActorIconSRV = nullptr;
    }
}

void FEditorMainPanel::Render(float DeltaTime)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ToolbarWidget.Render(DeltaTime);
    RenderEditorToolbar();
    RenderDockSpace();

    RenderViewportHostWindow();

    if (bShowControl)
        ControlWidget.Render(DeltaTime);
    if (bShowMaterialEditor)
        MaterialWidget.Render(DeltaTime);
    if (bShowProperty)
        PropertyWidget.Render(DeltaTime);
    if (bShowSceneManager)
        SceneWidget.Render(DeltaTime);
    if (bShowStatProfiler)
        StatWidget.Render(DeltaTime);
    ViewportOverlayWidget.Render(DeltaTime);

    float EffectiveDeltaTime = DeltaTime;
    if (EffectiveDeltaTime <= 0.0f)
    {
        EffectiveDeltaTime = ImGui::GetIO().DeltaTime;
        if (EffectiveDeltaTime <= 0.0f)
        {
            EffectiveDeltaTime = 1.0f / 60.0f;
        }
    }

    if (!bShowConsole)
    {
        bConsoleDrawerVisible = false;
    }

    const float TargetAnim = bConsoleDrawerVisible ? 1.0f : 0.0f;
    constexpr float AnimSpeed = 8.0f;
    if (ConsoleDrawerAnim < TargetAnim)
    {
        ConsoleDrawerAnim = std::min(1.0f, ConsoleDrawerAnim + EffectiveDeltaTime * AnimSpeed);
    }
    else if (ConsoleDrawerAnim > TargetAnim)
    {
        ConsoleDrawerAnim = std::max(0.0f, ConsoleDrawerAnim - EffectiveDeltaTime * AnimSpeed);
    }

    UpdateFooterEventLogs();
    FooterLogSystem.Tick(EffectiveDeltaTime);
    RenderConsoleDrawer(DeltaTime);
    RenderFooterOverlay(DeltaTime);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderDockSpace()
{
    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    constexpr float EditorToolbarHeight = 40.0f;
    constexpr float FooterHeight = 32.0f;
    const ImVec2 DockPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y + EditorToolbarHeight);
    const ImVec2 DockSize(
        MainViewport->WorkSize.x,
        (MainViewport->WorkSize.y > (FooterHeight + EditorToolbarHeight))
            ? (MainViewport->WorkSize.y - FooterHeight - EditorToolbarHeight)
            : 0.0f);

    ImGui::SetNextWindowPos(DockPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(DockSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags DockHostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##MainDockHost", nullptr, DockHostFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID DockSpaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f));
    ImGui::End();
}

void FEditorMainPanel::RenderEditorToolbar()
{
    if (!EditorEngine)
    {
        return;
    }

    ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    constexpr float ToolbarHeight = 40.0f;
    constexpr float ButtonSize = 30.0f;
    const ImVec2 ToolbarPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y);
    const ImVec2 ToolbarSize(MainViewport->WorkSize.x, ToolbarHeight);

    ImGui::SetNextWindowPos(ToolbarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ToolbarSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.98f));

    constexpr ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##EditorToolbarWeek06", nullptr, Flags))
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();
        FEditorViewportClient* Client = Layout.GetViewportClient(FocusedIndex);
        const FViewportRect& FocusedRect = Layout.GetSceneViewport(FocusedIndex).GetRect();
        const float SpawnLocalX = FocusedRect.Width > 0 ? static_cast<float>(FocusedRect.Width) * 0.5f : 0.0f;
        const float SpawnLocalY = FocusedRect.Height > 0 ? static_cast<float>(FocusedRect.Height) * 0.5f : 0.0f;
        const FVector ToolbarSpawnLocation = ComputePlacementLocation(Client, SpawnLocalX, SpawnLocalY);

        const bool bEditing = EditorEngine->GetEditorState() == EViewportPlayState::Editing;
        const bool bCanPlaceActor = bEditing && Client != nullptr;

        if (!bCanPlaceActor)
        {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
        const bool bAddClicked = ImGui::InvisibleButton("##ToolbarAddActor", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 AddMin = ImGui::GetItemRectMin();
        const ImVec2 AddMax = ImGui::GetItemRectMax();
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const bool bAddHovered = bCanPlaceActor && ImGui::IsItemHovered();
        const ImU32 AddBg = ImGui::GetColorU32(bAddHovered ? ImVec4(0.22f, 0.25f, 0.32f, 1.0f) : ImVec4(0.15f, 0.17f, 0.21f, 1.0f));
        const ImU32 AddBorder = ImGui::GetColorU32(bCanPlaceActor ? ImVec4(0.40f, 0.44f, 0.58f, 1.0f) : ImVec4(0.29f, 0.31f, 0.36f, 1.0f));
        DrawList->AddRectFilled(AddMin, AddMax, AddBg, 5.0f);
        DrawList->AddRect(AddMin, AddMax, AddBorder, 5.0f);
        if (AddActorIconSRV)
        {
            DrawList->AddImage(
                reinterpret_cast<ImTextureID>(AddActorIconSRV),
                ImVec2(AddMin.x + 5.0f, AddMin.y + 5.0f),
                ImVec2(AddMax.x - 5.0f, AddMax.y - 5.0f));
        }
        else
        {
            const ImU32 IconColor = ImGui::GetColorU32(ImVec4(0.84f, 0.88f, 0.96f, 1.0f));
            const float Cx = (AddMin.x + AddMax.x) * 0.5f;
            const float Cy = (AddMin.y + AddMax.y) * 0.5f;
            DrawList->AddLine(ImVec2(Cx - 7.0f, Cy), ImVec2(Cx + 7.0f, Cy), IconColor, 2.0f);
            DrawList->AddLine(ImVec2(Cx, Cy - 7.0f), ImVec2(Cx, Cy + 7.0f), IconColor, 2.0f);
        }
        if (bAddHovered)
        {
            ImGui::SetTooltip("Place Actor");
        }
        if (bAddClicked)
        {
            ImGui::OpenPopup("##ToolbarPlaceActorPopup");
        }
        if (ImGui::BeginPopup("##ToolbarPlaceActorPopup"))
        {
            for (int32 i = 0; i < ControlWidget.GetPrimitiveTypeCount(); ++i)
            {
                if (ImGui::MenuItem(ControlWidget.GetPrimitiveTypeLabel(i)))
                {
                    ControlWidget.SpawnPrimitive(i, ToolbarSpawnLocation, 1);
                }
            }
            ImGui::EndPopup();
        }

        if (!bCanPlaceActor)
        {
            ImGui::EndDisabled();
        }

        const EViewportPlayState CurrentState = EditorEngine->GetEditorState();
        const bool bPlaying = CurrentState == EViewportPlayState::Playing;
        const bool bPaused = CurrentState == EViewportPlayState::Paused;
        const bool bCanStop = CurrentState != EViewportPlayState::Editing;
        const float CenterX = ToolbarSize.x * 0.5f;
        const float ButtonY = 5.0f;
        const float Gap = 8.0f;

        ImGui::SetCursorPos(ImVec2(CenterX - ButtonSize - Gap * 0.5f, ButtonY));
        const bool bPlayClicked = ImGui::InvisibleButton("##ToolbarPlayPause", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 PlayMin = ImGui::GetItemRectMin();
        const ImVec2 PlayMax = ImGui::GetItemRectMax();
        const bool bPlayHovered = ImGui::IsItemHovered();
        DrawList->AddRectFilled(PlayMin, PlayMax, ImGui::GetColorU32(bPlayHovered ? ImVec4(0.18f, 0.34f, 0.22f, 1.0f) : ImVec4(0.14f, 0.24f, 0.17f, 1.0f)), 5.0f);
        DrawList->AddRect(PlayMin, PlayMax, ImGui::GetColorU32(ImVec4(0.34f, 0.62f, 0.39f, 1.0f)), 5.0f);
        const ImU32 PlayColor = ImGui::GetColorU32(ImVec4(0.52f, 0.95f, 0.60f, 1.0f));
        if (bPlaying)
        {
            DrawList->AddRectFilled(ImVec2(PlayMin.x + 9.0f, PlayMin.y + 8.0f), ImVec2(PlayMin.x + 13.0f, PlayMax.y - 8.0f), PlayColor, 1.0f);
            DrawList->AddRectFilled(ImVec2(PlayMax.x - 13.0f, PlayMin.y + 8.0f), ImVec2(PlayMax.x - 9.0f, PlayMax.y - 8.0f), PlayColor, 1.0f);
        }
        else
        {
            DrawList->AddTriangleFilled(
                ImVec2(PlayMin.x + 11.0f, PlayMin.y + 8.0f),
                ImVec2(PlayMin.x + 11.0f, PlayMax.y - 8.0f),
                ImVec2(PlayMax.x - 8.0f, (PlayMin.y + PlayMax.y) * 0.5f),
                PlayColor);
        }
        if (bPlayHovered)
        {
            ImGui::SetTooltip(bPlaying ? "Pause" : (bPaused ? "Resume" : "Play"));
        }
        if (bPlayClicked)
        {
            if (bPlaying)
            {
                EditorEngine->PausePlaySession();
            }
            else
            {
                EditorEngine->StartPlaySession();
            }
        }

        ImGui::SetCursorPos(ImVec2(CenterX + Gap * 0.5f, ButtonY));
        if (!bCanStop)
        {
            ImGui::BeginDisabled();
        }
        const bool bStopClicked = ImGui::InvisibleButton("##ToolbarStop", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 StopMin = ImGui::GetItemRectMin();
        const ImVec2 StopMax = ImGui::GetItemRectMax();
        const bool bStopHovered = bCanStop && ImGui::IsItemHovered();
        DrawList->AddRectFilled(StopMin, StopMax, ImGui::GetColorU32(bStopHovered ? ImVec4(0.34f, 0.18f, 0.18f, 1.0f) : ImVec4(0.24f, 0.14f, 0.14f, 1.0f)), 5.0f);
        DrawList->AddRect(StopMin, StopMax, ImGui::GetColorU32(bCanStop ? ImVec4(0.65f, 0.34f, 0.34f, 1.0f) : ImVec4(0.35f, 0.30f, 0.30f, 1.0f)), 5.0f);
        DrawList->AddRectFilled(ImVec2(StopMin.x + 9.0f, StopMin.y + 9.0f), ImVec2(StopMax.x - 9.0f, StopMax.y - 9.0f), ImGui::GetColorU32(ImVec4(0.95f, 0.53f, 0.53f, bCanStop ? 1.0f : 0.45f)), 1.0f);
        if (bStopHovered)
        {
            ImGui::SetTooltip("Stop");
        }
        if (bStopClicked && bCanStop)
        {
            EditorEngine->StopPlaySession();
        }
        if (!bCanStop)
        {
            ImGui::EndDisabled();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void FEditorMainPanel::RenderConsoleDrawer(float DeltaTime)
{
    (void)DeltaTime;
    constexpr float FooterHeight = 32.0f;
    constexpr float DrawerMaxHeight = 320.0f;
    if (ConsoleDrawerAnim <= 0.001f)
    {
        return;
    }

    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
    const float DrawerHeight = DrawerMaxHeight * ConsoleDrawerAnim;
    if (DrawerHeight <= 1.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight - DrawerHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, DrawerHeight), ImGuiCond_Always);
    if (MainViewport)
    {
        ImGui::SetNextWindowViewport(MainViewport->ID);
    }

    constexpr ImGuiWindowFlags DrawerFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.23f, 0.27f, 1.0f));

    if (bBringConsoleDrawerToFrontNextFrame)
    {
        ImGui::SetNextWindowFocus();
        bBringConsoleDrawerToFrontNextFrame = false;
    }

    if (ImGui::Begin("##EditorConsoleDrawer", nullptr, DrawerFlags))
    {
        ConsoleWidget.RenderDrawerToolbar();
        ImGui::Separator();
        ConsoleWidget.RenderLogContents(0.0f);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorMainPanel::RenderFooterOverlay(float DeltaTime)
{
    (void)DeltaTime;
    if (!EditorEngine)
    {
        return;
    }

    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
    constexpr float FooterHeight = 32.0f;

    ImGui::SetNextWindowPos(
        ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, FooterHeight), ImGuiCond_Always);
    if (MainViewport)
    {
        ImGui::SetNextWindowViewport(MainViewport->ID);
    }

    constexpr ImGuiWindowFlags FooterFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.23f, 0.27f, 1.0f));

    if (ImGui::Begin("##EditorStatusBar", nullptr, FooterFlags))
    {
        const TArray<FString> ActiveLogs = FooterLogSystem.GetActiveMessages();
        if (bShowConsole && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
        {
            switch (ConsoleBacktickCycleState)
            {
            case 0:
                ConsoleBacktickCycleState = 1;
                bConsoleDrawerVisible = false;
                bFocusConsoleInputNextFrame = true;
                break;
            case 1:
                ConsoleBacktickCycleState = 2;
                bConsoleDrawerVisible = true;
                bBringConsoleDrawerToFrontNextFrame = true;
                bFocusConsoleInputNextFrame = true;
                break;
            default:
                ConsoleBacktickCycleState = 0;
                bConsoleDrawerVisible = false;
                bFocusConsoleInputNextFrame = false;
                bFocusConsoleButtonNextFrame = true;
                break;
            }
        }

        const bool bDrawerOpen = ConsoleDrawerAnim > 0.5f;
        if (!bShowConsole)
        {
            ImGui::TextDisabled("Console Drawer hidden");
        }
        else
        {
            if (bFocusConsoleButtonNextFrame)
            {
                ImGui::SetKeyboardFocusHere();
                bFocusConsoleButtonNextFrame = false;
            }
            if (ImGui::Button(bDrawerOpen ? "Console ▼" : "Console ▲"))
            {
                bConsoleDrawerVisible = !bConsoleDrawerVisible;
                if (bConsoleDrawerVisible)
                {
                    ConsoleBacktickCycleState = 2;
                    bBringConsoleDrawerToFrontNextFrame = true;
                    bFocusConsoleInputNextFrame = true;
                }
                else
                {
                    ConsoleBacktickCycleState = 0;
                    bFocusConsoleButtonNextFrame = true;
                }
            }

            ImGui::SameLine();
            const float InputWidth = OverlaySize.x * (bDrawerOpen ? 0.35f : 0.175f);
            ConsoleWidget.RenderInputLine("##FooterConsoleInput", InputWidth, bFocusConsoleInputNextFrame);
            if (bFocusConsoleInputNextFrame)
            {
                ConsoleBacktickCycleState = bConsoleDrawerVisible ? 2 : 1;
            }
            bFocusConsoleInputNextFrame = false;
        }

        ImGui::SameLine();
        const EViewportPlayState CurrentState = EditorEngine->GetEditorState();
        const char* Domain = CurrentState == EViewportPlayState::Editing
            ? "Editor"
            : (CurrentState == EViewportPlayState::Paused ? "PIE Paused" : "PIE");
        ImGui::Text("Domain: %s", Domain);

        const char* LayoutLabel = GetViewportLayoutLabel(EditorEngine->GetViewportLayout().GetLayoutMode());
        char RightLabel[128];
        snprintf(RightLabel, sizeof(RightLabel), "Layout: %s", LayoutLabel);
        const float RightWidth = ImGui::CalcTextSize(RightLabel).x;
        const float RightX = OverlaySize.x - ImGui::GetStyle().WindowPadding.x - RightWidth;

        if (!ActiveLogs.empty())
        {
            const FString& LatestLog = ActiveLogs.back();
            const float LogWidth = ImGui::CalcTextSize(LatestLog.c_str()).x;
            float LogX = RightX - 16.0f - LogWidth;
            const float MinLogX = ImGui::GetCursorPosX() + 8.0f;
            if (LogX < MinLogX)
            {
                LogX = MinLogX;
            }
            ImGui::SameLine(LogX);
            ImGui::TextUnformatted(LatestLog.c_str());
        }

        ImGui::SameLine(RightX);
        ImGui::TextUnformatted(RightLabel);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorMainPanel::UpdateFooterEventLogs()
{
    if (!EditorEngine)
    {
        return;
    }

    const EViewportPlayState CurrentState = EditorEngine->GetEditorState();
    const bool bPIEPlaying = CurrentState != EViewportPlayState::Editing;
    if (!bFooterEventStateInitialized)
    {
        bFooterEventStateInitialized = true;
        bPrevPIEPlaying = bPIEPlaying;
        PrevEditorState = CurrentState;
        return;
    }

    if (!bPrevPIEPlaying && bPIEPlaying)
    {
        FooterLogSystem.Push("PIE started");
    }
    else if (bPrevPIEPlaying && !bPIEPlaying)
    {
        FooterLogSystem.Push("PIE ended");
    }
    else if (PrevEditorState != CurrentState)
    {
        switch (CurrentState)
        {
        case EViewportPlayState::Paused:
            FooterLogSystem.Push("PIE paused");
            break;
        case EViewportPlayState::Playing:
            FooterLogSystem.Push("PIE resumed");
            break;
        default:
            break;
        }
    }

    bPrevPIEPlaying = bPIEPlaying;
    PrevEditorState = CurrentState;
}

void FEditorMainPanel::Update()
{
    ImGuiIO& IO = ImGui::GetIO();

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    bool bViewportOperationActive = Layout.HasActiveOperationViewport();

    if (bViewportOperationActive)
    {
        IO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    else
    {
        IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    bool bWantMouse = bViewportOperationActive ? false : IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard;
    const bool bWantTextInput = IO.WantTextInput;
    const bool bAnyUIItemActive = ImGui::IsAnyItemActive();
    const bool bAnyUIItemHovered = ImGui::IsAnyItemHovered();
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    const bool bAnyWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    const bool bAnyWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

    bool bMouseOverViewportRect = false;
    if (Window)
    {
        POINT MouseClientPos = Window->ScreenToClientPoint(InputSystem::Get().GetMousePos());
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            const FViewportRect& ViewportRect = Layout.GetSceneViewport(i).GetRect();
            if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
            {
                bMouseOverViewportRect = true;
                break;
            }
        }
    }

    bool bHoveredViewportContentWindow = false;
    bool bHoveredNonViewportWindow = false;
    if (ImGuiContext* Context = ImGui::GetCurrentContext())
    {
        if (ImGuiWindow* HoveredWindow = Context->HoveredWindow)
        {
            const char* HoveredName = HoveredWindow->Name ? HoveredWindow->Name : "";
            bHoveredViewportContentWindow =
                (std::strcmp(HoveredName, "Viewport") == 0)
                || (std::strncmp(HoveredName, "Viewport###", 11) == 0);
            bHoveredNonViewportWindow = !bHoveredViewportContentWindow;
        }
    }

    if (!bHoveredViewportContentWindow
        && bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bAnyUIItemActive
        && !bAnyUIItemHovered
        && !bAnyPopupOpen)
    {
        bHoveredViewportContentWindow = true;
    }

    const bool bReleaseMouseToViewport =
        bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bAnyPopupOpen;
    const bool bNonViewportImGuiInteraction =
        bHoveredNonViewportWindow
        && (bAnyWindowHovered || bAnyWindowFocused || bAnyUIItemActive || bAnyUIItemHovered || bAnyPopupOpen || bWantTextInput || bWantKeyboard);

    if (bNonViewportImGuiInteraction)
    {
        bWantMouse = true;
    }
    else if (EditorEngine && bReleaseMouseToViewport)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    InputSystem::Get().SetGuiMouseCapture(bWantMouse);
    InputSystem::Get().SetGuiKeyboardCapture(bWantKeyboard);
    InputSystem::Get().SetGuiTextInputCapture(bWantTextInput);

    //	Focus는 MainPanel에서 입력 받음
    if (EditorEngine && InputSystem::Get().GetKeyUp('F') && !IO.WantTextInput)
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
        Layout.GetViewportClient(FocusedIdx)->FocusSelection();
    }

    // IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
    // 그 외에는 OS 수준에서 IME 컨텍스트를 NULL로 연결해 한글 조합이
    // 뷰포트에 남는 현상을 원천 차단한다.
    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            // InputText 포커스 중 — 기본 IME 컨텍스트 복원
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            // InputText 포커스 없음 — IME 컨텍스트 해제 (조합 불가)
            ImmAssociateContext(hWnd, NULL);
        }
    }
}

bool FEditorMainPanel::RequestNewScene()
{
    SceneWidget.NewScene();
    return true;
}

bool FEditorMainPanel::RequestLoadSceneWithDialog()
{
    FString PickedPath;
    if (!ToolbarWidget.OpenSceneFileDialog(PickedPath))
    {
        return false;
    }

    SceneWidget.LoadSceneFromFilePath(PickedPath);
    return true;
}

bool FEditorMainPanel::RequestSaveScene()
{
    SceneWidget.SaveScene();
    return true;
}

bool FEditorMainPanel::RequestSaveSceneAsWithDialog()
{
    FString PickedPath;
    if (!ToolbarWidget.SaveSceneFileDialog(PickedPath))
    {
        return false;
    }

    SceneWidget.SaveSceneToFilePath(PickedPath);
    return true;
}

// ImGui로 Viewport 가 차지할 영역을 계산하고 만든다.
void FEditorMainPanel::RenderViewportHostWindow()
{
    if (!EditorEngine)
        return;
    constexpr ImGuiWindowFlags WindowFlags = 0;
    FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();

    if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
    {
        GuiState.bViewportHostVisible = false;
        GuiState.ViewportHostRect = FViewportRect();
        EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
        ImGui::End();
        return;
    }

    const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
    if (ContentSize.x > 1.0f && ContentSize.y > 1.0f)
    {
        const ImVec2 ContentPos = ImGui::GetCursorScreenPos();
        const FViewportRect HostRect(
            static_cast<int32>(ContentPos.x),
            static_cast<int32>(ContentPos.y),
            static_cast<int32>(ContentSize.x),
            static_cast<int32>(ContentSize.y));

        GuiState.bViewportHostVisible = true;
        GuiState.ViewportHostRect = HostRect;
        EditorEngine->GetViewportLayout().SetHostRect(HostRect);

        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
		for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            auto& VP = Layout.GetSceneViewport(i);
            const FViewportRect ViewportRect = VP.GetRect();
            if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
            {
                continue;
            }

            const ID3D11ShaderResourceView* SceneColorSRV = VP.GetOutSRV();

            ImVec2 Size = ImVec2(
                static_cast<float>(ViewportRect.Width),
                static_cast<float>(ViewportRect.Height));
            ImGui::SetCursorScreenPos(ImVec2(
                static_cast<float>(ViewportRect.X),
                static_cast<float>(ViewportRect.Y)));

            if (SceneColorSRV)
            {
                ID3D11DeviceContext* DeviceContext = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
                ImDrawList* DrawList = ImGui::GetWindowDrawList();

                DrawList->AddCallback(SetOpaqueBlendStateCallback, DeviceContext);
                ImGui::Image(reinterpret_cast<ImTextureID>(SceneColorSRV), Size);
                DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }
            else
            {
                ImGui::Dummy(Size);
            }
        }

        // 뷰포트별 독립 메뉴바 오버레이
        {
            FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
            constexpr float MenuBarH = 34.0f;

            for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
            {
                if (FEditorViewportClient* Client = Layout.GetViewportClient(i))
                {
                    Client->SetViewportInputDeadZoneTop(MenuBarH);
                }

                FViewportRect ViewportRect = Layout.GetSceneViewport(i).GetRect();
                if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
                    continue;

                const float LocalX = static_cast<float>(ViewportRect.X - HostRect.X);
                const float LocalY = static_cast<float>(ViewportRect.Y - HostRect.Y);
                if (LocalX < 0.0f || LocalY < 0.0f)
                    continue;

                ImGui::SetCursorScreenPos(ImVec2(ContentPos.x + LocalX, ContentPos.y + LocalY));

                char ChildID[32];
                snprintf(ChildID, sizeof(ChildID), "##VPMenu%d", i);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 0.98f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.26f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.29f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.30f, 0.53f, 1.0f));
                constexpr ImGuiWindowFlags OverlayFlags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoFocusOnAppearing;

                if (ImGui::BeginChild(ChildID, ImVec2(static_cast<float>(ViewportRect.Width), MenuBarH), false, OverlayFlags))
                {
                    ImGui::PushID(i);
                    RenderViewportIconToolbarForIndex(i);
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(4);
            }
        }

        TickViewportContextMenu();
        RenderViewportContextMenu();
    }
    else
    {
        GuiState.bViewportHostVisible = false;
        GuiState.ViewportHostRect = FViewportRect();
        EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
    }

    ImGui::End();
}

void FEditorMainPanel::TickViewportContextMenu()
{
    if (!EditorEngine || !Window || ImGui::IsPopupOpen("##ViewportContextMenu"))
    {
        return;
    }

    InputSystem& IS = InputSystem::Get();
    POINT MouseScreenPos = IS.GetMousePos();
    POINT MouseClientPos = Window->ScreenToClientPoint(MouseScreenPos);
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();

    auto FindViewportAtClientPoint = [&Layout](const POINT& ClientPoint) -> int32
    {
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            const FViewportRect& Rect = Layout.GetSceneViewport(i).GetRect();
            if (Rect.Width <= 0 || Rect.Height <= 0)
            {
                continue;
            }
            if (Rect.Contains(static_cast<int32>(ClientPoint.x), static_cast<int32>(ClientPoint.y)))
            {
                FEditorViewportClient* Client = Layout.GetViewportClient(i);
                const float LocalY = static_cast<float>(ClientPoint.y - Rect.Y);
                if (Client && Client->IsPointerInViewportInputDeadZone(LocalY))
                {
                    return -1;
                }
                return i;
            }
        }
        return -1;
    };

    constexpr float RightClickPopupThresholdPx = 20.0f;
    constexpr float RightClickPopupThresholdSq = RightClickPopupThresholdPx * RightClickPopupThresholdPx;

    if (IS.GetKeyDown(VK_RBUTTON))
    {
        const int32 ViewportIndex = FindViewportAtClientPoint(MouseClientPos);
        if (ViewportIndex >= 0)
        {
            ViewportContextMenuState.bRightClickTracking = true;
            ViewportContextMenuState.TrackingViewportIndex = ViewportIndex;
            ViewportContextMenuState.RightClickTravelSq = 0.0f;
            ViewportContextMenuState.PressScreenPos = MouseScreenPos;
        }
    }

    if (ViewportContextMenuState.bRightClickTracking && IS.GetKey(VK_RBUTTON))
    {
        const float DeltaX = static_cast<float>(IS.MouseDeltaX());
        const float DeltaY = static_cast<float>(IS.MouseDeltaY());
        ViewportContextMenuState.RightClickTravelSq += DeltaX * DeltaX + DeltaY * DeltaY;
    }

    if (!ViewportContextMenuState.bRightClickTracking || !IS.GetKeyUp(VK_RBUTTON))
    {
        return;
    }

    const int32 ReleaseViewportIndex = FindViewportAtClientPoint(MouseClientPos);
    const bool bClickCandidate =
        ReleaseViewportIndex == ViewportContextMenuState.TrackingViewportIndex
        && ViewportContextMenuState.RightClickTravelSq <= RightClickPopupThresholdSq
        && !IS.GetRightDragging()
        && !IS.GetRightDragEnd();
    const bool bHasModifier = IS.GetKey(VK_CONTROL) || IS.GetKey(VK_MENU) || IS.GetKey(VK_SHIFT);

    if (bClickCandidate && !bHasModifier)
    {
        Layout.SetLastFocusedViewportIndex(ViewportContextMenuState.TrackingViewportIndex);
        if (FEditorViewportClient* Client = Layout.GetViewportClient(ViewportContextMenuState.TrackingViewportIndex))
        {
            POINT PressClientPos = Window->ScreenToClientPoint(ViewportContextMenuState.PressScreenPos);
            const FViewportRect& Rect = Layout.GetSceneViewport(ViewportContextMenuState.TrackingViewportIndex).GetRect();
            const float LocalX = MathUtil::Clamp(static_cast<float>(PressClientPos.x - Rect.X), 0.0f, static_cast<float>(std::max(0, Rect.Width - 1)));
            const float LocalY = MathUtil::Clamp(static_cast<float>(PressClientPos.y - Rect.Y), 0.0f, static_cast<float>(std::max(0, Rect.Height - 1)));
            Client->RequestSelectAtViewportLocalPoint(LocalX, LocalY, false, false);
            ViewportContextMenuState.PendingSpawnViewportIndex = ViewportContextMenuState.TrackingViewportIndex;
            ViewportContextMenuState.PendingSpawnLocalX = LocalX;
            ViewportContextMenuState.PendingSpawnLocalY = LocalY;
        }
        ViewportContextMenuState.PendingPopupViewportIndex = ViewportContextMenuState.TrackingViewportIndex;
        ViewportContextMenuState.PendingPopupScreenPos = ImVec2(
            static_cast<float>(ViewportContextMenuState.PressScreenPos.x),
            static_cast<float>(ViewportContextMenuState.PressScreenPos.y + 2));
    }

    ViewportContextMenuState.bRightClickTracking = false;
    ViewportContextMenuState.TrackingViewportIndex = -1;
    ViewportContextMenuState.RightClickTravelSq = 0.0f;
}

void FEditorMainPanel::RenderViewportContextMenu()
{
    if (!EditorEngine)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    if (ViewportContextMenuState.PendingPopupViewportIndex >= 0)
    {
        ImGui::SetNextWindowPos(ViewportContextMenuState.PendingPopupScreenPos, ImGuiCond_Always);
        ImGui::OpenPopup("##ViewportContextMenu");
        ViewportContextMenuState.PendingPopupViewportIndex = -1;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f, 0.0f), ImVec2(260.0f, FLT_MAX));
    if (!ImGui::BeginPopup("##ViewportContextMenu"))
    {
        return;
    }

    const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = Layout.GetViewportClient(FocusedIndex);
    FEditorViewportState& State = Layout.GetViewportState(FocusedIndex);
    const bool bEditing = Client && Client->GetPlayState() == EViewportPlayState::Editing;

    ImGui::TextDisabled("%s", GetViewportSlotName(FocusedIndex));
    ImGui::Separator();

    if (ImGui::BeginMenu("Place Actor", bEditing && Client != nullptr))
    {
        const int32 SpawnViewportIndex =
            ViewportContextMenuState.PendingSpawnViewportIndex >= 0
                ? ViewportContextMenuState.PendingSpawnViewportIndex
                : FocusedIndex;
        FEditorViewportClient* SpawnClient = Layout.GetViewportClient(SpawnViewportIndex);
        const FVector SpawnLocation = ComputePlacementLocation(
            SpawnClient,
            ViewportContextMenuState.PendingSpawnLocalX,
            ViewportContextMenuState.PendingSpawnLocalY);

        for (int32 i = 0; i < ControlWidget.GetPrimitiveTypeCount(); ++i)
        {
            if (ImGui::MenuItem(ControlWidget.GetPrimitiveTypeLabel(i)))
            {
                ControlWidget.SpawnPrimitive(i, SpawnLocation, 1);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Delete", "Del", false, bEditing && Client != nullptr))
    {
        Client->RequestDeleteSelection();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Focus Selection", "F", false, Client != nullptr))
    {
        Client->FocusSelection();
    }

    if (!bEditing)
    {
        if (ImGui::MenuItem("Stop PIE", "Esc"))
        {
            EditorEngine->StopPlaySession();
        }
        ImGui::EndPopup();
        return;
    }

    if (ImGui::BeginMenu("Transform Mode"))
    {
        if (ImGui::MenuItem("Select", "Q / 1")) Client->RequestSetSelectMode();
        if (ImGui::MenuItem("Translate", "W / 2")) Client->RequestSetTranslateMode();
        if (ImGui::MenuItem("Rotate", "E / 3")) Client->RequestSetRotateMode();
        if (ImGui::MenuItem("Scale", "R / 4")) Client->RequestSetScaleMode();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Selection"))
    {
        if (ImGui::MenuItem("Select All", "Ctrl+A")) Client->RequestSelectAllActors();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) Client->RequestDuplicateSelection();
        if (ImGui::MenuItem("Delete", "Delete")) Client->RequestDeleteSelection();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Layout"))
    {
        struct FLayoutMenuItem
        {
            const char* Label;
            EEditorViewportLayoutMode Mode;
        };
        static constexpr FLayoutMenuItem LayoutItems[] =
        {
            { "One Pane", EEditorViewportLayoutMode::OnePane },
            { "Two Panes Horiz", EEditorViewportLayoutMode::TwoPanesHoriz },
            { "Two Panes Vert", EEditorViewportLayoutMode::TwoPanesVert },
            { "Three Panes Left", EEditorViewportLayoutMode::ThreePanesLeft },
            { "Three Panes Right", EEditorViewportLayoutMode::ThreePanesRight },
            { "Three Panes Top", EEditorViewportLayoutMode::ThreePanesTop },
            { "Three Panes Bottom", EEditorViewportLayoutMode::ThreePanesBottom },
            { "Four Panes 2x2", EEditorViewportLayoutMode::FourPanes2x2 },
            { "Four Panes Left", EEditorViewportLayoutMode::FourPanesLeft },
            { "Four Panes Right", EEditorViewportLayoutMode::FourPanesRight },
            { "Four Panes Top", EEditorViewportLayoutMode::FourPanesTop },
            { "Four Panes Bottom", EEditorViewportLayoutMode::FourPanesBottom },
        };

        const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
        for (const FLayoutMenuItem& Item : LayoutItems)
        {
            if (ImGui::MenuItem(Item.Label, nullptr, CurrentMode == Item.Mode))
            {
                Layout.SetLayoutModeAnimated(Item.Mode, Item.Mode == EEditorViewportLayoutMode::OnePane ? FocusedIndex : -1);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Split"))
        {
            Layout.ToggleViewportSplit();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View Mode"))
    {
        static constexpr EViewMode Modes[] = {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_BlinnPhong,
            EViewMode::Unlit,
            EViewMode::Heatmap,
            EViewMode::Wireframe,
            EViewMode::Depth,
            EViewMode::Normal,
        };
        static constexpr const char* Labels[] = {
            "Lit (Gouraud)", "Lit (Lambert)", "Lit (Blinn-Phong)", "Unlit",
            "Heatmap", "Wireframe", "Depth", "Normal"
        };
        for (int32 i = 0; i < static_cast<int32>(EViewMode::Count); ++i)
        {
            if (ImGui::MenuItem(Labels[i], nullptr, State.ViewMode == Modes[i]))
            {
                State.ViewMode = Modes[i];
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

bool FEditorMainPanel::DrawViewportTextButton(const char* Id, const char* Label, bool bPairFirst, bool bPairSecond)
{
    const ImVec2 TextSize = ImGui::CalcTextSize(Label);
    const ImVec2 Padding = ImGui::GetStyle().FramePadding;
    const ImVec2 ButtonSize(TextSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
    const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const bool bHovered = ImGui::IsItemHovered();
    const bool bHeld = ImGui::IsItemActive();
    const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImDrawFlags RoundFlags = ImDrawFlags_RoundCornersAll;
    if (bPairFirst)
    {
        RoundFlags = ImDrawFlags_RoundCornersLeft;
    }
    if (bPairSecond)
    {
        RoundFlags = ImDrawFlags_RoundCornersRight;
    }

    ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, RoundFlags);
    const ImVec2 TextPos(Min.x + (ButtonSize.x - TextSize.x) * 0.5f, Min.y + (ButtonSize.y - TextSize.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Label);
    ImGui::GetWindowDrawList()->AddText(ImVec2(TextPos.x + 0.8f, TextPos.y), ImGui::GetColorU32(ImGuiCol_Text), Label);
    return bPressed;
}

bool FEditorMainPanel::DrawViewportIconButton(const char* Id, EViewportToolIcon Icon, const char* FallbackLabel, const char* Tooltip, bool bSelected, bool bEnabled)
{
    if (!bEnabled)
    {
        ImGui::BeginDisabled();
    }

    if (bSelected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.34f, 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.42f, 0.72f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.50f, 0.82f, 1.0f));
    }

    ID3D11ShaderResourceView* SRV = ViewportToolIcons[static_cast<int32>(Icon)];
    bool bPressed = false;
    if (!SRV)
    {
        bPressed = DrawViewportTextButton(Id, FallbackLabel);
    }
    else
    {
        constexpr ImVec2 IconSize(16.0f, 16.0f);
        const ImVec2 Padding = ImGui::GetStyle().FramePadding;
        const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
        bPressed = ImGui::InvisibleButton(Id, ButtonSize);
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const bool bHovered = ImGui::IsItemHovered();
        const bool bHeld = ImGui::IsItemActive();
        const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding);
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(SRV),
            ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
            ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, bEnabled ? 1.0f : 0.45f)));
    }

    if (bSelected)
    {
        ImGui::PopStyleColor(3);
    }
    if (!bEnabled)
    {
        ImGui::EndDisabled();
    }

    if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
    return bEnabled && bPressed;
}

void FEditorMainPanel::RenderViewportIconToolbarForIndex(int32 ViewportIndex)
{
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client)
    {
        return;
    }

    const bool bEditing = Client->GetPlayState() == EViewportPlayState::Editing;

    ImGui::PushID(ViewportIndex);
    constexpr float ToolbarLeftPadding = 8.0f;
    const float CenteredToolbarY = std::max(0.0f, (ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::SetCursorPos(ImVec2(ToolbarLeftPadding, CenteredToolbarY));
    if (DrawViewportIconButton("##SelectMode", EViewportToolIcon::Select, "Q", "Select (Q / 1)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Select, bEditing))
    {
        Client->RequestSetSelectMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##TranslateMode", EViewportToolIcon::Translate, "W", "Translate (W / 2)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Translate, bEditing))
    {
        Client->RequestSetTranslateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##RotateMode", EViewportToolIcon::Rotate, "E", "Rotate (E / 3)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Rotate, bEditing))
    {
        Client->RequestSetRotateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##ScaleMode", EViewportToolIcon::Scale, "R", "Scale (R / 4)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Scale, bEditing))
    {
        Client->RequestSetScaleMode();
    }

    ImGui::SameLine(0.0f, 10.0f);
    {
        const ImVec2 SeparatorStart = ImGui::GetCursorScreenPos();
        const float SeparatorHeight = ImGui::GetFrameHeight() - 4.0f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f),
            ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f + SeparatorHeight),
            IM_COL32(155, 155, 155, 255),
            1.0f);
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));
    }

    ImGui::SameLine(0.0f, 10.0f);
    const bool bWorldSpace = !Client->GetGizmo() || Client->GetGizmo()->IsWorldSpace();
    if (DrawViewportIconButton("##SpaceMode", bWorldSpace ? EViewportToolIcon::WorldSpace : EViewportToolIcon::LocalSpace, bWorldSpace ? "W" : "L", bWorldSpace ? "World Space (X)" : "Local Space (X)", bWorldSpace, bEditing))
    {
        Client->RequestToggleCoordinateSpace();
    }

    static bool GTranslateSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static bool GRotateSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static bool GScaleSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static int32 GTranslateSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static int32 GRotateSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static int32 GScaleSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static constexpr const char* TranslateSnapLabels[] = { "1", "5", "10", "50", "100" };
    static constexpr const char* RotateSnapLabels[] = { "5", "10", "15", "30", "45" };
    static constexpr const char* ScaleSnapLabels[] = { "0.1", "0.25", "0.5", "1.0", "5.0" };
    static constexpr float TranslateSnapValues[] = { 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
    static constexpr float RotateSnapValues[] = { 5.0f, 10.0f, 15.0f, 30.0f, 45.0f };
    static constexpr float ScaleSnapValues[] = { 0.1f, 0.25f, 0.5f, 1.0f, 5.0f };

    auto DrawSnapSection = [&](EViewportToolIcon SnapIcon, const char* Prefix, bool& bEnabled, int32& ValueIndex, const char* const* Labels, int32 LabelCount)
    {
        ImGui::SameLine(0.0f, 6.0f);
        if (bEnabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.43f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.50f, 0.36f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.36f, 0.26f, 1.0f));
        }

        char ToggleID[48];
        snprintf(ToggleID, sizeof(ToggleID), "##%sSnapToggle", Prefix);
        const bool bTogglePressed = DrawViewportIconButton(ToggleID, SnapIcon, Prefix, Prefix, false, bEditing);
        if (bEnabled)
        {
            ImGui::PopStyleColor(3);
        }
        if (bTogglePressed)
        {
            bEnabled = !bEnabled;
        }

        ImGui::SameLine(0.0f, 0.0f);
        char PopupID[48];
        snprintf(PopupID, sizeof(PopupID), "##%sSnapPopup", Prefix);
        char ValueBtnID[48];
        snprintf(ValueBtnID, sizeof(ValueBtnID), "##%sSnapValueButton", Prefix);
        char Label[24];
        snprintf(Label, sizeof(Label), "%s ▼", Labels[ValueIndex]);
        if (DrawViewportTextButton(ValueBtnID, Label, false, true))
        {
            ImGui::OpenPopup(PopupID);
        }
        if (ImGui::BeginPopup(PopupID))
        {
            for (int32 i = 0; i < LabelCount; ++i)
            {
                const bool bSelected = (ValueIndex == i);
                if (ImGui::RadioButton(Labels[i], bSelected))
                {
                    ValueIndex = i;
                }
            }
            ImGui::EndPopup();
        }
    };

    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        DrawSnapSection(EViewportToolIcon::TranslateSnap, "T", GTranslateSnapEnabled[ViewportIndex], GTranslateSnapIndex[ViewportIndex], TranslateSnapLabels, IM_ARRAYSIZE(TranslateSnapLabels));
        DrawSnapSection(EViewportToolIcon::RotateSnap, "R", GRotateSnapEnabled[ViewportIndex], GRotateSnapIndex[ViewportIndex], RotateSnapLabels, IM_ARRAYSIZE(RotateSnapLabels));
        DrawSnapSection(EViewportToolIcon::ScaleSnap, "S", GScaleSnapEnabled[ViewportIndex], GScaleSnapIndex[ViewportIndex], ScaleSnapLabels, IM_ARRAYSIZE(ScaleSnapLabels));
    }

    if (Client->GetGizmo() && Layout.GetLastFocusedViewportIndex() == ViewportIndex)
    {
        Client->GetGizmo()->SetTranslateSnap(GTranslateSnapEnabled[ViewportIndex], TranslateSnapValues[GTranslateSnapIndex[ViewportIndex]]);
        Client->GetGizmo()->SetRotateSnap(GRotateSnapEnabled[ViewportIndex], RotateSnapValues[GRotateSnapIndex[ViewportIndex]]);
        Client->GetGizmo()->SetScaleSnap(GScaleSnapEnabled[ViewportIndex], ScaleSnapValues[GScaleSnapIndex[ViewportIndex]]);
    }

    ImGui::SameLine();
    if (DrawViewportIconButton("##FrameSelection", EViewportToolIcon::Camera, "F", "Focus Selection (F)", false, true))
    {
        Client->FocusSelection();
    }

    char TypePopupID[48];
    snprintf(TypePopupID, sizeof(TypePopupID), "##ViewportTypePopup_%d", ViewportIndex);
    char TypeButtonLabel[64];
    snprintf(TypeButtonLabel, sizeof(TypeButtonLabel), "%s ▼", GetViewportTypeName(Client->GetViewportType()));
    char ViewPopupID[48];
    snprintf(ViewPopupID, sizeof(ViewPopupID), "##ViewportViewPopup_%d", ViewportIndex);
    char ViewButtonLabel[80];
    snprintf(ViewButtonLabel, sizeof(ViewButtonLabel), "%s ▼", GetViewModeName(Client->GetViewportState()->ViewMode));
    char StatsPopupID[48];
    snprintf(StatsPopupID, sizeof(StatsPopupID), "##ViewportStatsPopup_%d", ViewportIndex);

    const ImVec2 FramePadding = ImGui::GetStyle().FramePadding;
    auto CalcTextButtonWidth = [&](const char* Label) -> float
    {
        return ImGui::CalcTextSize(Label).x + FramePadding.x * 2.0f;
    };
    constexpr float IconButtonWidth = 16.0f + 5.0f * 2.0f;
    float RightGroupWidth = 0.0f;
    int32 RightItemCount = 0;
    auto AddRightItemWidth = [&](float Width)
    {
        if (RightItemCount > 0)
        {
            RightGroupWidth += ImGui::GetStyle().ItemSpacing.x;
        }
        RightGroupWidth += Width;
        ++RightItemCount;
    };
    AddRightItemWidth(CalcTextButtonWidth(TypeButtonLabel));
    AddRightItemWidth(CalcTextButtonWidth(ViewButtonLabel));
    AddRightItemWidth(CalcTextButtonWidth("Stats ▼"));
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);

    const float RightStartX = ImGui::GetWindowContentRegionMax().x - RightGroupWidth;
    const float CurrentCursorX = ImGui::GetCursorPosX();
    const bool bUseOverflowMenu = RightStartX <= CurrentCursorX + 8.0f;
    const ImVec2 WindowScreenPos = ImGui::GetWindowPos();
    auto SetToolbarItemScreenPos = [&](float LocalX)
    {
        ImGui::SetCursorScreenPos(ImVec2(WindowScreenPos.x + std::max(0.0f, LocalX), WindowScreenPos.y + CenteredToolbarY));
    };

    if (bUseOverflowMenu)
    {
        const float OverflowStartX = ImGui::GetWindowContentRegionMax().x - IconButtonWidth;
        SetToolbarItemScreenPos(OverflowStartX);
        if (DrawViewportIconButton("##ViewportToolbarOverflow", EViewportToolIcon::Menu, "...", "Viewport Toolbar", false, true))
        {
            ImGui::OpenPopup("##ViewportToolbarOverflowPopup");
        }

        if (ImGui::BeginPopup("##ViewportToolbarOverflowPopup"))
        {
            if (ImGui::BeginMenu("Type"))
            {
                if (ViewportIndex == 0)
                {
                    ImGui::MenuItem("Perspective", nullptr, true, false);
                }
                else
                {
                    static constexpr EEditorViewportType OrthoTypes[] = {
                        EVT_OrthoTop, EVT_OrthoBottom,
                        EVT_OrthoFront, EVT_OrthoBack,
                        EVT_OrthoLeft, EVT_OrthoRight
                    };
                    for (EEditorViewportType Type : OrthoTypes)
                    {
                        if (ImGui::MenuItem(GetViewportTypeName(Type), nullptr, Client->GetViewportType() == Type))
                        {
                            Client->SetViewportType(Type);
                            Client->ApplyCameraMode();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                static constexpr EViewMode Modes[] = {
                    EViewMode::Lit_Gouraud,
                    EViewMode::Lit_Lambert,
                    EViewMode::Lit_BlinnPhong,
                    EViewMode::Unlit,
                    EViewMode::Heatmap,
                    EViewMode::Wireframe,
                    EViewMode::Depth,
                    EViewMode::Normal,
                };
                for (EViewMode Mode : Modes)
                {
                    if (ImGui::MenuItem(GetViewModeName(Mode), nullptr, Client->GetViewportState()->ViewMode == Mode))
                    {
                        Client->GetViewportState()->ViewMode = Mode;
                    }
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("Light Culling"))
                {
                    static constexpr ELightCullMode CullModes[] = {
                        ELightCullMode::Clustered,
                        ELightCullMode::Tiled,
                        ELightCullMode::None,
                    };
                    for (ELightCullMode CullMode : CullModes)
                    {
                        if (ImGui::MenuItem(GetLightCullModeName(CullMode), nullptr, Client->GetViewportState()->LightCullMode == CullMode))
                        {
                            Client->GetViewportState()->LightCullMode = CullMode;
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Stats"))
            {
                FEditorViewportState* ViewportState = Client->GetViewportState();
                ImGui::MenuItem("FPS", nullptr, &ViewportState->bShowStatFPS);
                ImGui::MenuItem("Memory", nullptr, &ViewportState->bShowStatMemory);
                ImGui::MenuItem("Cascade Vis", nullptr, &ViewportState->bShowCascadeVis);
                ImGui::MenuItem("Light", nullptr, &ViewportState->bShowLight);
                ImGui::MenuItem("Shadow", nullptr, &ViewportState->bShowShadow);
                ImGui::EndMenu();
            }

            const bool bSettingsVisible = ViewportOverlayWidget.IsViewportSettingsVisible();
            if (ImGui::MenuItem("Viewport Settings", nullptr, bSettingsVisible))
            {
                ViewportOverlayWidget.SetViewportSettingsVisible(!bSettingsVisible);
            }

            if (ImGui::BeginMenu("Layout"))
            {
                struct FLayoutMenuItem
                {
                    const char* Label;
                    EEditorViewportLayoutMode Mode;
                };
                static constexpr FLayoutMenuItem LayoutItems[] =
                {
                    { "One Pane", EEditorViewportLayoutMode::OnePane },
                    { "Two Panes Horiz", EEditorViewportLayoutMode::TwoPanesHoriz },
                    { "Two Panes Vert", EEditorViewportLayoutMode::TwoPanesVert },
                    { "Three Panes Left", EEditorViewportLayoutMode::ThreePanesLeft },
                    { "Three Panes Right", EEditorViewportLayoutMode::ThreePanesRight },
                    { "Three Panes Top", EEditorViewportLayoutMode::ThreePanesTop },
                    { "Three Panes Bottom", EEditorViewportLayoutMode::ThreePanesBottom },
                    { "Four Panes 2x2", EEditorViewportLayoutMode::FourPanes2x2 },
                    { "Four Panes Left", EEditorViewportLayoutMode::FourPanesLeft },
                    { "Four Panes Right", EEditorViewportLayoutMode::FourPanesRight },
                    { "Four Panes Top", EEditorViewportLayoutMode::FourPanesTop },
                    { "Four Panes Bottom", EEditorViewportLayoutMode::FourPanesBottom },
                };

                const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
                for (const FLayoutMenuItem& Item : LayoutItems)
                {
                    if (ImGui::MenuItem(Item.Label, nullptr, CurrentMode == Item.Mode))
                    {
                        Layout.SetLayoutModeAnimated(Item.Mode, Item.Mode == EEditorViewportLayoutMode::OnePane ? ViewportIndex : -1);
                    }
                }
                ImGui::EndMenu();
            }

            const EEditorViewportLayoutMode CurrentLayout = Layout.GetLayoutMode();
            if (ImGui::MenuItem(CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split Viewport" : "Merge Viewport"))
            {
                Layout.SetLastFocusedViewportIndex(ViewportIndex);
                Layout.ToggleViewportSplit();
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
        return;
    }

    SetToolbarItemScreenPos(RightStartX);

    if (DrawViewportTextButton("##ViewportTypeButton", TypeButtonLabel))
    {
        ImGui::OpenPopup(TypePopupID);
    }
    if (ImGui::BeginPopup(TypePopupID))
    {
        if (ViewportIndex == 0)
        {
            ImGui::MenuItem("Perspective", nullptr, true, false);
        }
        else
        {
            static constexpr EEditorViewportType kOrthoTypes[] = {
                EVT_OrthoTop, EVT_OrthoBottom,
                EVT_OrthoFront, EVT_OrthoBack,
                EVT_OrthoLeft, EVT_OrthoRight
            };
            for (EEditorViewportType Type : kOrthoTypes)
            {
                if (ImGui::MenuItem(GetViewportTypeName(Type), nullptr, Client->GetViewportType() == Type))
                {
                    Client->SetViewportType(Type);
                    Client->ApplyCameraMode();
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (DrawViewportTextButton("##ViewportViewButton", ViewButtonLabel))
    {
        ImGui::OpenPopup(ViewPopupID);
    }
    if (ImGui::BeginPopup(ViewPopupID))
    {
        static constexpr EViewMode Modes[] = {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_BlinnPhong,
            EViewMode::Unlit,
            EViewMode::Heatmap,
            EViewMode::Wireframe,
            EViewMode::Depth,
            EViewMode::Normal,
        };
        for (EViewMode Mode : Modes)
        {
            if (ImGui::MenuItem(GetViewModeName(Mode), nullptr, Client->GetViewportState()->ViewMode == Mode))
            {
                Client->GetViewportState()->ViewMode = Mode;
            }
        }

        ImGui::Separator();
        if (ImGui::BeginMenu("Light Culling"))
        {
            static constexpr ELightCullMode CullModes[] = {
                ELightCullMode::Clustered,
                ELightCullMode::Tiled,
                ELightCullMode::None,
            };
            for (ELightCullMode CullMode : CullModes)
            {
                if (ImGui::MenuItem(GetLightCullModeName(CullMode), nullptr, Client->GetViewportState()->LightCullMode == CullMode))
                {
                    Client->GetViewportState()->LightCullMode = CullMode;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (DrawViewportTextButton("##ViewportStatsButton", "Stats ▼"))
    {
        ImGui::OpenPopup(StatsPopupID);
    }
    if (ImGui::BeginPopup(StatsPopupID))
    {
        FEditorViewportState* ViewportState = Client->GetViewportState();
        ImGui::MenuItem("FPS", nullptr, &ViewportState->bShowStatFPS);
        ImGui::MenuItem("Memory", nullptr, &ViewportState->bShowStatMemory);
        ImGui::MenuItem("Cascade Vis", nullptr, &ViewportState->bShowCascadeVis);
        ImGui::MenuItem("Light", nullptr, &ViewportState->bShowLight);
        ImGui::MenuItem("Shadow", nullptr, &ViewportState->bShowShadow);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (DrawViewportIconButton("##ViewportSettings", EViewportToolIcon::Setting, "S", "Viewport Settings", ViewportOverlayWidget.IsViewportSettingsVisible(), true))
    {
        ViewportOverlayWidget.SetViewportSettingsVisible(!ViewportOverlayWidget.IsViewportSettingsVisible());
    }

    ImGui::SameLine();
    if (DrawViewportIconButton("##LayoutIconMenu", EViewportToolIcon::Menu, "L", "Layout Presets", false, true))
    {
        ImGui::OpenPopup("##LayoutIconPopup");
    }

    if (ImGui::BeginPopup("##LayoutIconPopup"))
    {
        constexpr int32 Columns = 4;
        constexpr ImVec2 IconSize(32.0f, 32.0f);
        const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
        for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
        {
            ImGui::PushID(i);
            const EEditorViewportLayoutMode Mode = static_cast<EEditorViewportLayoutMode>(i);
            const bool bSelected = (Mode == CurrentMode);
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.34f, 0.62f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.42f, 0.72f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.50f, 0.82f, 1.0f));
            }

            bool bPressed = false;
            if (ViewportLayoutIcons[i])
            {
                bPressed = ImGui::ImageButton("##LayoutIcon", reinterpret_cast<void*>(ViewportLayoutIcons[i]), IconSize);
            }
            else
            {
                bPressed = ImGui::Button(GetViewportLayoutLabel(Mode), ImVec2(110.0f, 0.0f));
            }

            if (bSelected)
            {
                ImGui::PopStyleColor(3);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", GetViewportLayoutLabel(Mode));
            }
            if (bPressed)
            {
                    Layout.SetLayoutModeAnimated(Mode, Mode == EEditorViewportLayoutMode::OnePane ? ViewportIndex : -1);
                ImGui::CloseCurrentPopup();
            }

            if ((i + 1) % Columns != 0)
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    const EEditorViewportLayoutMode CurrentLayout = Layout.GetLayoutMode();
    const EEditorViewportLayoutMode ToggleLayout =
        CurrentLayout == EEditorViewportLayoutMode::OnePane
            ? EEditorViewportLayoutMode::FourPanes2x2
            : EEditorViewportLayoutMode::OnePane;
    const int32 ToggleLayoutIndex = static_cast<int32>(ToggleLayout);
    ID3D11ShaderResourceView* ToggleIcon =
        (ToggleLayoutIndex >= 0 && ToggleLayoutIndex < static_cast<int32>(EEditorViewportLayoutMode::Max))
            ? ViewportLayoutIcons[ToggleLayoutIndex]
            : nullptr;
    const char* ToggleTooltip = CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split Viewport" : "Merge Viewport";
    bool bTogglePressed = false;
    if (ToggleIcon)
    {
        constexpr ImVec2 IconSize(16.0f, 16.0f);
        const ImVec2 Padding = ImGui::GetStyle().FramePadding;
        const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
        bTogglePressed = ImGui::InvisibleButton("##SplitMergeViewport", ButtonSize);
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const bool bHovered = ImGui::IsItemHovered();
        const bool bHeld = ImGui::IsItemActive();
        const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding);
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(ToggleIcon),
            ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
            ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
    }
    else
    {
        bTogglePressed = DrawViewportTextButton("##SplitMergeViewportText", CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split" : "Merge");
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", ToggleTooltip);
    }
    if (bTogglePressed)
    {
        Layout.SetLastFocusedViewportIndex(ViewportIndex);
        Layout.ToggleViewportSplit();
    }
    ImGui::PopID();
}

// 개별 뷰포트 메뉴바 렌더링 — Index 번 뷰포트에 대한 Layout / Type / View / Stats 메뉴
void FEditorMainPanel::RenderViewportMenuBarForIndex(int32 Index)
{
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(Index);
    FEditorViewportState& State = Layout.GetViewportState(Index);

    ImGui::TextDisabled("%s | %s | %s",
                        GetViewportSlotName(Index),
                        GetViewportTypeName(Client->GetViewportType()),
                        GetViewModeName(State.ViewMode));
    ImGui::SameLine();

    if (ImGui::BeginMenu("Layout"))
    {
        struct FLayoutMenuItem
        {
            const char* Label;
            EEditorViewportLayoutMode Mode;
        };
        static constexpr FLayoutMenuItem LayoutItems[] =
        {
            { "One Pane", EEditorViewportLayoutMode::OnePane },
            { "Two Panes Horiz", EEditorViewportLayoutMode::TwoPanesHoriz },
            { "Two Panes Vert", EEditorViewportLayoutMode::TwoPanesVert },
            { "Three Panes Left", EEditorViewportLayoutMode::ThreePanesLeft },
            { "Three Panes Right", EEditorViewportLayoutMode::ThreePanesRight },
            { "Three Panes Top", EEditorViewportLayoutMode::ThreePanesTop },
            { "Three Panes Bottom", EEditorViewportLayoutMode::ThreePanesBottom },
            { "Four Panes 2x2", EEditorViewportLayoutMode::FourPanes2x2 },
            { "Four Panes Left", EEditorViewportLayoutMode::FourPanesLeft },
            { "Four Panes Right", EEditorViewportLayoutMode::FourPanesRight },
            { "Four Panes Top", EEditorViewportLayoutMode::FourPanesTop },
            { "Four Panes Bottom", EEditorViewportLayoutMode::FourPanesBottom },
        };

        const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
        for (const FLayoutMenuItem& Item : LayoutItems)
        {
            const bool bSelected = (CurrentMode == Item.Mode);
            if (ImGui::MenuItem(Item.Label, nullptr, bSelected))
            {
                Layout.SetLayoutModeAnimated(Item.Mode, Item.Mode == EEditorViewportLayoutMode::OnePane ? Index : -1);
            }
        }

        if (Layout.IsSingleViewportMode())
        {
            ImGui::Separator();
            for (int32 j = 0; j < FEditorViewportLayout::MaxViewports; ++j)
            {
                const bool bSel = (Layout.GetSingleViewportIndex() == j);
                if (ImGui::MenuItem(GetViewportSlotName(j), nullptr, bSel))
                {
                    Layout.SetSingleViewportMode(true, j);
                    Layout.SetLastFocusedViewportIndex(j);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Split"))
        {
            Layout.ToggleViewportSplit();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Type"))
    {
        if (Index == 0)
        {
            ImGui::TextDisabled("Viewport 0 is fixed to Perspective.");
            ImGui::Separator();
            ImGui::MenuItem("Perspective", nullptr, true, false);
        }
        else
        {
            static constexpr EEditorViewportType kOrthoTypes[] = {
                EVT_OrthoTop, EVT_OrthoBottom,
                EVT_OrthoFront, EVT_OrthoBack,
                EVT_OrthoLeft, EVT_OrthoRight
            };
            for (EEditorViewportType Type : kOrthoTypes)
            {
                const bool bSel = (Client->GetViewportType() == Type);
                if (ImGui::MenuItem(GetViewportTypeName(Type), nullptr, bSel))
                {
                    Client->SetViewportType(Type);
                    Client->ApplyCameraMode();
                }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        static constexpr EViewMode Modes[] = {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_BlinnPhong,
            EViewMode::Unlit,
			EViewMode::Heatmap,
            EViewMode::Wireframe,
            EViewMode::Depth,
			EViewMode::Normal,
        };
        static constexpr const char* Labels[] = { "Lit (Gouraud)", "Lit (Lambert)", "Lit (Blinn-Phong)", "Unlit", "Heatmap", "Wireframe", "Depth", "Normal" };

        for (int32 j = 0; j < static_cast<int32>(EViewMode::Count); ++j)
        {
            const bool bSel = (State.ViewMode == Modes[j]);
            if (ImGui::MenuItem(Labels[j], nullptr, bSel))
                State.ViewMode = Modes[j];
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Light Culling"))
        {
            static constexpr ELightCullMode CullModes[] = {
                ELightCullMode::Clustered,
				ELightCullMode::Tiled,
                ELightCullMode::None,
            };
            for (ELightCullMode CullMode : CullModes)
            {
                const bool bSel = (State.LightCullMode == CullMode);
                if (ImGui::MenuItem(GetLightCullModeName(CullMode), nullptr, bSel))
                    State.LightCullMode = CullMode;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Stats"))
    {
        ImGui::MenuItem("FPS", nullptr, &State.bShowStatFPS);
        ImGui::MenuItem("Memory", nullptr, &State.bShowStatMemory);
        ImGui::MenuItem("Cascade Vis", nullptr, &State.bShowCascadeVis);
        ImGui::MenuItem("Light", nullptr, &State.bShowLight);
        ImGui::MenuItem("Shadow", nullptr, &State.bShowShadow);
        ImGui::EndMenu();
    }
}
