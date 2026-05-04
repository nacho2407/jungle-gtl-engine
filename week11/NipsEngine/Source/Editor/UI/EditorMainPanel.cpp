

#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Packaging/GamePackager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Runtime/Script/ScriptManager.h"
#include "Engine/Core/Paths.h"
#include "Core/ResourceManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "WICTextureLoader.h"

#include <commdlg.h>

#include "Render/Renderer/Renderer.h"
#include "Render/Resource/Shader.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Serialization/PrefabManager.h"
#include "Serialization/SceneSaveManager.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cwctype>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <future>
namespace
{
constexpr const char* PackagingPopupName = "Packaging Settings";
constexpr int32 DefaultPIEUILayoutWidth = 1920;
constexpr int32 DefaultPIEUILayoutHeight = 1080;

FString MakeFooterSceneDisplayPath(const FString& FilePath)
{
    if (FilePath.empty())
    {
        return {};
    }

    std::filesystem::path SceneDir = std::filesystem::path(FSceneSaveManager::GetSceneDirectory()).lexically_normal();
    std::filesystem::path ScenePath = std::filesystem::path(FPaths::ToWide(FilePath)).lexically_normal();
    std::error_code Ec;
    std::filesystem::path RelativePath = std::filesystem::relative(ScenePath, SceneDir.parent_path(), Ec);
    FString Display = Ec ? FPaths::ToUtf8(ScenePath.filename().wstring()) : FPaths::ToUtf8(RelativePath.wstring());
    std::replace(Display.begin(), Display.end(), '/', '\\');
    return Display;
}

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

FString FormatHistoryBytes(size_t Bytes)
{
    char Buffer[64];
    const double Value = static_cast<double>(Bytes);
    if (Bytes >= 1024ull * 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f MB", Value / (1024.0 * 1024.0));
    }
    else if (Bytes >= 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f KB", Value / 1024.0);
    }
    else
    {
        snprintf(Buffer, sizeof(Buffer), "%zu B", Bytes);
    }
    return Buffer;
}

float GetCameraBaseSpeed()
{
    return std::max(0.1f, FEditorSettings::Get().CameraSpeed);
}

float GetCameraSpeedMultiplier(FEditorViewportClient* Client)
{
    return Client ? Client->GetMoveSpeed() / GetCameraBaseSpeed() : 1.0f;
}

void SetCameraSpeedMultiplier(FEditorViewportClient* Client, float Multiplier)
{
    if (!Client)
    {
        return;
    }
    Client->SetMoveSpeed(MathUtil::Clamp(GetCameraBaseSpeed() * Multiplier, 0.1f, 5000.0f));
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

    if (Camera->IsOrthographic())
    {
        FVector PlaneNormal = FVector::UpVector;
        switch (Client->GetViewportType())
        {
        case EVT_OrthoTop:
        case EVT_OrthoBottom:
            PlaneNormal = FVector::UpVector;
            break;
        case EVT_OrthoFront:
        case EVT_OrthoBack:
            PlaneNormal = FVector::ForwardVector;
            break;
        case EVT_OrthoLeft:
        case EVT_OrthoRight:
            PlaneNormal = FVector::RightVector;
            break;
        default:
            PlaneNormal = Camera->GetEffectiveForward().GetSafeNormal();
            break;
        }

        const float Denom = FVector::DotProduct(Ray.Direction, PlaneNormal);
        if (std::fabs(Denom) > 0.0001f)
        {
            const float T = FVector::DotProduct(-Ray.Origin, PlaneNormal) / Denom;
            if (T >= 0.0f)
            {
                return Ray.Origin + Ray.Direction * T;
            }
        }

        FVector Projected = Ray.Origin;
        switch (Client->GetViewportType())
        {
        case EVT_OrthoTop:
        case EVT_OrthoBottom:
            Projected.Z = 0.0f;
            break;
        case EVT_OrthoFront:
        case EVT_OrthoBack:
            Projected.X = 0.0f;
            break;
        case EVT_OrthoLeft:
        case EVT_OrthoRight:
            Projected.Y = 0.0f;
            break;
        default:
            break;
        }
        return Projected;
    }

    const FVector Forward = Camera->GetEffectiveForward().GetSafeNormal();
    const FVector PlanePoint = Camera->GetLocation() + Forward * 10.0f;
    const float Denom = FVector::DotProduct(Ray.Direction, Forward);
    if (std::fabs(Denom) > 0.0001f)
    {
        const float T = FVector::DotProduct(PlanePoint - Ray.Origin, Forward) / Denom;
        if (T > 0.0f)
        {
            return Ray.Origin + Ray.Direction * T;
        }
    }

    return PlanePoint;
}

bool HasParentDirectoryReference(const std::filesystem::path& Path)
{
    for (const std::filesystem::path& Part : Path)
    {
        if (Part == L"..")
        {
            return true;
        }
    }
    return false;
}

std::wstring GetLowerExtension(const std::filesystem::path& Path)
{
    std::wstring Extension = Path.extension().wstring();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
    return Extension;
}

FString ResolveStaticMeshDropLoadPath(const FString& PayloadPath)
{
    std::filesystem::path Path(FPaths::ToWide(PayloadPath));
    const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
    if (!Path.is_absolute())
    {
        Path = RootPath / Path;
    }
    Path = Path.lexically_normal();

    const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);
    if (RelativePath.empty() || HasParentDirectoryReference(RelativePath))
    {
        return {};
    }

    const std::wstring Extension = GetLowerExtension(Path);
    if (Extension == L".obj")
    {
        return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
    }
    if (Extension == L".bin")
    {
        return FPaths::Normalize(FPaths::ToUtf8(Path.generic_wstring()));
    }
    return {};
}

FString NormalizePackagingScenePath(const FString& ScenePath)
{
    if (ScenePath.empty())
    {
        return {};
    }

    std::filesystem::path Path(FPaths::ToWide(ScenePath));
    if (Path.is_absolute())
    {
        return FPaths::Normalize(FPaths::ToRelativeString(Path.wstring()));
    }
    return FPaths::Normalize(ScenePath);
}

bool AddUniquePackagingScene(TArray<FString>& Scenes, const FString& ScenePath)
{
    const FString NormalizedScene = NormalizePackagingScenePath(ScenePath);
    if (NormalizedScene.empty())
    {
        return false;
    }

    for (const FString& Existing : Scenes)
    {
        if (FPaths::Normalize(Existing) == NormalizedScene)
        {
            return false;
        }
    }

    Scenes.push_back(NormalizedScene);
    return true;
}

bool OpenPackagingAssetFileDialog(const wchar_t* Filter, FString& OutFilePath)
{
    OutFilePath.clear();

    WCHAR FileBuffer[MAX_PATH] = {};
    OPENFILENAMEW DialogDesc = {};
    DialogDesc.lStructSize = sizeof(DialogDesc);
    DialogDesc.hwndOwner = ImGui::GetMainViewport()
        ? static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw)
        : nullptr;
    DialogDesc.lpstrFilter = Filter;
    DialogDesc.lpstrFile = FileBuffer;
    DialogDesc.nMaxFile = MAX_PATH;
    DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    const BOOL bPicked = GetOpenFileNameW(&DialogDesc);
    std::error_code RestoreEc;
    std::filesystem::current_path(PrevCwd, RestoreEc);
    if (!bPicked)
    {
        return false;
    }

    OutFilePath = FPaths::ToRelativeString(FileBuffer);
    if (OutFilePath.empty())
    {
        OutFilePath = FPaths::ToUtf8(FileBuffer);
    }
    return true;
}

std::wstring ToLowerPathExtension(const FString& Path)
{
    std::wstring Ext = std::filesystem::path(FPaths::ToWide(Path)).extension().wstring();
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](wchar_t Ch)
    {
        return static_cast<wchar_t>(std::towlower(Ch));
    });
    return Ext;
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
    ContentBrowserWidget.Initialize(InEditorEngine);
    ControlWidget.Initialize(InEditorEngine);
    MaterialWidget.Initialize(InEditorEngine);
    PropertyWidget.Initialize(InEditorEngine);
    SceneWidget.Initialize(InEditorEngine);
    ViewportOverlayWidget.Initialize(InEditorEngine);
    StatWidget.Initialize(InEditorEngine);
    PlayStreamWidget.Initialize(InEditorEngine);
    ToolbarWidget.Initialize(InEditorEngine);
    RuntimeUIPreviewWidget.Initialize(InEditorEngine);
    RuntimeUIPreviewWidget.SetRmlRenderQueue(
        [this](const FRuntimeUIRenderContext& Context)
        {
            PendingPIERmlUiRenderContexts.push_back(Context);
        });
    ToolbarWidget.SetViewportOverlayWidget(&ViewportOverlayWidget);
    ToolbarWidget.SetSceneWidget(&SceneWidget);
    ToolbarWidget.SetPlayStreamWidget(&PlayStreamWidget);
    ToolbarWidget.SetPIEViewportFullscreenCallback([this](bool bEnabled) { SetPIEViewportFullscreenEnabled(bEnabled); });
    ToolbarWidget.SetBuildGameCallback([this]() { RequestBuildGame(); });
    ToolbarWidget.SetPanelVisibilityRefs(&bShowConsole, &bShowControl, &bShowProperty, &bShowSceneManager,
                                         &bShowMaterialEditor, &bShowStatProfiler, &bShowEditorDebug,
                                         &bShowContentBrowser, &bShowUndoHistory, &bShowRuntimeUIPreview,
                                         &bPIEViewportFullscreenEnabled);
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
    if (!SaveIconSRV)
    {
        const std::wstring SaveIconPath = IconDir + L"Save.png";
        DirectX::CreateWICTextureFromFile(Device, SaveIconPath.c_str(), nullptr, &SaveIconSRV);
    }
    if (!HotReloadIconSRV)
    {
        const std::wstring HotReloadIconPath = IconDir + L"HotReload.png";
        DirectX::CreateWICTextureFromFile(Device, HotReloadIconPath.c_str(), nullptr, &HotReloadIconSRV);
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
    if (SaveIconSRV)
    {
        SaveIconSRV->Release();
        SaveIconSRV = nullptr;
    }
    if (HotReloadIconSRV)
    {
        HotReloadIconSRV->Release();
        HotReloadIconSRV = nullptr;
    }
}

void FEditorMainPanel::Render(float DeltaTime)
{
    PendingPIERmlUiRenderContexts.clear();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    TickBuildGameTask();

    const ImGuiIO& IO = ImGui::GetIO();
    if (!IO.WantTextInput && IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        ToggleContentBrowser();
    }
    else
    {
        ContentBrowserWidget.SetVisible(bShowContentBrowser);
    }

    const bool bContentBrowserVisibleBeforeMenu = bShowContentBrowser;
    ToolbarWidget.Render(DeltaTime);
    if (!bContentBrowserVisibleBeforeMenu && bShowContentBrowser)
    {
        OpenContentBrowser();
    }
    else if (bContentBrowserVisibleBeforeMenu && !bShowContentBrowser)
    {
        CloseContentBrowser();
    }
    RenderEditorToolbar();
    RenderDockSpace();

    RenderViewportHostWindow();
    ViewportOverlayWidget.RenderViewportFrameOverlays(DeltaTime);

    const bool bDrawEditorPanels = !bHideEditorWindowsForPIE;
    if (bDrawEditorPanels && bShowControl)
        ControlWidget.Render(DeltaTime);
    if (bDrawEditorPanels && bShowMaterialEditor)
        MaterialWidget.Render(DeltaTime);
    if (bDrawEditorPanels && bShowProperty)
        PropertyWidget.Render(DeltaTime);
    if (bDrawEditorPanels && bShowSceneManager)
        SceneWidget.Render(DeltaTime);
    if (bDrawEditorPanels && bShowStatProfiler)
        StatWidget.Render(DeltaTime);
    if (bDrawEditorPanels)
        RenderEditorDebugPanel(DeltaTime);
    if (bDrawEditorPanels)
        RenderUndoHistoryPanel(DeltaTime);
    if (bDrawEditorPanels && bShowRuntimeUIPreview)
        RuntimeUIPreviewWidget.Render(DeltaTime);
    if (bDrawEditorPanels && bShowConsole && ConsoleWidget.IsFloatingWindowMode())
        ConsoleWidget.Render(DeltaTime);
    RenderBuildGameModal();
    ViewportOverlayWidget.RenderFloatingOverlays(DeltaTime);

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
    if (bDrawEditorPanels)
    {
        ContentBrowserWidget.Render(DeltaTime);
        bShowContentBrowser = ContentBrowserWidget.IsVisible();
        HandleContentBrowserViewportDrop();
    }
    RenderConsoleDrawer(DeltaTime);
    RenderFooterOverlay(DeltaTime);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    for (const FRuntimeUIRenderContext& Context : PendingPIERmlUiRenderContexts)
    {
        if (EditorEngine)
        {
            EditorEngine->RenderRuntimeUI(Context);
        }
    }
    PendingPIERmlUiRenderContexts.clear();
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
    if (!bHasSavedPIEPanelVisibility)
    {
        SavedPIEPanelVisibility.bShowConsole = bShowConsole;
        SavedPIEPanelVisibility.bShowControl = bShowControl;
        SavedPIEPanelVisibility.bShowProperty = bShowProperty;
        SavedPIEPanelVisibility.bShowSceneManager = bShowSceneManager;
        SavedPIEPanelVisibility.bShowMaterialEditor = bShowMaterialEditor;
        SavedPIEPanelVisibility.bShowStatProfiler = bShowStatProfiler;
        SavedPIEPanelVisibility.bShowPlayStream = bShowPlayStream;
        SavedPIEPanelVisibility.bShowEditorDebug = bShowEditorDebug;
        SavedPIEPanelVisibility.bShowContentBrowser = bShowContentBrowser;
        SavedPIEPanelVisibility.bConsoleDrawerVisible = bConsoleDrawerVisible;
        SavedPIEPanelVisibility.bViewportSettingsVisible = ViewportOverlayWidget.IsViewportSettingsVisible();
        SavedPIEPanelVisibility.bGroupedStatOverlayVisible = ViewportOverlayWidget.IsGroupedStatOverlayVisible();
        bHasSavedPIEPanelVisibility = true;
    }

    bHideEditorWindowsForPIE = true;
    if (bPIEViewportFullscreenEnabled)
    {
        bShowConsole = false;
        bShowControl = false;
        bShowProperty = false;
        bShowSceneManager = false;
        bShowMaterialEditor = false;
        bShowStatProfiler = false;
        bShowPlayStream = false;
        bShowEditorDebug = false;
        bShowContentBrowser = false;
        ContentBrowserWidget.SetVisible(false);
        bConsoleDrawerVisible = false;
        ViewportOverlayWidget.SetViewportSettingsVisible(false);
        ViewportOverlayWidget.SetGroupedStatOverlayVisible(false);
        ApplyPIEViewportFullscreen();
    }
    else
    {
        bHideEditorWindowsForPIE = false;
    }
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
    bHideEditorWindowsForPIE = false;
    if (!bHasSavedPIEPanelVisibility)
    {
        RestorePIEViewportLayout();
        return;
    }

    bShowConsole = SavedPIEPanelVisibility.bShowConsole;
    bShowControl = SavedPIEPanelVisibility.bShowControl;
    bShowProperty = SavedPIEPanelVisibility.bShowProperty;
    bShowSceneManager = SavedPIEPanelVisibility.bShowSceneManager;
    bShowMaterialEditor = SavedPIEPanelVisibility.bShowMaterialEditor;
    bShowStatProfiler = SavedPIEPanelVisibility.bShowStatProfiler;
    bShowPlayStream = SavedPIEPanelVisibility.bShowPlayStream;
    bShowEditorDebug = SavedPIEPanelVisibility.bShowEditorDebug;
    bShowContentBrowser = SavedPIEPanelVisibility.bShowContentBrowser;
    ContentBrowserWidget.SetVisible(bShowContentBrowser);
    bConsoleDrawerVisible = SavedPIEPanelVisibility.bConsoleDrawerVisible;
    ViewportOverlayWidget.SetViewportSettingsVisible(SavedPIEPanelVisibility.bViewportSettingsVisible);
    ViewportOverlayWidget.SetGroupedStatOverlayVisible(SavedPIEPanelVisibility.bGroupedStatOverlayVisible);
    bHasSavedPIEPanelVisibility = false;
    RestorePIEViewportLayout();
}

void FEditorMainPanel::SetPIEViewportFullscreenEnabled(bool bEnabled)
{
    if (bPIEViewportFullscreenEnabled == bEnabled)
    {
        return;
    }

    bPIEViewportFullscreenEnabled = bEnabled;
    if (!EditorEngine || EditorEngine->GetEditorState() == EViewportPlayState::Editing)
    {
        return;
    }

    if (bPIEViewportFullscreenEnabled)
    {
        if (!bHasSavedPIEPanelVisibility)
        {
            SavedPIEPanelVisibility.bShowConsole = bShowConsole;
            SavedPIEPanelVisibility.bShowControl = bShowControl;
            SavedPIEPanelVisibility.bShowProperty = bShowProperty;
            SavedPIEPanelVisibility.bShowSceneManager = bShowSceneManager;
            SavedPIEPanelVisibility.bShowMaterialEditor = bShowMaterialEditor;
            SavedPIEPanelVisibility.bShowStatProfiler = bShowStatProfiler;
            SavedPIEPanelVisibility.bShowPlayStream = bShowPlayStream;
            SavedPIEPanelVisibility.bShowEditorDebug = bShowEditorDebug;
            SavedPIEPanelVisibility.bShowContentBrowser = bShowContentBrowser;
            SavedPIEPanelVisibility.bConsoleDrawerVisible = bConsoleDrawerVisible;
            SavedPIEPanelVisibility.bViewportSettingsVisible = ViewportOverlayWidget.IsViewportSettingsVisible();
            SavedPIEPanelVisibility.bGroupedStatOverlayVisible = ViewportOverlayWidget.IsGroupedStatOverlayVisible();
            bHasSavedPIEPanelVisibility = true;
        }

        bHideEditorWindowsForPIE = true;
        bShowConsole = false;
        bShowControl = false;
        bShowProperty = false;
        bShowSceneManager = false;
        bShowMaterialEditor = false;
        bShowStatProfiler = false;
        bShowPlayStream = false;
        bShowEditorDebug = false;
        bShowContentBrowser = false;
        ContentBrowserWidget.SetVisible(false);
        bConsoleDrawerVisible = false;
        ViewportOverlayWidget.SetViewportSettingsVisible(false);
        ViewportOverlayWidget.SetGroupedStatOverlayVisible(false);
        ApplyPIEViewportFullscreen();
    }
    else
    {
        bHideEditorWindowsForPIE = false;
        if (bHasSavedPIEPanelVisibility)
        {
            bShowConsole = SavedPIEPanelVisibility.bShowConsole;
            bShowControl = SavedPIEPanelVisibility.bShowControl;
            bShowProperty = SavedPIEPanelVisibility.bShowProperty;
            bShowSceneManager = SavedPIEPanelVisibility.bShowSceneManager;
            bShowMaterialEditor = SavedPIEPanelVisibility.bShowMaterialEditor;
            bShowStatProfiler = SavedPIEPanelVisibility.bShowStatProfiler;
            bShowPlayStream = SavedPIEPanelVisibility.bShowPlayStream;
            bShowEditorDebug = SavedPIEPanelVisibility.bShowEditorDebug;
            bShowContentBrowser = SavedPIEPanelVisibility.bShowContentBrowser;
            ContentBrowserWidget.SetVisible(bShowContentBrowser);
            bConsoleDrawerVisible = SavedPIEPanelVisibility.bConsoleDrawerVisible;
            ViewportOverlayWidget.SetViewportSettingsVisible(SavedPIEPanelVisibility.bViewportSettingsVisible);
            ViewportOverlayWidget.SetGroupedStatOverlayVisible(SavedPIEPanelVisibility.bGroupedStatOverlayVisible);
            bHasSavedPIEPanelVisibility = false;
        }
        RestorePIEViewportLayout();
    }
}

void FEditorMainPanel::ApplyPIEViewportFullscreen()
{
    if (!EditorEngine || !bPIEViewportFullscreenEnabled)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();

    if (!SavedPIEViewportLayout.bValid)
    {
        SavedPIEViewportLayout.bValid = true;
        SavedPIEViewportLayout.LayoutMode = Layout.GetLayoutMode();
        SavedPIEViewportLayout.SingleViewportIndex = Layout.GetSingleViewportIndex();
        SavedPIEViewportLayout.LastFocusedViewportIndex = FocusedIndex;
    }

    Layout.SetLayoutModeAnimated(EEditorViewportLayoutMode::OnePane, FocusedIndex);
}

void FEditorMainPanel::RestorePIEViewportLayout()
{
    if (!EditorEngine || !SavedPIEViewportLayout.bValid)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    Layout.SetLayoutMode(SavedPIEViewportLayout.LayoutMode, SavedPIEViewportLayout.SingleViewportIndex);
    Layout.SetLastFocusedViewportIndex(SavedPIEViewportLayout.LastFocusedViewportIndex);
    SavedPIEViewportLayout = FPIEViewportLayoutSnapshot{};
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
        const FVector ToolbarSpawnLocation = FVector(0.0f, 0.0f, 0.0f);

        const bool bEditing = EditorEngine->GetEditorState() == EViewportPlayState::Editing;
        const bool bCanPlaceActor = Client && Client->AllowsEditorWorldControl();
        const bool bCanSave = bEditing;
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
        if (!bCanSave)
        {
            ImGui::BeginDisabled();
        }
        const bool bSaveClicked = ImGui::InvisibleButton("##ToolbarSaveScene", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 SaveMin = ImGui::GetItemRectMin();
        const ImVec2 SaveMax = ImGui::GetItemRectMax();
        const bool bSaveHovered = bCanSave && ImGui::IsItemHovered();
        const ImU32 SaveBg = ImGui::GetColorU32(bSaveHovered ? ImVec4(0.22f, 0.25f, 0.32f, 1.0f) : ImVec4(0.15f, 0.17f, 0.21f, 1.0f));
        const ImU32 SaveBorder = ImGui::GetColorU32(bCanSave ? ImVec4(0.40f, 0.44f, 0.58f, 1.0f) : ImVec4(0.29f, 0.31f, 0.36f, 1.0f));
        DrawList->AddRectFilled(SaveMin, SaveMax, SaveBg, 5.0f);
        DrawList->AddRect(SaveMin, SaveMax, SaveBorder, 5.0f);
        if (SaveIconSRV)
        {
            DrawList->AddImage(
                reinterpret_cast<ImTextureID>(SaveIconSRV),
                ImVec2(SaveMin.x + 5.0f, SaveMin.y + 5.0f),
                ImVec2(SaveMax.x - 5.0f, SaveMax.y - 5.0f),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, bCanSave ? 1.0f : 0.45f)));
        }
        else
        {
            const ImU32 IconColor = ImGui::GetColorU32(ImVec4(0.84f, 0.88f, 0.96f, bCanSave ? 1.0f : 0.45f));
            DrawList->AddRect(ImVec2(SaveMin.x + 8.0f, SaveMin.y + 7.0f), ImVec2(SaveMax.x - 8.0f, SaveMax.y - 7.0f), IconColor, 2.0f, 0, 1.7f);
            DrawList->AddLine(ImVec2(SaveMin.x + 11.0f, SaveMax.y - 12.0f), ImVec2(SaveMax.x - 11.0f, SaveMax.y - 12.0f), IconColor, 1.7f);
        }
        if (bSaveHovered)
        {
            ImGui::SetTooltip("Save Scene");
        }
        if (bSaveClicked && bCanSave)
        {
            RequestSaveScene();
        }
        if (!bCanSave)
        {
            ImGui::EndDisabled();
        }

        if (!bCanPlaceActor)
        {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPos(ImVec2(46.0f, 5.0f));
        const bool bAddClicked = ImGui::InvisibleButton("##ToolbarAddActor", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 AddMin = ImGui::GetItemRectMin();
        const ImVec2 AddMax = ImGui::GetItemRectMax();
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
            ControlWidget.DrawPlaceActorMenu(ToolbarSpawnLocation);
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

        ImGui::SetCursorPos(ImVec2(CenterX + ButtonSize + Gap * 1.5f, ButtonY));
        const bool bFullscreenOn = IsPIEViewportFullscreenEnabled();
        const bool bFullscreenClicked = ImGui::InvisibleButton("##ToolbarPIEFullscreen", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 FullMin = ImGui::GetItemRectMin();
        const ImVec2 FullMax = ImGui::GetItemRectMax();
        const bool bFullHovered = ImGui::IsItemHovered();
        const ImVec4 FullBgColor = bFullscreenOn
            ? (bFullHovered ? ImVec4(0.20f, 0.31f, 0.46f, 1.0f) : ImVec4(0.15f, 0.24f, 0.37f, 1.0f))
            : (bFullHovered ? ImVec4(0.24f, 0.26f, 0.31f, 1.0f) : ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
        DrawList->AddRectFilled(FullMin, FullMax, ImGui::GetColorU32(FullBgColor), 5.0f);
        DrawList->AddRect(FullMin, FullMax, ImGui::GetColorU32(bFullscreenOn ? ImVec4(0.36f, 0.57f, 0.86f, 1.0f) : ImVec4(0.38f, 0.40f, 0.46f, 1.0f)), 5.0f);
        const ImU32 FullIconColor = ImGui::GetColorU32(bFullscreenOn ? ImVec4(0.64f, 0.82f, 1.00f, 1.0f) : ImVec4(0.72f, 0.76f, 0.84f, 1.0f));
        const float Pad = 8.0f;
        const float Corner = 6.0f;
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMin.y + Pad), ImVec2(FullMin.x + Pad + Corner, FullMin.y + Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMin.y + Pad), ImVec2(FullMin.x + Pad, FullMin.y + Pad + Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMin.y + Pad), ImVec2(FullMax.x - Pad - Corner, FullMin.y + Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMin.y + Pad), ImVec2(FullMax.x - Pad, FullMin.y + Pad + Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMax.y - Pad), ImVec2(FullMin.x + Pad + Corner, FullMax.y - Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMax.y - Pad), ImVec2(FullMin.x + Pad, FullMax.y - Pad - Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMax.y - Pad), ImVec2(FullMax.x - Pad - Corner, FullMax.y - Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMax.y - Pad), ImVec2(FullMax.x - Pad, FullMax.y - Pad - Corner), FullIconColor, 1.8f);
        if (bFullHovered)
        {
            ImGui::SetTooltip(bFullscreenOn ? "PIE Fullscreen Viewport: On" : "PIE Fullscreen Viewport: Off");
        }
        if (bFullscreenClicked)
        {
            SetPIEViewportFullscreenEnabled(!bFullscreenOn);
        }

        if (bCanStop)
        {
            FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
            const int32 PIEViewportIndex = (EditorEngine->GetActivePIEViewportIndex() >= 0)
                ? EditorEngine->GetActivePIEViewportIndex()
                : Layout.GetLastFocusedViewportIndex();
            FEditorViewportClient* PIEClient = Layout.GetViewportClient(PIEViewportIndex);

            ImGui::SetCursorPos(ImVec2(FullMax.x - ImGui::GetWindowPos().x + Gap, ButtonY + 2.0f));
            if (PIEClient)
            {
                char PIEViewPopupID[48];
                snprintf(PIEViewPopupID, sizeof(PIEViewPopupID), "##MainToolbarPIEViewPopup_%d", PIEViewportIndex);
                char PIEViewButtonLabel[80];
                snprintf(PIEViewButtonLabel, sizeof(PIEViewButtonLabel), "View: %s", GetViewModeName(PIEClient->GetViewportState()->ViewMode));

                if (ImGui::Button(PIEViewButtonLabel, ImVec2(132.0f, ButtonSize - 4.0f)))
                {
                    ImGui::OpenPopup(PIEViewPopupID);
                }
                if (ImGui::BeginPopup(PIEViewPopupID))
                {
                    static constexpr EViewMode PIEViewModes[] = {
                        EViewMode::Lit_Gouraud,
                        EViewMode::Unlit,
                        EViewMode::Wireframe,
                        EViewMode::Depth,
                        EViewMode::Normal,
                    };
                    for (EViewMode Mode : PIEViewModes)
                    {
                        if (ImGui::MenuItem(GetViewModeName(Mode), nullptr, PIEClient->GetViewportState()->ViewMode == Mode))
                        {
                            PIEClient->GetViewportState()->ViewMode = Mode;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::SameLine(0.0f, Gap);
            }

            if (ImGui::Checkbox("PIE 1920x1080", &bPIEUseFixedUILayout) && bPIEUseFixedUILayout)
            {
                PIEUILayoutWidth = DefaultPIEUILayoutWidth;
                PIEUILayoutHeight = DefaultPIEUILayoutHeight;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Use a 16:9 PIE viewport and a fixed 1920x1080 RML layout.");
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void FEditorMainPanel::RenderConsoleDrawer(float DeltaTime)
{
    (void)DeltaTime;
    if (ConsoleWidget.IsFloatingWindowMode())
    {
        return;
    }
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

void FEditorMainPanel::OpenConsoleDrawer(bool bFocusInput)
{
    bConsoleDrawerVisible = true;
    ConsoleBacktickCycleState = 2;
    bBringConsoleDrawerToFrontNextFrame = true;
    bFocusConsoleInputNextFrame = bFocusInput;
    CloseContentBrowser();
}

void FEditorMainPanel::CloseConsoleDrawer()
{
    bConsoleDrawerVisible = false;
    ConsoleBacktickCycleState = 0;
    bFocusConsoleInputNextFrame = false;
}

void FEditorMainPanel::OpenContentBrowser()
{
    bShowContentBrowser = true;
    ContentBrowserWidget.OpenAssetRoot();
    ContentBrowserWidget.SetVisible(true);
    CloseConsoleDrawer();
}

void FEditorMainPanel::CloseContentBrowser()
{
    bShowContentBrowser = false;
    ContentBrowserWidget.SetVisible(false);
}

void FEditorMainPanel::ToggleContentBrowser()
{
    if (bShowContentBrowser)
    {
        CloseContentBrowser();
    }
    else
    {
        OpenContentBrowser();
    }
}

void FEditorMainPanel::OpenMaterialAsset(UMaterialInterface* Material)
{
    if (!Material)
    {
        return;
    }

    bShowMaterialEditor = true;
    MaterialWidget.OpenMaterialAsset(Material);
}

void FEditorMainPanel::OpenMaterialSlot(UPrimitiveComponent* PrimitiveComp, int32 SlotIndex)
{
    if (!PrimitiveComp)
    {
        return;
    }

    bShowMaterialEditor = true;
    MaterialWidget.OpenMaterialSlot(PrimitiveComp, SlotIndex);
}

void FEditorMainPanel::PushFooterLog(const FString& Message)
{
    FooterLogSystem.Push(Message, 5.0f);
}

void FEditorMainPanel::RequestPIEViewportInputFocus()
{
    PendingPIEViewportInputFocusFrames = 3;
    bConsoleDrawerVisible = false;
    bFocusConsoleInputNextFrame = false;
    bFocusConsoleButtonNextFrame = false;
}

bool FEditorMainPanel::CanCloseEditor()
{
    return SceneWidget.PromptSaveIfDirty();
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
                CloseContentBrowser();
                break;
            case 1:
                OpenConsoleDrawer(true);
                break;
            default:
                CloseConsoleDrawer();
                bFocusConsoleInputNextFrame = false;
                bFocusConsoleButtonNextFrame = true;
                break;
            }
        }

        const char* ContentLabel = bShowContentBrowser
            ? (ContentBrowserWidget.IsDrawerMode() ? "Content ▼" : "Content Window")
            : "Content ▲";
        if (ImGui::Button(ContentLabel))
        {
            ToggleContentBrowser();
        }

        ImGui::SameLine();
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
            const char* ConsoleLabel = ConsoleWidget.IsFloatingWindowMode()
                ? "Console Window"
                : (bDrawerOpen ? "Console ▼" : "Console ▲");
            if (ImGui::Button(ConsoleLabel))
            {
                if (ConsoleWidget.IsFloatingWindowMode())
                {
                    ConsoleWidget.SetPresentationMode(FEditorConsoleWidget::EPresentationMode::Drawer);
                    OpenConsoleDrawer(true);
                }
                else if (!bConsoleDrawerVisible)
                {
                    OpenConsoleDrawer(true);
                }
                else
                {
                    CloseConsoleDrawer();
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
        FString SceneLabel = FString("Level: ") + SceneWidget.GetCurrentSceneDisplayPath();
        if (EditorEngine->GetEditorState() != EViewportPlayState::Editing)
        {
            const FString ActiveScenePath = EditorEngine->GetCurrentScenePath();
            if (!ActiveScenePath.empty())
            {
                SceneLabel += FString(" | ActiveScene: ") + MakeFooterSceneDisplayPath(ActiveScenePath);
            }
        }
        ImGui::TextDisabled("%s", SceneLabel.c_str());

		ImGui::SameLine();

        {
            const char* Label = "Hot Reload";
            ImGuiStyle& Style = ImGui::GetStyle();
            const float IconSize = 18.0f;
            const float ButtonWidth = 120.0f;
            const float TotalWidth = IconSize + Style.ItemSpacing.x + ButtonWidth;
            const float RightX = ImGui::GetWindowContentRegionMax().x;
            const float ButtonStartX = RightX - TotalWidth;

            if (!ActiveLogs.empty())
            {
                const FString& LatestFooterLog = ActiveLogs.back();
                const float LogWidth = ImGui::CalcTextSize(LatestFooterLog.c_str()).x;
                const float MinLogX = ImGui::GetCursorPosX() + 16.0f;
                const float LogRightPadding = 18.0f;
                const float LogX = std::max(MinLogX, ButtonStartX - LogRightPadding - LogWidth);
                ImGui::SetCursorPosX(LogX);
                ImGui::TextUnformatted(LatestFooterLog.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", LatestFooterLog.c_str());
                }
                ImGui::SameLine();
            }

            ImGui::SetCursorPosX(ButtonStartX);
            ImGui::Image((ImTextureID)HotReloadIconSRV, ImVec2(IconSize, IconSize));
            ImGui::SameLine();

            if (ImGui::Button(Label, ImVec2(ButtonWidth, 0.0f)))
            {
                FScriptManager::Get().HotReloadScripts();
                PushFooterLog("Lua scripts hot reloaded");
            }
        }
    }

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorMainPanel::RenderUndoHistoryPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!bShowUndoHistory || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Undo History", &bShowUndoHistory))
    {
        ImGui::End();
        return;
    }

    const TArray<FUndoSnapshotEntry>& UndoEntries = EditorEngine->GetUndoHistory();
    const TArray<FUndoSnapshotEntry>& RedoEntries = EditorEngine->GetRedoHistory();
    const FUndoHistoryStats HistoryStats = EditorEngine->GetUndoHistoryStats();

    const bool bCanUndo = !UndoEntries.empty();
    const bool bCanRedo = !RedoEntries.empty();
    ImGui::BeginDisabled(!bCanUndo);
    if (ImGui::Button("Undo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->Undo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanRedo);
    if (ImGui::Button("Redo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->Redo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanUndo && !bCanRedo);
    if (ImGui::Button("Clear", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->ClearUndoHistory();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Stat History", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Entries: %d / %d", HistoryStats.UndoCount + HistoryStats.RedoCount, HistoryStats.MaxEntries);
        ImGui::TextDisabled("Undo %d, Redo %d", HistoryStats.UndoCount, HistoryStats.RedoCount);
        ImGui::Text("Snapshot Data: %s", FormatHistoryBytes(HistoryStats.LogicalBytes).c_str());
        ImGui::Text("Reserved Memory: %s", FormatHistoryBytes(HistoryStats.ReservedBytes).c_str());
        ImGui::TextDisabled("Approx Total: %s", FormatHistoryBytes(HistoryStats.ApproxTotalBytes).c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Approx Total = string reserved capacity + entry storage. Scene restore also creates a temporary world only while undo/redo is executing.");
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("Undo");
    ImGui::BeginChild("##UndoHistoryList", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.62f), true);
    if (UndoEntries.empty())
    {
        ImGui::TextDisabled("No undo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(UndoEntries.size()) - 1; Index >= 0; --Index)
        {
            ImGui::PushID(Index);
            const FString Label = UndoEntries[Index].Label.empty() ? FString("Scene Edit") : UndoEntries[Index].Label;
            if (ImGui::Selectable(Label.c_str()))
            {
                EditorEngine->RestoreUndoHistoryIndex(Index);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::TextDisabled("Redo");
    ImGui::BeginChild("##RedoHistoryList", ImVec2(0.0f, 0.0f), true);
    if (RedoEntries.empty())
    {
        ImGui::TextDisabled("No redo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(RedoEntries.size()) - 1; Index >= 0; --Index)
        {
            const FString Label = RedoEntries[Index].Label.empty() ? FString("Scene Edit") : RedoEntries[Index].Label;
            ImGui::TextUnformatted(Label.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void FEditorMainPanel::RenderEditorDebugPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!bShowEditorDebug || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor Debug", &bShowEditorDebug))
    {
        ImGui::End();
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Camera Base Speed", &Settings.CameraSpeed, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Camera Rotate Speed", &Settings.CameraRotationSpeed, 1.0f, 1.0f, 720.0f, "%.0f");
        ImGui::DragFloat("Camera Zoom Speed", &Settings.CameraZoomSpeed, 1.0f, 10.0f, 5000.0f, "%.0f");
        ImGui::DragFloat("Dolly Speed Scale", &Settings.CameraDollySpeedScale, 0.01f, 0.05f, 5.0f, "%.2fx");
        ImGui::DragFloat("Pan Speed Scale", &Settings.CameraPanSpeedScale, 0.05f, 0.05f, 10.0f, "%.2fx");
        ImGui::Checkbox("Camera Smoothing", &Settings.bEnableCameraSmoothing);
        ImGui::BeginDisabled(!Settings.bEnableCameraSmoothing);
        ImGui::DragFloat("Move Smooth Speed", &Settings.CameraMoveSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::DragFloat("Rotate Smooth Speed", &Settings.CameraRotateSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::EndDisabled();

        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        if (FEditorViewportClient* FocusedClient = Layout.GetViewportClient(Layout.GetLastFocusedViewportIndex()))
        {
            float SpeedMultiplier = GetCameraSpeedMultiplier(FocusedClient);
            if (ImGui::DragFloat("Focused Speed Multiplier", &SpeedMultiplier, 0.05f, 0.01f, 500.0f, "%.2fx"))
            {
                SetCameraSpeedMultiplier(FocusedClient, SpeedMultiplier);
            }
        }
    }

    if (ImGui::CollapsingHeader("Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
        ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
        ImGui::Checkbox("Axis", &Settings.ShowFlags.bAxis);
        ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
        ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);
        ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);
        if (Settings.ShowFlags.bBoundingVolume)
        {
            ImGui::Indent();
            ImGui::Checkbox("BVH Bounding Volume", &Settings.ShowFlags.bBVHBoundingVolume);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Enable LOD", &Settings.ShowFlags.bEnableLOD);
        ImGui::Checkbox("Decals", &Settings.ShowFlags.bDecals);
        ImGui::Checkbox("Fog", &Settings.ShowFlags.bFog);
        ImGui::Checkbox("Shadow", &Settings.ShowFlags.bShadow);
        ImGui::Checkbox("FXAA", &Settings.bEnableFXAA);
    }

    if (ImGui::CollapsingHeader("Place Actors (Grid)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const int32 PrimitiveCount = ControlWidget.GetPrimitiveTypeCount();
        DebugGridPrimitiveType = MathUtil::Clamp(DebugGridPrimitiveType, 0, PrimitiveCount - 1);

        if (ImGui::BeginCombo("Actor Type", ControlWidget.GetPrimitiveTypeLabel(DebugGridPrimitiveType)))
        {
            for (int32 i = 0; i < PrimitiveCount; ++i)
            {
                const bool bSelected = (DebugGridPrimitiveType == i);
                if (ImGui::Selectable(ControlWidget.GetPrimitiveTypeLabel(i), bSelected))
                {
                    DebugGridPrimitiveType = i;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::DragInt("Rows", &DebugGridRows, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Cols", &DebugGridCols, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Layers", &DebugGridLayers, 1.0f, 1, 32, "%d");
        ImGui::DragFloat("Grid Spacing", &DebugGridSpacing, 0.1f, 0.1f, 1000.0f, "%.2f");
        ImGui::Checkbox("Center Grid Around Origin", &bDebugGridCenter);
        ImGui::DragFloat3("Origin", &DebugGridOrigin.X, 0.1f, -100000.0f, 100000.0f, "%.2f");

        DebugGridRows = MathUtil::Clamp(DebugGridRows, 1, 128);
        DebugGridCols = MathUtil::Clamp(DebugGridCols, 1, 128);
        DebugGridLayers = MathUtil::Clamp(DebugGridLayers, 1, 32);
        DebugGridSpacing = std::max(0.1f, DebugGridSpacing);

        const int32 TotalActors = DebugGridRows * DebugGridCols * DebugGridLayers;
        ImGui::Text("Total Actors: %d", TotalActors);
        if (TotalActors > 2048)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Large grid; spawn is capped at 2048 per click.");
        }

        if (ImGui::Button("Spawn Grid Actors"))
        {
            const float RowOffset = bDebugGridCenter ? static_cast<float>(DebugGridRows - 1) * 0.5f : 0.0f;
            const float ColOffset = bDebugGridCenter ? static_cast<float>(DebugGridCols - 1) * 0.5f : 0.0f;
            const float LayerOffset = bDebugGridCenter ? static_cast<float>(DebugGridLayers - 1) * 0.5f : 0.0f;
            const int32 SpawnLimit = std::min(TotalActors, 2048);
            int32 SpawnedCount = 0;

            for (int32 Layer = 0; Layer < DebugGridLayers && SpawnedCount < SpawnLimit; ++Layer)
            {
                for (int32 Row = 0; Row < DebugGridRows && SpawnedCount < SpawnLimit; ++Row)
                {
                    for (int32 Col = 0; Col < DebugGridCols && SpawnedCount < SpawnLimit; ++Col)
                    {
                        const FVector Location(
                            DebugGridOrigin.X + (static_cast<float>(Col) - ColOffset) * DebugGridSpacing,
                            DebugGridOrigin.Y + (static_cast<float>(Row) - RowOffset) * DebugGridSpacing,
                            DebugGridOrigin.Z + (static_cast<float>(Layer) - LayerOffset) * DebugGridSpacing);
                        if (ControlWidget.SpawnPrimitive(DebugGridPrimitiveType, Location, 1))
                        {
                            ++SpawnedCount;
                        }
                    }
                }
            }

            if (UWorld* World = EditorEngine->GetFocusedWorld())
            {
                World->RebuildSpatialIndex();
            }
            FEditorConsoleWidget::AddLog(
                "Editor Debug grid spawned %d %s actors\n",
                SpawnedCount,
                ControlWidget.GetPrimitiveTypeLabel(DebugGridPrimitiveType));
        }
    }

    ImGui::End();
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
    const bool bMouseOverContentBrowser = ContentBrowserWidget.IsMouseOverBrowser();
    bool bViewportOperationActive = Layout.HasActiveOperationViewport() && !bMouseOverContentBrowser;

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
    const bool bAnyDragDropActive = ImGui::GetDragDropPayload() != nullptr;
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
        && !bMouseOverContentBrowser
        && !bAnyUIItemActive
        && !bAnyPopupOpen)
    {
        bHoveredViewportContentWindow = true;
    }

    if (bMouseOverContentBrowser)
    {
        bHoveredViewportContentWindow = false;
        bHoveredNonViewportWindow = true;
    }

    const bool bForcePIEViewportInputFocus =
        PendingPIEViewportInputFocusFrames > 0
        && EditorEngine
        && EditorEngine->GetEditorState() == EViewportPlayState::Playing;

    const bool bReleaseMouseToViewport =
        bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bAnyUIItemActive
        && !bAnyDragDropActive
        && !bAnyPopupOpen;
    const bool bNonViewportImGuiInteraction =
        bMouseOverContentBrowser
        ||
        (bHoveredNonViewportWindow && (bAnyWindowHovered || bAnyWindowFocused || bAnyUIItemActive || bAnyUIItemHovered || bAnyPopupOpen || bWantTextInput || bWantKeyboard))
        || bAnyUIItemActive
        || bAnyDragDropActive
        || bAnyPopupOpen;

    if (bNonViewportImGuiInteraction)
    {
        bWantMouse = true;
    }
    else if (EditorEngine && bReleaseMouseToViewport)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    if (bForcePIEViewportInputFocus)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    InputSystem::Get().SetGuiMouseCapture(bWantMouse);
    InputSystem::Get().SetGuiKeyboardCapture(bWantKeyboard);
    InputSystem::Get().SetGuiTextInputCapture(bForcePIEViewportInputFocus ? false : bWantTextInput);
    const bool bAllowViewportMouseFocus =
        (bForcePIEViewportInputFocus || bMouseOverViewportRect) &&
        !bHoveredNonViewportWindow &&
        !bAnyPopupOpen &&
        !bAnyDragDropActive &&
        !bWantTextInput;
    InputSystem::Get().SetGuiViewportMouseBlock(
        bForcePIEViewportInputFocus
            ? false
            : (bAnyDragDropActive ||
               bAnyPopupOpen ||
               bMouseOverContentBrowser ||
               bHoveredNonViewportWindow));
    InputSystem::Get().SetGuiViewportMouseFocusAllowed(bAllowViewportMouseFocus);

    if (bForcePIEViewportInputFocus)
    {
        --PendingPIEViewportInputFocusFrames;
    }

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
    if (!SceneWidget.PromptSaveIfDirty())
    {
        return false;
    }

    FString PickedPath;
    if (!ToolbarWidget.OpenSceneFileDialog(PickedPath))
    {
        return false;
    }

    return SceneWidget.LoadSceneFromFilePath(PickedPath, false);
}

bool FEditorMainPanel::RequestSaveScene()
{
    return SceneWidget.SaveScene();
}

bool FEditorMainPanel::RequestSaveSceneAsWithDialog()
{
    FString PickedPath;
    if (!ToolbarWidget.SaveSceneFileDialog(PickedPath))
    {
        return false;
    }

    return SceneWidget.SaveSceneToFilePath(PickedPath);
}

void FEditorMainPanel::RequestBuildGame()
{
    if (bBuildGameInProgress)
    {
        PushFooterLog("Packaging already in progress");
        return;
    }

    if (SceneWidget.IsSceneDirty() && !SceneWidget.PromptSaveIfDirty())
    {
        PushFooterLog("Packaging canceled");
        return;
    }

    PendingBuildSettings.GameName = "NipsGame";
    PendingBuildSettings.StartupScene = SceneWidget.HasCurrentSceneFilePath()
        ? SceneWidget.GetCurrentSceneFilePath()
        : "Asset/Scene/Default.scene";
    PendingBuildSettings.OutputDirectory = "Builds/Windows/" + PendingBuildSettings.GameName;
    PendingBuildSettings.PlayerControllerClass = "APlayerController";
    PendingBuildSettings.Configuration = EGameBuildConfiguration::Development;
    PendingBuildSettings.IconPath.clear();
    PendingBuildSettings.SplashImagePath.clear();
    PendingBuildSettings.SplashMinSeconds = 3.0f;
    PendingBuildSettings.bCleanOutput = true;
    PendingBuildSettings.bRunAfterBuild = false;
    PendingBuildSettings.IncludedScenes.clear();
    AddUniquePackagingScene(PendingBuildSettings.IncludedScenes, PendingBuildSettings.StartupScene);

    strncpy_s(BuildGameNameBuffer, PendingBuildSettings.GameName.c_str(), _TRUNCATE);
    strncpy_s(BuildStartupSceneBuffer, PendingBuildSettings.StartupScene.c_str(), _TRUNCATE);
    strncpy_s(BuildSceneListAddBuffer, "", _TRUNCATE);
    strncpy_s(BuildPlayerControllerClassBuffer, PendingBuildSettings.PlayerControllerClass.c_str(), _TRUNCATE);
    strncpy_s(BuildOutputDirectoryBuffer, PendingBuildSettings.OutputDirectory.c_str(), _TRUNCATE);
    strncpy_s(BuildIconPathBuffer, PendingBuildSettings.IconPath.c_str(), _TRUNCATE);
    strncpy_s(BuildSplashImagePathBuffer, PendingBuildSettings.SplashImagePath.c_str(), _TRUNCATE);

    bOpenBuildGameModal = true;
}

void FEditorMainPanel::TickBuildGameTask()
{
    if (!bBuildGameInProgress || !BuildGameFuture.valid())
    {
        return;
    }

    if (BuildGameFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }

    const FGamePackageResult Result = BuildGameFuture.get();
    bBuildGameInProgress = false;
    PushFooterLog(Result.bSucceeded ? "Game package created" : Result.Message);
}

void FEditorMainPanel::RenderBuildGameModal()
{
    if (bOpenBuildGameModal)
    {
        ImGui::OpenPopup(PackagingPopupName);
        bOpenBuildGameModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(PackagingPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    if (ImGui::BeginTable("##PackagingSettingsTable", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##PackageGameName", BuildGameNameBuffer, IM_ARRAYSIZE(BuildGameNameBuffer));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Startup Scene");
        ImGui::TableSetColumnIndex(1);
        const float BrowseButtonWidth = 76.0f;
        ImGui::SetNextItemWidth(-(BrowseButtonWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##PackageStartupScene", BuildStartupSceneBuffer, IM_ARRAYSIZE(BuildStartupSceneBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse##StartupScene", ImVec2(BrowseButtonWidth, 0.0f)))
        {
            FString PickedPath;
            if (ToolbarWidget.OpenSceneFileDialog(PickedPath))
            {
                const FString RelativePath = FPaths::ToRelativeString(FPaths::ToWide(PickedPath));
                strncpy_s(BuildStartupSceneBuffer, RelativePath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Player Controller");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##PackagePlayerController", BuildPlayerControllerClassBuffer, IM_ARRAYSIZE(BuildPlayerControllerClassBuffer));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Output Directory");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##PackageOutputDirectory", BuildOutputDirectoryBuffer, IM_ARRAYSIZE(BuildOutputDirectoryBuffer));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game Icon");
        ImGui::TableSetColumnIndex(1);
        const float BrandingBrowseWidth = 76.0f;
        ImGui::SetNextItemWidth(-(BrandingBrowseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##PackageGameIcon", BuildIconPathBuffer, IM_ARRAYSIZE(BuildIconPathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse##PackageGameIcon", ImVec2(BrandingBrowseWidth, 0.0f)))
        {
            FString PickedPath;
            if (OpenPackagingAssetFileDialog(L"Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0", PickedPath))
            {
                strncpy_s(BuildIconPathBuffer, PickedPath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Splash Image");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-(BrandingBrowseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##PackageSplashImage", BuildSplashImagePathBuffer, IM_ARRAYSIZE(BuildSplashImagePathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse##PackageSplashImage", ImVec2(BrandingBrowseWidth, 0.0f)))
        {
            FString PickedPath;
            if (OpenPackagingAssetFileDialog(L"Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0", PickedPath))
            {
                strncpy_s(BuildSplashImagePathBuffer, PickedPath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Splash Seconds");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::DragFloat("##PackageSplashSeconds", &PendingBuildSettings.SplashMinSeconds, 0.05f, 3.0f, 10.0f, "%.2f");
        PendingBuildSettings.SplashMinSeconds = MathUtil::Clamp(PendingBuildSettings.SplashMinSeconds, 3.0f, 10.0f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Configuration");
        ImGui::TableSetColumnIndex(1);
        int BuildConfigIndex = PendingBuildSettings.Configuration == EGameBuildConfiguration::Development ? 0 : 1;
        const char* ConfigItems[] = { "Development (GameClientDebug)", "Shipping (GameClientRelease)" };
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##PackageConfiguration", &BuildConfigIndex, ConfigItems, IM_ARRAYSIZE(ConfigItems)))
        {
            PendingBuildSettings.Configuration = BuildConfigIndex == 0
                ? EGameBuildConfiguration::Development
                : EGameBuildConfiguration::Shipping;
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Scenes to Copy");
    ImGui::TextDisabled("Startup scene is always included. Add extra scenes that should be copied with the package.");
    const float SceneButtonWidth = 76.0f;
    const float SceneAddButtonWidth = 52.0f;
    ImGui::SetNextItemWidth(-(SceneButtonWidth + SceneAddButtonWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f));
    ImGui::InputText("##PackageSceneToAdd", BuildSceneListAddBuffer, IM_ARRAYSIZE(BuildSceneListAddBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse##PackageSceneToAdd", ImVec2(SceneButtonWidth, 0.0f)))
    {
        FString PickedPath;
        if (ToolbarWidget.OpenSceneFileDialog(PickedPath))
        {
            const FString RelativePath = FPaths::ToRelativeString(FPaths::ToWide(PickedPath));
            strncpy_s(BuildSceneListAddBuffer, RelativePath.c_str(), _TRUNCATE);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##PackageSceneToAdd", ImVec2(SceneAddButtonWidth, 0.0f)))
    {
        if (AddUniquePackagingScene(PendingBuildSettings.IncludedScenes, BuildSceneListAddBuffer))
        {
            strncpy_s(BuildSceneListAddBuffer, "", _TRUNCATE);
        }
    }
    if (ImGui::Button("Add Startup Scene"))
    {
        AddUniquePackagingScene(PendingBuildSettings.IncludedScenes, BuildStartupSceneBuffer);
    }

    const float SceneListHeight = 118.0f;
    if (ImGui::BeginChild("##PackageSceneList", ImVec2(0.0f, SceneListHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (PendingBuildSettings.IncludedScenes.empty())
        {
            ImGui::TextDisabled("No extra scenes added.");
        }
        for (int32 SceneIndex = 0; SceneIndex < static_cast<int32>(PendingBuildSettings.IncludedScenes.size()); ++SceneIndex)
        {
            ImGui::PushID(SceneIndex);
            ImGui::TextUnformatted(PendingBuildSettings.IncludedScenes[SceneIndex].c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 58.0f);
            if (ImGui::SmallButton("Remove"))
            {
                PendingBuildSettings.IncludedScenes.erase(PendingBuildSettings.IncludedScenes.begin() + SceneIndex);
                --SceneIndex;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("Clean Output", &PendingBuildSettings.bCleanOutput);
    ImGui::SameLine();
    ImGui::Checkbox("Run After Packaging", &PendingBuildSettings.bRunAfterBuild);

    const FString GameName = FPaths::Normalize(BuildGameNameBuffer);
    const FString StartupScene = FPaths::Normalize(BuildStartupSceneBuffer);
    const FString PlayerControllerClass = FPaths::Normalize(BuildPlayerControllerClassBuffer);
    const FString OutputDirectory = FPaths::Normalize(BuildOutputDirectoryBuffer);
    const FString IconPath = FPaths::Normalize(BuildIconPathBuffer);
    const FString SplashImagePath = FPaths::Normalize(BuildSplashImagePathBuffer);
    const bool bValidGameName = !GameName.empty();
    const bool bValidScene = !StartupScene.empty() && std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(StartupScene)));
    const bool bValidPlayerController = !PlayerControllerClass.empty();
    const bool bValidOutput = !OutputDirectory.empty();
    const std::wstring IconExt = ToLowerPathExtension(IconPath);
    const std::wstring SplashExt = ToLowerPathExtension(SplashImagePath);
    const bool bValidIcon = IconPath.empty() || (std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(IconPath))) && IconExt == L".ico");
    const bool bValidSplashExt = SplashImagePath.empty() || SplashExt == L".png" || SplashExt == L".jpg" || SplashExt == L".jpeg" || SplashExt == L".bmp";
    const bool bValidSplash = SplashImagePath.empty() || (std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(SplashImagePath))) && bValidSplashExt);
    bool bValidIncludedScenes = true;
    for (const FString& IncludedScene : PendingBuildSettings.IncludedScenes)
    {
        if (IncludedScene.empty() || !std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(IncludedScene))))
        {
            bValidIncludedScenes = false;
            break;
        }
    }

    ImGui::SeparatorText("Validation");
    ImGui::Text("Configuration: %s",
        PendingBuildSettings.Configuration == EGameBuildConfiguration::Development
            ? "Development -> GameClientDebug|x64"
            : "Shipping -> GameClientRelease|x64");
    ImGui::Text("Output Exe: %s", "NipsGame.exe");
    ImGui::Text("Scenes to Copy: %d", static_cast<int32>(PendingBuildSettings.IncludedScenes.size()));
    ImGui::Text("Icon: %s", IconPath.empty() ? "(none)" : IconPath.c_str());
    ImGui::Text("Splash: %s", SplashImagePath.empty() ? "(none)" : SplashImagePath.c_str());

    if (bValidScene)
    {
        ImGui::TextColored(ImVec4(0.42f, 0.78f, 0.48f, 1.0f), "Startup scene found.");
    }
    if (!bValidScene)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Startup scene does not exist.");
    }
    if (!bValidGameName)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Game name is empty.");
    }
    if (!bValidPlayerController)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Player controller class is empty.");
    }
    if (!bValidOutput)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Output directory is empty.");
    }
    if (!bValidIcon)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Game icon must be an existing .ico file.");
    }
    if (!bValidSplash)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Splash must be an existing png, jpg, jpeg, or bmp file.");
    }
    if (!bValidIncludedScenes)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "One or more scenes to copy do not exist.");
    }
    if (bBuildGameInProgress)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.58f, 0.18f, 1.0f), "Packaging is running. Check Console for live output.");
    }

    ImGui::Separator();
    const bool bCanBuild = bValidGameName && bValidScene && bValidPlayerController && bValidOutput && bValidIcon && bValidSplash && bValidIncludedScenes;
    const bool bDisablePackageButton = !bCanBuild || bBuildGameInProgress;
    if (bDisablePackageButton)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Package", ImVec2(120.0f, 0.0f)))
    {
        PendingBuildSettings.GameName = GameName;
        PendingBuildSettings.StartupScene = StartupScene;
        PendingBuildSettings.PlayerControllerClass = PlayerControllerClass;
        PendingBuildSettings.OutputDirectory = OutputDirectory;
        PendingBuildSettings.IconPath = IconPath;
        PendingBuildSettings.SplashImagePath = SplashImagePath;
        PendingBuildSettings.SplashMinSeconds = MathUtil::Clamp(PendingBuildSettings.SplashMinSeconds, 3.0f, 10.0f);
        AddUniquePackagingScene(PendingBuildSettings.IncludedScenes, StartupScene);
        PushFooterLog("Packaging game...");
        bBuildGameInProgress = true;
        BuildGameFuture = std::async(std::launch::async, [Settings = PendingBuildSettings]()
        {
            return FGamePackager::BuildAndPackage(Settings);
        });
        ImGui::CloseCurrentPopup();
    }
    if (bDisablePackageButton)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

bool FEditorMainPanel::SpawnStaticMeshFromContentPath(const FString& PayloadPath, int32 ViewportIndex, float LocalX, float LocalY)
{
    if (!EditorEngine)
    {
        return false;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client || !Client->AllowsEditorWorldControl())
    {
        return false;
    }

    const FString MeshLoadPath = ResolveStaticMeshDropLoadPath(PayloadPath);
    if (MeshLoadPath.empty())
    {
        PushFooterLog("Unsupported static mesh drop");
        return false;
    }

    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh(MeshLoadPath);
    if (!Mesh || !Mesh->HasValidMeshData())
    {
        PushFooterLog("Failed to load dropped static mesh");
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    EditorEngine->CaptureUndoSnapshot("Place Static Mesh");
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
    if (!Actor)
    {
        return false;
    }

    Actor->InitDefaultComponents();
    Actor->SetActorLocation(ComputePlacementLocation(Client, LocalX, LocalY));
    if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
    {
        StaticMeshComp->SetStaticMesh(Mesh);
    }

    Layout.SetLastFocusedViewportIndex(ViewportIndex);
    EditorEngine->GetSelectionManager().Select(Actor);
    SceneWidget.MarkSceneDirty();
    PushFooterLog("StaticMesh actor placed from Content Browser");
    return true;
}

bool FEditorMainPanel::SpawnPrefabFromContentPath(const FString& PayloadPath, int32 ViewportIndex, float LocalX, float LocalY)
{
    if (!EditorEngine)
    {
        return false;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client || !Client->AllowsEditorWorldControl())
    {
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    std::filesystem::path PrefabPath;
    std::error_code PrefabEc;
    if (!FPrefabManager::ResolvePrefabPath(PayloadPath, PrefabPath, false) || !std::filesystem::is_regular_file(PrefabPath, PrefabEc))
    {
        PushFooterLog("Failed to find prefab");
        return false;
    }

    EditorEngine->CaptureUndoSnapshot("Place Prefab");
    AActor* Actor = FPrefabManager::SpawnActorFromPrefab(World, PayloadPath);
    if (!Actor)
    {
        PushFooterLog("Failed to spawn prefab");
        return false;
    }

    Actor->SetActorLocation(ComputePlacementLocation(Client, LocalX, LocalY));
    World->SyncSpatialIndex();
    Layout.SetLastFocusedViewportIndex(ViewportIndex);
    EditorEngine->GetSelectionManager().Select(Actor);
    SceneWidget.MarkSceneDirty();
    PushFooterLog("Prefab actor placed from Content Browser");
    return true;
}

bool FEditorMainPanel::SpawnPrefabAtOrigin(const FString& PayloadPath)
{
    if (!EditorEngine)
    {
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    std::filesystem::path PrefabPath;
    std::error_code PrefabEc;
    if (!FPrefabManager::ResolvePrefabPath(PayloadPath, PrefabPath, false) || !std::filesystem::is_regular_file(PrefabPath, PrefabEc))
    {
        PushFooterLog("Failed to find prefab");
        return false;
    }

    EditorEngine->CaptureUndoSnapshot("Place Prefab");
    AActor* Actor = FPrefabManager::SpawnActorFromPrefab(World, PayloadPath);
    if (!Actor)
    {
        PushFooterLog("Failed to spawn prefab");
        return false;
    }

    Actor->SetActorLocation(FVector::ZeroVector);
    World->SyncSpatialIndex();
    EditorEngine->GetSelectionManager().Select(Actor);
    SceneWidget.MarkSceneDirty();
    PushFooterLog("Prefab actor placed at origin");
    return true;
}

void FEditorMainPanel::HandleContentBrowserViewportDrop()
{
    FString PayloadType;
    FString PayloadPath;
    if (!ContentBrowserWidget.ConsumeReleasedDragPayload(PayloadType, PayloadPath))
    {
        return;
    }
    if ((PayloadType != "ObjectContentItem" && PayloadType != "PrefabContentItem") || ContentBrowserWidget.IsMouseOverBrowser())
    {
        return;
    }

    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 FocusedViewportIndex = Layout.GetLastFocusedViewportIndex();
    auto TryDropOnViewport = [&](int32 ViewportIndex) -> bool
    {
        FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
        if (!Client || !Client->AllowsEditorWorldControl())
        {
            return false;
        }

        const FViewportRect& Rect = Layout.GetSceneViewport(ViewportIndex).GetRect();
        if (Rect.Width <= 0 || Rect.Height <= 0)
        {
            return false;
        }
        if (MousePos.x < static_cast<float>(Rect.X)
            || MousePos.x >= static_cast<float>(Rect.X + Rect.Width)
            || MousePos.y < static_cast<float>(Rect.Y)
            || MousePos.y >= static_cast<float>(Rect.Y + Rect.Height))
        {
            return false;
        }

        const float LocalX = MathUtil::Clamp(MousePos.x - static_cast<float>(Rect.X), 0.0f, std::max(0.0f, static_cast<float>(Rect.Width - 1)));
        const float LocalY = MathUtil::Clamp(MousePos.y - static_cast<float>(Rect.Y), 0.0f, std::max(0.0f, static_cast<float>(Rect.Height - 1)));
        if (PayloadType == "PrefabContentItem")
        {
            return SpawnPrefabFromContentPath(PayloadPath, ViewportIndex, LocalX, LocalY);
        }
        return SpawnStaticMeshFromContentPath(PayloadPath, ViewportIndex, LocalX, LocalY);
    };

    if (FocusedViewportIndex >= 0 && FocusedViewportIndex < FEditorViewportLayout::MaxViewports && TryDropOnViewport(FocusedViewportIndex))
    {
        return;
    }
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        if (i != FocusedViewportIndex && TryDropOnViewport(i))
        {
            return;
        }
    }
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
        ApplyPIEFixedAspectViewportRect();

        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedViewportIndex = Layout.GetLastFocusedViewportIndex();
        const bool bPIEActive = EditorEngine->GetEditorState() != EViewportPlayState::Editing;
        const int32 ActivePIEViewportIndex = EditorEngine->GetActivePIEViewportIndex();
        auto DrawSceneViewport = [&](int32 ViewportIndex)
        {
            if (bPIEActive && ActivePIEViewportIndex >= 0 && ViewportIndex != ActivePIEViewportIndex)
            {
                return;
            }

            auto& VP = Layout.GetSceneViewport(ViewportIndex);
            const FViewportRect ViewportRect = VP.GetRect();
            if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
            {
                return;
            }

            const ID3D11ShaderResourceView* SceneColorSRV = VP.GetOutSRV();

            ImVec2 Size = ImVec2(
                static_cast<float>(ViewportRect.Width),
                static_cast<float>(ViewportRect.Height));
            ImGui::SetCursorScreenPos(ImVec2(
                static_cast<float>(ViewportRect.X),
                static_cast<float>(ViewportRect.Y)));
            ImDrawList* DrawList = ImGui::GetWindowDrawList();

            if (SceneColorSRV)
            {
                ID3D11DeviceContext* DeviceContext = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

                DrawList->AddCallback(SetOpaqueBlendStateCallback, DeviceContext);
                ImGui::Image(reinterpret_cast<ImTextureID>(SceneColorSRV), Size);
                DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }
            else
            {
                ImGui::Dummy(Size);
            }

            if (bPIEActive && ViewportIndex == FocusedViewportIndex)
            {
                RenderRuntimeUIForPIEViewport(ViewportRect, ImGui::GetIO().DeltaTime);
            }

            FEditorViewportClient* DropClient = Layout.GetViewportClient(ViewportIndex);
            const FEditorViewportState& State = Layout.GetViewportState(ViewportIndex);
            const float PIEFlashAlpha = DropClient ? DropClient->GetPIEStartOutlineFlashAlpha() : 0.0f;
            const bool bFocused = ViewportIndex == FocusedViewportIndex;
            const bool bHovered = State.bHovered;
            if (bFocused || bHovered || PIEFlashAlpha > 0.0f)
            {
                const ImVec2 OutlineMin(static_cast<float>(ViewportRect.X), static_cast<float>(ViewportRect.Y));
                const ImVec2 OutlineMax(
                    static_cast<float>(ViewportRect.X + ViewportRect.Width),
                    static_cast<float>(ViewportRect.Y + ViewportRect.Height));
                const ImU32 OutlineColor = bFocused ? IM_COL32(82, 168, 255, 235) : IM_COL32(170, 190, 210, 120);
                DrawList->AddRect(OutlineMin, OutlineMax, OutlineColor, 0.0f, 0, bFocused ? 2.0f : 1.0f);
                if (PIEFlashAlpha > 0.0f)
                {
                    const int32 Alpha = static_cast<int32>(220.0f * PIEFlashAlpha);
                    DrawList->AddRect(OutlineMin, OutlineMax, IM_COL32(120, 255, 150, Alpha), 0.0f, 0, 4.0f);
                }
            }
        };

		for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            if (i != FocusedViewportIndex)
            {
                DrawSceneViewport(i);
            }
        }
        if (FocusedViewportIndex >= 0 && FocusedViewportIndex < FEditorViewportLayout::MaxViewports)
        {
            DrawSceneViewport(FocusedViewportIndex);
        }
        if (!bPIEActive)
        {
            ViewportOverlayWidget.RenderSplitterBar(ImGui::GetWindowDrawList());
        }

        // 뷰포트별 독립 메뉴바 오버레이
        {
            constexpr float MenuBarH = 34.0f;
            const bool bOnlyFocusedToolbar = Layout.IsLayoutTransitionActive();

            for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
            {
                const int32 DrawIndex = (i == FEditorViewportLayout::MaxViewports - 1)
                    ? FocusedViewportIndex
                    : (i < FocusedViewportIndex ? i : i + 1);
                if (DrawIndex < 0 || DrawIndex >= FEditorViewportLayout::MaxViewports)
                    continue;
                if (bPIEActive && ActivePIEViewportIndex >= 0 && DrawIndex != ActivePIEViewportIndex)
                    continue;
                if (bOnlyFocusedToolbar && DrawIndex != FocusedViewportIndex)
                    continue;

                if (FEditorViewportClient* Client = Layout.GetViewportClient(DrawIndex))
                {
                    const bool bHidePIEViewportToolbar =
                        EditorEngine->GetEditorState() != EViewportPlayState::Editing && Client->IsPIEPossessed();
                    Client->SetViewportInputDeadZoneTop(bHidePIEViewportToolbar ? 0.0f : MenuBarH);
                    if (bHidePIEViewportToolbar)
                    {
                        continue;
                    }
                }

                FViewportRect ViewportRect = Layout.GetSceneViewport(DrawIndex).GetRect();
                if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
                    continue;

                const float LocalX = static_cast<float>(ViewportRect.X - HostRect.X);
                const float LocalY = static_cast<float>(ViewportRect.Y - HostRect.Y);
                if (LocalX < 0.0f || LocalY < 0.0f)
                    continue;

                ImGui::SetCursorScreenPos(ImVec2(ContentPos.x + LocalX, ContentPos.y + LocalY));

                char ChildID[32];
                snprintf(ChildID, sizeof(ChildID), "##VPMenu%d", DrawIndex);

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
                    ImGui::PushID(DrawIndex);
                    RenderViewportIconToolbarForIndex(DrawIndex);
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

FViewportRect FEditorMainPanel::GetPIEFixedAspectViewportRect(const FViewportRect& SourceRect) const
{
    if (SourceRect.Width <= 0 || SourceRect.Height <= 0 || PIEUILayoutWidth <= 0 || PIEUILayoutHeight <= 0)
    {
        return SourceRect;
    }

    const float TargetAspect = static_cast<float>(PIEUILayoutWidth) / static_cast<float>(PIEUILayoutHeight);
    const float SourceAspect = static_cast<float>(SourceRect.Width) / static_cast<float>(SourceRect.Height);

    int32 Width = SourceRect.Width;
    int32 Height = SourceRect.Height;
    if (SourceAspect > TargetAspect)
    {
        Width = std::max(static_cast<int32>(static_cast<float>(SourceRect.Height) * TargetAspect), 1);
    }
    else
    {
        Height = std::max(static_cast<int32>(static_cast<float>(SourceRect.Width) / TargetAspect), 1);
    }

    const int32 X = SourceRect.X + (SourceRect.Width - Width) / 2;
    const int32 Y = SourceRect.Y + (SourceRect.Height - Height) / 2;
    return FViewportRect(X, Y, Width, Height);
}

void FEditorMainPanel::ApplyPIEFixedAspectViewportRect()
{
    if (!EditorEngine || !bPIEUseFixedUILayout || EditorEngine->GetEditorState() == EViewportPlayState::Editing)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 PIEViewportIndex = (EditorEngine->GetActivePIEViewportIndex() >= 0)
        ? EditorEngine->GetActivePIEViewportIndex()
        : Layout.GetLastFocusedViewportIndex();
    if (PIEViewportIndex < 0 || PIEViewportIndex >= FEditorViewportLayout::MaxViewports)
    {
        return;
    }

    FEditorViewportClient* Client = Layout.GetViewportClient(PIEViewportIndex);
    if (!Client || !Client->IsPIEPossessed())
    {
        return;
    }

    FSceneViewport& SceneViewport = Layout.GetSceneViewport(PIEViewportIndex);
    const FViewportRect SourceRect = SceneViewport.GetRect();
    const FViewportRect FixedRect = GetPIEFixedAspectViewportRect(SourceRect);
    if (FixedRect.X == SourceRect.X && FixedRect.Y == SourceRect.Y &&
        FixedRect.Width == SourceRect.Width && FixedRect.Height == SourceRect.Height)
    {
        return;
    }

    SceneViewport.SetRect(FixedRect);
    Client->SetViewportSize(static_cast<float>(FixedRect.Width), static_cast<float>(FixedRect.Height));
}

void FEditorMainPanel::RenderRuntimeUIForPIEViewport(const FViewportRect& ViewportRect, float DeltaTime)
{
    if (!EditorEngine || ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
    {
        return;
    }

    const int32 LayoutWidth = bPIEUseFixedUILayout ? std::max(PIEUILayoutWidth, 1) : ViewportRect.Width;
    const int32 LayoutHeight = bPIEUseFixedUILayout ? std::max(PIEUILayoutHeight, 1) : ViewportRect.Height;

    FRuntimeUIRenderContext Context;
    Context.RenderMode = ERuntimeUIRenderMode::PIE;
    Context.ViewportMin = FRuntimeUIVector2(static_cast<float>(ViewportRect.X), static_cast<float>(ViewportRect.Y));
    Context.ViewportSize = FRuntimeUIVector2(static_cast<float>(ViewportRect.Width), static_cast<float>(ViewportRect.Height));
    Context.LayoutSize = FRuntimeUIVector2(static_cast<float>(LayoutWidth), static_cast<float>(LayoutHeight));
    Context.DeltaTime = DeltaTime;

    PendingPIERmlUiRenderContexts.push_back(Context);

    if (EditorEngine->PumpPIERmlUiInput(ViewportRect, LayoutWidth, LayoutHeight))
    {
        InputSystem::Get().SetGuiMouseCapture(true);
        InputSystem::Get().SetGuiViewportMouseBlock(true);
    }
}

void FEditorMainPanel::TickViewportContextMenu()
{
    if (!EditorEngine || !Window || ImGui::IsPopupOpen("##ViewportContextMenu"))
    {
        return;
    }

    if (ContentBrowserWidget.IsVisible() && ContentBrowserWidget.IsMouseOverBrowser())
    {
        ViewportContextMenuState.bRightClickTracking = false;
        ViewportContextMenuState.TrackingViewportIndex = -1;
        ViewportContextMenuState.RightClickTravelSq = 0.0f;
        return;
    }

    InputSystem& IS = InputSystem::Get();
    const FGuiInputState& GuiState = IS.GetGuiInputState();
    if (GuiState.bBlockViewportMouse && !GuiState.bAllowViewportMouseFocus)
    {
        ViewportContextMenuState.bRightClickTracking = false;
        ViewportContextMenuState.TrackingViewportIndex = -1;
        ViewportContextMenuState.RightClickTravelSq = 0.0f;
        return;
    }

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
            if (FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex))
            {
                if (Client->IsPIEPossessed())
                {
                    ViewportContextMenuState.bRightClickTracking = false;
                    ViewportContextMenuState.TrackingViewportIndex = -1;
                    ViewportContextMenuState.RightClickTravelSq = 0.0f;
                    ViewportContextMenuState.PendingPopupViewportIndex = -1;
                    return;
                }
            }

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
    if (FEditorViewportClient* TrackingClient = Layout.GetViewportClient(ViewportContextMenuState.TrackingViewportIndex))
    {
        if (TrackingClient->IsPIEPossessed())
        {
            ViewportContextMenuState.bRightClickTracking = false;
            ViewportContextMenuState.TrackingViewportIndex = -1;
            ViewportContextMenuState.RightClickTravelSq = 0.0f;
            ViewportContextMenuState.PendingPopupViewportIndex = -1;
            return;
        }
    }

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
        if (FEditorViewportClient* PendingClient = Layout.GetViewportClient(ViewportContextMenuState.PendingPopupViewportIndex))
        {
            if (PendingClient->IsPIEPossessed())
            {
                ViewportContextMenuState.PendingPopupViewportIndex = -1;
                ViewportContextMenuState.PendingSpawnViewportIndex = -1;
                return;
            }
        }

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
    const bool bEditorControl = Client && Client->AllowsEditorWorldControl();
    const bool bPIEActive = Client && Client->IsPIEActive();
    const bool bHasSelection = !EditorEngine->GetSelectionManager().IsEmpty();

    ImGui::TextDisabled("%s", GetViewportSlotName(FocusedIndex));
    ImGui::Separator();

    if (ImGui::BeginMenu("Place Actor", bEditorControl && Client != nullptr))
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

        ControlWidget.DrawPlaceActorMenu(SpawnLocation, true);
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Delete", "Del", false, bEditorControl && Client != nullptr && bHasSelection))
    {
        Client->RequestDeleteSelection();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Focus Selection", "F", false, Client != nullptr && bHasSelection))
    {
        Client->FocusSelection();
    }

    if (!bEditorControl)
    {
        if (bPIEActive && ImGui::MenuItem("Stop PIE", "Esc"))
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
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, bHasSelection)) Client->RequestDuplicateSelection();
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

bool FEditorMainPanel::DrawViewportIconButton(const char* Id, EViewportToolIcon Icon, const char* FallbackLabel, const char* Tooltip, bool bSelected, bool bEnabled, bool bPairFirst, bool bPairSecond)
{
    if (!bEnabled)
    {
        ImGui::BeginDisabled();
    }

    if (bSelected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
    }

    ID3D11ShaderResourceView* SRV = ViewportToolIcons[static_cast<int32>(Icon)];
    bool bPressed = false;
    if (!SRV)
    {
        bPressed = DrawViewportTextButton(Id, FallbackLabel, bPairFirst, bPairSecond);
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

    const bool bEditorControl = Client->AllowsEditorWorldControl();
    const bool bPIEPossessed = Client->IsPIEPossessed();

    ImGui::PushID(ViewportIndex);
    constexpr float ToolbarLeftPadding = 8.0f;
    const float CenteredToolbarY = std::max(0.0f, (ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::SetCursorPos(ImVec2(ToolbarLeftPadding, CenteredToolbarY));

    if (bPIEPossessed)
    {
        ImGui::PopID();
        return;
    }

    if (DrawViewportIconButton("##SelectMode", EViewportToolIcon::Select, "Q", "Select (Q / 1)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Select, bEditorControl))
    {
        Client->RequestSetSelectMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##TranslateMode", EViewportToolIcon::Translate, "W", "Translate (W / 2)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Translate, bEditorControl))
    {
        Client->RequestSetTranslateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##RotateMode", EViewportToolIcon::Rotate, "E", "Rotate (E / 3)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Rotate, bEditorControl))
    {
        Client->RequestSetRotateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton("##ScaleMode", EViewportToolIcon::Scale, "R", "Scale (R / 4)", Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Scale, bEditorControl))
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
    if (DrawViewportIconButton("##SpaceMode", bWorldSpace ? EViewportToolIcon::WorldSpace : EViewportToolIcon::LocalSpace, bWorldSpace ? "W" : "L", bWorldSpace ? "World Space (X)" : "Local Space (X)", bWorldSpace, bEditorControl))
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
        const bool bTogglePressed = DrawViewportIconButton(ToggleID, SnapIcon, Prefix, Prefix, false, bEditorControl, true, false);
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

    const ImVec2 WindowScreenPos = ImGui::GetWindowPos();
    const float LeftGroupEndX = ImGui::GetItemRectMax().x - WindowScreenPos.x;

    char TypePopupID[48];
    snprintf(TypePopupID, sizeof(TypePopupID), "##ViewportTypePopup_%d", ViewportIndex);
    char TypeButtonLabel[64];
    snprintf(TypeButtonLabel, sizeof(TypeButtonLabel), "%s ▼", GetViewportTypeName(Client->GetViewportType()));
    char CameraPopupID[48];
    snprintf(CameraPopupID, sizeof(CameraPopupID), "##ViewportCameraSpeedPopup_%d", ViewportIndex);
    char CameraButtonLabel[48];
    snprintf(CameraButtonLabel, sizeof(CameraButtonLabel), "Cam %.1fx ▼", GetCameraSpeedMultiplier(Client));
    char ViewPopupID[48];
    snprintf(ViewPopupID, sizeof(ViewPopupID), "##ViewportViewPopup_%d", ViewportIndex);
    char ViewButtonLabel[80];
    snprintf(ViewButtonLabel, sizeof(ViewButtonLabel), "%s ▼", GetViewModeName(Client->GetViewportState()->ViewMode));

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
    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        AddRightItemWidth(CalcTextButtonWidth(CameraButtonLabel));
    }
    AddRightItemWidth(CalcTextButtonWidth(ViewButtonLabel));
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);

    const float ContentRightX = ImGui::GetWindowContentRegionMax().x;
    const float RightStartX = ContentRightX - RightGroupWidth;
    const float MinRightGroupStartX = LeftGroupEndX + ImGui::GetStyle().ItemSpacing.x + 12.0f;
    const bool bUseOverflowMenu = RightStartX <= MinRightGroupStartX;
    auto SetToolbarItemScreenPos = [&](float LocalX)
    {
        ImGui::SetCursorScreenPos(ImVec2(WindowScreenPos.x + std::max(0.0f, LocalX), WindowScreenPos.y + CenteredToolbarY));
    };

    if (bUseOverflowMenu)
    {
        const float OverflowStartX = ContentRightX - IconButtonWidth;
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

            if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
            {
                float SpeedMultiplier = GetCameraSpeedMultiplier(Client);
                if (ImGui::SliderFloat("Camera Speed", &SpeedMultiplier, 0.01f, 500.0f, "%.2fx"))
                {
                    SetCameraSpeedMultiplier(Client, SpeedMultiplier);
                }
                ImGui::Separator();
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

    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        ImGui::SameLine();
        if (DrawViewportTextButton("##ViewportCameraSpeedButton", CameraButtonLabel))
        {
            ImGui::OpenPopup(CameraPopupID);
        }
        if (ImGui::BeginPopup(CameraPopupID))
        {
            float SpeedMultiplier = GetCameraSpeedMultiplier(Client);
            if (ImGui::SliderFloat("Speed", &SpeedMultiplier, 0.01f, 500.0f, "%.2fx"))
            {
                SetCameraSpeedMultiplier(Client, SpeedMultiplier);
            }
            ImGui::EndPopup();
        }
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
    if (DrawViewportIconButton("##ViewportSettings", EViewportToolIcon::Setting, "S", "Viewport Settings", ViewportOverlayWidget.IsViewportSettingsVisible(), true))
    {
        ViewportOverlayWidget.SetViewportSettingsVisible(!ViewportOverlayWidget.IsViewportSettingsVisible());
    }

    ImGui::SameLine();
    if (DrawViewportIconButton("##LayoutIconMenu", EViewportToolIcon::Menu, "L", "Layout Presets", false, true, true, false))
    {
        ImGui::OpenPopup("##LayoutIconPopup");
    }

    if (ImGui::BeginPopup("##LayoutIconPopup"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
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
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
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
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 0.0f);
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
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersRight);
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(ToggleIcon),
            ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
            ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
    }
    else
    {
        bTogglePressed = DrawViewportTextButton("##SplitMergeViewportText", CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split" : "Merge", false, true);
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
