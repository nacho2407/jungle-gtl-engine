#include "EditorViewerWindowWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorChromeConstants.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Viewport/ViewportLayout.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "Core/Paths.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/ResourceManager.h"
#include "Component/GizmoComponent.h"
#include "Component/TransformProxy.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "Object/Class.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cmath>
#include <functional>

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

bool UsesAbsoluteImGuiCoordinates()
{
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

POINT ImGuiScreenToClientPoint(FWindowsWindow* Window, const ImVec2& Point)
{
    POINT Result =
    {
        static_cast<LONG>(std::lround(Point.x)),
        static_cast<LONG>(std::lround(Point.y))
    };
    if (Window && Window->GetHWND() && UsesAbsoluteImGuiCoordinates())
    {
        ::ScreenToClient(Window->GetHWND(), &Result);
    }
    return Result;
}

FString GetBaseFileNameWithoutExtension(const FString& Path)
{
    if (Path.empty())
    {
        return "Viewer";
    }

    const size_t SlashPos = Path.find_last_of("/\\");
    const size_t NameBegin = SlashPos == FString::npos ? 0 : SlashPos + 1;
    FString Name = Path.substr(NameBegin);

    const size_t DotPos = Name.find_last_of('.');
    if (DotPos != FString::npos && DotPos > 0)
    {
        Name = Name.substr(0, DotPos);
    }

    return Name.empty() ? "Viewer" : Name;
}

FString GetViewerAssetLabel(FEditorViewer* Viewer)
{
    return Viewer ? GetBaseFileNameWithoutExtension(Viewer->GetFileName()) : FString("Viewer");
}

void GetSelectableAnimNotifyClasses(TArray<UClass*>& OutClasses)
{
    OutClasses.clear();
    FReflectionRegistry::Get().GetClassesDerivedFrom(UAnimNotify::StaticClass(), OutClasses);
    OutClasses.erase(
        std::remove_if(
            OutClasses.begin(),
            OutClasses.end(),
            [](UClass* Class)
            {
                return !Class ||
                    Class == UAnimNotify::StaticClass() ||
                    Class == UAnimNotifyState::StaticClass() ||
                    Class->HasAnyClassFlags(CF_Abstract);
            }),
        OutClasses.end());

    std::sort(
        OutClasses.begin(),
        OutClasses.end(),
        [](const UClass* A, const UClass* B)
        {
            return FString(A->GetDisplayName()) < FString(B->GetDisplayName());
        });
}

FString GetAnimNotifyClassDisplayName(const FString& ClassName)
{
    if (UClass* Class = FReflectionRegistry::Get().FindClass(ClassName))
    {
        return Class->GetDisplayName();
    }
    return ClassName.empty() ? FString("None") : ClassName;
}

FString GetDefaultEditorNotifyClassName(float Duration)
{
    TArray<UClass*> NotifyClasses;
    GetSelectableAnimNotifyClasses(NotifyClasses);

    UClass* DesiredBaseClass = Duration > 0.0f ? UAnimNotifyState::StaticClass() : UAnimNotify::StaticClass();
    for (UClass* Class : NotifyClasses)
    {
        if (Class && Class->IsChildOf(DesiredBaseClass))
        {
            return Class->GetName();
        }
    }

    return NotifyClasses.empty() ? FString() : NotifyClasses.front()->GetName();
}

bool DrawAnimNotifyClassCombo(const char* Label, FString& InOutClassName)
{
    TArray<UClass*> NotifyClasses;
    GetSelectableAnimNotifyClasses(NotifyClasses);
    if (NotifyClasses.empty())
    {
        ImGui::TextDisabled("No AnimNotify classes are registered.");
        return false;
    }

    if (InOutClassName.empty())
    {
        InOutClassName = NotifyClasses.front()->GetName();
    }

    bool bChanged = false;
    const FString Preview = GetAnimNotifyClassDisplayName(InOutClassName);
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo(Label, Preview.c_str()))
    {
        for (UClass* Class : NotifyClasses)
        {
            const FString ClassName = Class->GetName();
            const bool bSelected = ClassName == InOutClassName;
            const FString ItemLabel = FString(Class->GetDisplayName()) + "##" + ClassName;
            if (ImGui::Selectable(ItemLabel.c_str(), bSelected))
            {
                InOutClassName = ClassName;
                bChanged = true;
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return bChanged;
}

void ApplyDetachedDocumentWindowClass()
{
    ImGuiWindowClass WindowClass;
    WindowClass.ClassId = 0x4A534457u; // "JSDW" - detached document window class
    WindowClass.ViewportFlagsOverrideSet =
        ImGuiViewportFlags_NoAutoMerge |
        ImGuiViewportFlags_NoDecoration;
    WindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoTaskBarIcon;
    ImGui::SetNextWindowClass(&WindowClass);
}

HWND GetCurrentViewportHwnd()
{
    ImGuiViewport* Viewport = ImGui::GetWindowViewport();
    if (!Viewport)
    {
        return nullptr;
    }
    return static_cast<HWND>(Viewport->PlatformHandleRaw ? Viewport->PlatformHandleRaw : Viewport->PlatformHandle);
}

ImGui_ImplWin32_CustomChromeRect MakeChromeRect(const ImVec2& Min, const ImVec2& Max, const ImVec2& WindowPos)
{
    return ImGui_ImplWin32_CustomChromeRect{
        static_cast<int>(Min.x - WindowPos.x),
        static_cast<int>(Min.y - WindowPos.y),
        static_cast<int>(Max.x - WindowPos.x),
        static_cast<int>(Max.y - WindowPos.y)
    };
}

void AddChromeRect(ImGui_ImplWin32_CustomChromeRect* Rects, int& Count, const ImVec2& Min, const ImVec2& Max, const ImVec2& WindowPos)
{
    if (Count >= 16)
    {
        return;
    }
    Rects[Count++] = MakeChromeRect(Min, Max, WindowPos);
}

bool IsViewportMaximized(HWND Hwnd)
{
    return Hwnd && ::IsZoomed(Hwnd) != FALSE;
}

void ToggleViewportMaximize(HWND Hwnd)
{
    if (!Hwnd)
    {
        return;
    }
    ::PostMessageW(Hwnd, WM_SYSCOMMAND, IsViewportMaximized(Hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
}

bool DrawDetachedWindowButton(
    const char* Id,
    const char* Tooltip,
    const ImVec2& Size,
    const ImVec4& HoverColor,
    const ImVec4& ActiveColor,
    const std::function<void(ImDrawList*, const ImVec2&, const ImVec2&, ImU32)>& DrawIcon)
{
    ImGui::PushID(Id);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveColor);

    const bool bClicked = ImGui::InvisibleButton("##Button", Size);
    const bool bHovered = ImGui::IsItemHovered();
    const bool bActive = ImGui::IsItemActive();
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const ImU32 BgColor = ImGui::GetColorU32(
        bActive ? ActiveColor : (bHovered ? HoverColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f)));

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddRectFilled(Min, Max, BgColor, 0.0f);
    DrawIcon(DrawList, Min, Max, ImGui::GetColorU32(ImVec4(0.82f, 0.85f, 0.90f, 1.0f)));

    if (bHovered && Tooltip)
    {
        ImGui::SetTooltip("%s", Tooltip);
    }

    ImGui::PopStyleColor(3);
    ImGui::PopID();
    return bClicked;
}

constexpr uint64 MeshEditHashOffset = 14695981039346656037ull;
constexpr uint64 MeshEditHashPrime = 1099511628211ull;

uint64 HashBytes(uint64 Seed, const void* Data, size_t Size)
{
    const unsigned char* Bytes = static_cast<const unsigned char*>(Data);
    for (size_t Index = 0; Index < Size; ++Index)
    {
        Seed ^= static_cast<uint64>(Bytes[Index]);
        Seed *= MeshEditHashPrime;
    }
    return Seed;
}

template <typename T>
uint64 HashValue(uint64 Seed, const T& Value)
{
    return HashBytes(Seed, &Value, sizeof(T));
}

uint64 HashString(uint64 Seed, const FString& Value)
{
    const uint64 Length = static_cast<uint64>(Value.size());
    Seed = HashValue(Seed, Length);
    return Value.empty() ? Seed : HashBytes(Seed, Value.data(), Value.size());
}

uint64 HashMatrix(uint64 Seed, const FMatrix& Matrix)
{
    return HashBytes(Seed, Matrix.M, sizeof(Matrix.M));
}
}

void FEditorViewerWindowWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorViewerWindowWidget::Shutdown()
{
    Children.clear();
    BoneToSocketIndices.clear();
    CachedMesh = nullptr;
    CachedSkComp = nullptr; 

    PendingPreviewPickerSocketIdx = -1; 
    RenameSocketIdx = -1;               
    bMeshDirty = false; 
    CleanMeshEditSignature = 0;
    bHasCleanMeshEditSignature = false;
    PreviewMeshPathBufferSource.clear();
    PreviewMeshPathBuffer[0] = '\0';
    SelectedAnimTrackIndex = -1;
    PendingAnimNotifyTimeToAdd = 0.0f;
    CachedAnimSequence = nullptr;

    Viewer = nullptr;
    bOpen = false;
}

FString FEditorViewerWindowWidget::GetWindowName() const
{
    char WindowName[64];
    sprintf_s(WindowName, "###ViewerWindow_%p", Viewer);
    return GetViewerAssetLabel(Viewer) + WindowName;
}

bool FEditorViewerWindowWidget::CanSaveMesh() const
{
	if (!Viewer || Viewer->IsAnimationSequenceViewer())
	{
		return false;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	return SkelComp && SkelComp->GetSkeletalMesh() && HasMeshAssetEdits();
}

bool FEditorViewerWindowWidget::IsMeshDirty() const
{
	return Viewer && !Viewer->IsAnimationSequenceViewer() && HasMeshAssetEdits();
}

void FEditorViewerWindowWidget::RequestSaveMesh()
{
	if (!Viewer || Viewer->IsAnimationSequenceViewer())
	{
		return;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (FResourceManager::Get().SaveSkeletalMesh(Mesh))
	{
		ResetMeshDirtyBaseline();
	}
}

FSkeletalMesh* FEditorViewerWindowWidget::ResolveCurrentMeshData() const
{
	if (CachedMesh)
	{
		return CachedMesh;
	}

	if (!Viewer)
	{
		return nullptr;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
	return Mesh ? Mesh->GetMeshData() : nullptr;
}

uint64 FEditorViewerWindowWidget::ComputeEditableMeshSignature(const FSkeletalMesh* MeshData) const
{
	if (!MeshData)
	{
		return 0;
	}

	uint64 Hash = MeshEditHashOffset;

	Hash = HashValue(Hash, static_cast<uint64>(MeshData->Bones.size()));
	for (const FBoneInfo& Bone : MeshData->Bones)
	{
		Hash = HashString(Hash, Bone.Name);
		Hash = HashValue(Hash, Bone.ParentIndex);
		Hash = HashMatrix(Hash, Bone.LocalBindTransform);
		Hash = HashMatrix(Hash, Bone.GlobalBindTransform);
		Hash = HashMatrix(Hash, Bone.InverseBindPose);
	}

	Hash = HashValue(Hash, static_cast<uint64>(MeshData->Sockets.size()));
	for (const FSkeletalMeshSocket& Socket : MeshData->Sockets)
	{
		Hash = HashString(Hash, Socket.Name.ToString());
		Hash = HashValue(Hash, Socket.BoneIndex);
		Hash = HashValue(Hash, Socket.RelativeLocation.X);
		Hash = HashValue(Hash, Socket.RelativeLocation.Y);
		Hash = HashValue(Hash, Socket.RelativeLocation.Z);
		Hash = HashValue(Hash, Socket.RelativeRotation.Pitch);
		Hash = HashValue(Hash, Socket.RelativeRotation.Yaw);
		Hash = HashValue(Hash, Socket.RelativeRotation.Roll);
		Hash = HashValue(Hash, Socket.RelativeScale.X);
		Hash = HashValue(Hash, Socket.RelativeScale.Y);
		Hash = HashValue(Hash, Socket.RelativeScale.Z);
	}

	return Hash;
}

void FEditorViewerWindowWidget::ResetMeshDirtyBaseline()
{
	FSkeletalMesh* MeshData = ResolveCurrentMeshData();
	if (!MeshData)
	{
		CleanMeshEditSignature = 0;
		bHasCleanMeshEditSignature = false;
		bMeshDirty = false;
		return;
	}

	CleanMeshEditSignature = ComputeEditableMeshSignature(MeshData);
	bHasCleanMeshEditSignature = true;
	bMeshDirty = false;
}

bool FEditorViewerWindowWidget::HasMeshAssetEdits() const
{
	FSkeletalMesh* MeshData = ResolveCurrentMeshData();
	if (!MeshData)
	{
		return false;
	}

	if (bMeshDirty)
	{
		return true;
	}

	return bHasCleanMeshEditSignature && ComputeEditableMeshSignature(MeshData) != CleanMeshEditSignature;
}

void FEditorViewerWindowWidget::Render(float DeltaTime)
{
    if (!bOpen)
        return;

    if (!EditorEngine)
        return;

	if (!Viewer)
        return;

    const float TitleBarFramePaddingY = std::max(
        0.0f,
        (FEditorChromeMetrics::ApplicationTitleBarHeight - ImGui::GetFontSize()) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(13.0f, TitleBarFramePaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.060f, 0.072f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.055f, 0.060f, 0.072f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.20f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.17f, 0.22f, 1.0f));

	FString WindowName = GetWindowName();
	bool bDockRequested = false;
	bool bCloseRequested = false;

    // Detached document는 borderless secondary viewport로 띄우고,
    // Win32 backend에 titlebar hit-test 정보를 넘겨 native window처럼 움직이게 한다.
    ApplyDetachedDocumentWindowClass();
	// Make the viewer window reasonably large on first creation.
	ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (const ImGuiViewport* MainViewport = ImGui::GetMainViewport())
    {
        ImGui::SetNextWindowPos(
            ImVec2(MainViewport->Pos.x + 120.0f, MainViewport->Pos.y + 90.0f),
            ImGuiCond_FirstUseEver);
    }
	constexpr ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(WindowName.c_str(), &bOpen, WindowFlags))
	{
        RenderDetachedDocumentChrome(bDockRequested, bCloseRequested);
        RenderDetachedDocumentToolbar(bDockRequested);
		RenderContent(DeltaTime);
	}
	ImGui::End();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(5);

	if (bDockRequested)
	{
		EditorEngine->GetMainPanel().RequestDockViewer(Viewer);
		return;
	}
	if (bCloseRequested)
	{
		bOpen = false;
	}

	if (!bOpen)
    {
        EditorEngine->RemoveViewer(Viewer);
        Shutdown();
    }
}

void FEditorViewerWindowWidget::RenderDetachedDocumentChrome(bool& bDockRequested, bool& bCloseRequested)
{
    if (!Viewer || !ImGui::BeginMenuBar())
    {
        return;
    }

    constexpr float WindowButtonWidth = 48.0f;
    constexpr float TitleBarHeight = FEditorChromeMetrics::ApplicationTitleBarHeight;
    constexpr float MenuStartX = 0.0f;

    HWND ViewportHwnd = GetCurrentViewportHwnd();
    const ImVec2 WindowPos = ImGui::GetWindowPos();
    const ImVec2 WindowSize = ImGui::GetWindowSize();
    const float ButtonStartX = std::max(0.0f, WindowSize.x - WindowButtonWidth * 3.0f);

    ImGui_ImplWin32_CustomChromeRect ChromeRects[16] = {};
    int ChromeRectCount = 0;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    const float TitleBarFramePaddingY = std::max(
        0.0f,
        (TitleBarHeight - ImGui::GetFontSize()) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18.0f, TitleBarFramePaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 8.0f));

    ImGui::SetCursorPos(ImVec2(MenuStartX, 0.0f));

    const bool bCanSaveMesh = CanSaveMesh();
    const char* SaveMeshLabel = IsMeshDirty() ? "Save Mesh *" : "Save Mesh";

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem(SaveMeshLabel, "Ctrl+S", false, bCanSaveMesh))
        {
            RequestSaveMesh();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close"))
        {
            bCloseRequested = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Asset"))
    {
        if (ImGui::MenuItem(SaveMeshLabel, nullptr, false, bCanSaveMesh))
        {
            RequestSaveMesh();
        }
        ImGui::MenuItem("Reimport Mesh", nullptr, false, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::MenuItem("Dock Back"))
        {
            bDockRequested = true;
        }
        if (ImGui::MenuItem("Close"))
        {
            bCloseRequested = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools"))
    {
        FSkeletalViewerShowFlags& ShowFlags = Viewer->GetClient().GetShowFlags();
        ImGui::MenuItem("Bones", nullptr, &ShowFlags.bShowBones);
        ImGui::MenuItem("Bounding Box", nullptr, &ShowFlags.bShowBoundingBox);
        ImGui::MenuItem("Outline", nullptr, &ShowFlags.bShowOutline);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help"))
    {
        ImGui::TextDisabled(Viewer->IsAnimationSequenceViewer() ? "Animation Sequence Viewer" : "Skeletal Mesh Previewer");
        ImGui::EndMenu();
    }

    const float MenuEndX = std::min(ButtonStartX, ImGui::GetCursorScreenPos().x - WindowPos.x + 8.0f);
    AddChromeRect(
        ChromeRects,
        ChromeRectCount,
        ImVec2(WindowPos.x, WindowPos.y),
        ImVec2(WindowPos.x + MenuEndX, WindowPos.y + TitleBarHeight),
        WindowPos);

    const FString AssetLabel = GetViewerAssetLabel(Viewer);
    const ImVec2 TitleSize = ImGui::CalcTextSize(AssetLabel.c_str());
    const float TitleX = std::clamp(
        MenuEndX + (ButtonStartX - MenuEndX - TitleSize.x) * 0.5f,
        MenuEndX + 8.0f,
        std::max(MenuEndX + 8.0f, ButtonStartX - TitleSize.x - 8.0f));
    DrawList->AddText(
        ImVec2(WindowPos.x + TitleX, WindowPos.y + (TitleBarHeight - TitleSize.y) * 0.5f),
        ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
        AssetLabel.c_str());

    const ImVec2 ButtonSize(WindowButtonWidth, TitleBarHeight);
    ImGui::SetCursorPos(ImVec2(ButtonStartX, 0.0f));
    if (DrawDetachedWindowButton(
        "DetachedMinimize",
        "Minimize",
        ButtonSize,
        ImVec4(0.14f, 0.16f, 0.20f, 1.0f),
        ImVec4(0.18f, 0.20f, 0.25f, 1.0f),
        [](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
        {
            const float Y = (Min.y + Max.y) * 0.5f + 4.0f;
            InDrawList->AddLine(ImVec2(Min.x + 17.0f, Y), ImVec2(Max.x - 17.0f, Y), Color, 1.6f);
        }))
    {
        if (ViewportHwnd)
        {
            ::PostMessageW(ViewportHwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        }
    }
    AddChromeRect(ChromeRects, ChromeRectCount, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), WindowPos);

    ImGui::SameLine(0.0f, 0.0f);
    if (DrawDetachedWindowButton(
        "DetachedMaximize",
        IsViewportMaximized(ViewportHwnd) ? "Restore" : "Maximize",
        ButtonSize,
        ImVec4(0.14f, 0.16f, 0.20f, 1.0f),
        ImVec4(0.18f, 0.20f, 0.25f, 1.0f),
        [ViewportHwnd](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
        {
            const bool bMaximized = IsViewportMaximized(ViewportHwnd);
            const ImVec2 A(Min.x + 17.0f, Min.y + 12.0f);
            const ImVec2 B(Max.x - 17.0f, Max.y - 12.0f);
            if (bMaximized)
            {
                InDrawList->AddRect(ImVec2(A.x + 3.0f, A.y), ImVec2(B.x + 3.0f, B.y - 3.0f), Color, 0.0f, 0, 1.4f);
                InDrawList->AddRect(ImVec2(A.x, A.y + 3.0f), ImVec2(B.x, B.y), Color, 0.0f, 0, 1.4f);
            }
            else
            {
                InDrawList->AddRect(A, B, Color, 0.0f, 0, 1.4f);
            }
        }))
    {
        ToggleViewportMaximize(ViewportHwnd);
    }
    AddChromeRect(ChromeRects, ChromeRectCount, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), WindowPos);

    ImGui::SameLine(0.0f, 0.0f);
    if (DrawDetachedWindowButton(
        "DetachedClose",
        "Close",
        ButtonSize,
        ImVec4(0.62f, 0.18f, 0.20f, 1.0f),
        ImVec4(0.46f, 0.10f, 0.13f, 1.0f),
        [](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
        {
            InDrawList->AddLine(ImVec2(Min.x + 17.0f, Min.y + 12.0f), ImVec2(Max.x - 17.0f, Max.y - 12.0f), Color, 1.6f);
            InDrawList->AddLine(ImVec2(Max.x - 17.0f, Min.y + 12.0f), ImVec2(Min.x + 17.0f, Max.y - 12.0f), Color, 1.6f);
        }))
    {
        bCloseRequested = true;
    }
    AddChromeRect(ChromeRects, ChromeRectCount, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), WindowPos);

    ImGui_ImplWin32_SetCustomChrome(ViewportHwnd, static_cast<int>(TitleBarHeight), ChromeRects, ChromeRectCount);
    ImGui::PopStyleVar(3);
    ImGui::EndMenuBar();
}

void FEditorViewerWindowWidget::RenderDetachedDocumentToolbar(bool& bDockRequested)
{
    if (!Viewer || !EditorEngine)
    {
        return;
    }

    constexpr ImGuiWindowFlags ToolbarFlags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("##DetachedViewerToolbar", ImVec2(0.0f, 40.0f), false, ToolbarFlags);
    ImGui::SetCursorPos(ImVec2(8.0f, 6.0f));

    const bool bCanSaveMesh = CanSaveMesh();
    if (!bCanSaveMesh)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(IsMeshDirty() ? "Save *" : "Save"))
    {
        RequestSaveMesh();
    }
    if (!bCanSaveMesh)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Dock"))
    {
        bDockRequested = true;
    }
    ImGui::SameLine(0.0f, 12.0f);
    EditorEngine->GetMainPanel().RenderViewerToolbarControls(Viewer);
    ImGui::EndChild();
}

void FEditorViewerWindowWidget::RenderEmbedded(float DeltaTime)
{
	if (!bOpen || !EditorEngine || !Viewer)
	{
		return;
	}

	RenderContent(DeltaTime);
}

void FEditorViewerWindowWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	FSceneViewport& SceneViewport = Viewer->GetViewport();
	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();

    if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
        return;
	}

	ImVec2 FullSize = ImGui::GetContentRegionAvail();
	const float CenterWidth = std::max(160.0f, FullSize.x - LeftPanelWidth - RightPanelWidth - (ImGui::GetStyle().ItemSpacing.x * 2.0f));

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelMeshComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* SkeletalMesh = SkelMeshComp ? SkelMeshComp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* MeshData = SkeletalMesh ? SkeletalMesh->GetMeshData() : nullptr;
	CachedSkComp = SkelMeshComp;

	if (Viewer->IsAnimationSequenceViewer())
	{
		CachedMesh = MeshData;
		UAnimSequence* Sequence = Viewer->GetAnimSequence();
		if (CachedAnimSequence != Sequence)
		{
			CachedAnimSequence = Sequence;
			SelectedAnimTrackIndex = -1;
			SelectedAnimNotifyIndex = -1;
			DraggingAnimNotifyIndex = -1;
			SelectedAnimNotifyNameBufferIndex = -1;
			SelectedAnimNotifyNameBuffer[0] = '\0';
			bAnimNotifyDragDirty = false;
		}


		const float TimelineHeight = 350.0f;
		ImVec2 WorkSize = ImGui::GetContentRegionAvail();
		const float ViewAreaHeight = std::max(180.0f, WorkSize.y - TimelineHeight - ImGui::GetStyle().ItemSpacing.y);
		const float DetailsWidth = std::clamp(RightPanelWidth, 350.0f, std::max(350.0f, WorkSize.x * 0.4f));
		const float ViewWidth = std::max(220.0f, WorkSize.x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x);

		RenderViewportPanel(SceneViewport, SRV, ImVec2(ViewWidth, ViewAreaHeight));

		ImGui::SameLine();
		ImGui::BeginChild("AnimSequenceDetailsPanel", ImVec2(DetailsWidth, ViewAreaHeight), true);
		RenderAnimSequenceDetails(Sequence, SkeletalMesh);
		ImGui::EndChild();

		const float TimelineListWidth = 350.0f;
        const float SpacingX = ImGui::GetStyle().ItemSpacing.x;

        const float TimelineWidth = std::max(
            260.0f,
            WorkSize.x - TimelineListWidth - SpacingX);

        ImGui::BeginChild("AnimSequenceTimelinePanel", ImVec2(TimelineWidth, 0.0f), true);
        RenderAnimSequenceToolbar(Sequence);
        RenderAnimSequenceTimeline(Sequence);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("AnimSequenceListPanel", ImVec2(TimelineListWidth, 0.0f), true);
        RenderAnimSequenceList(Sequence);
        ImGui::EndChild();

        return;
	}

	if (CachedAnimSequence)
	{
		CachedAnimSequence = nullptr;
		DraggingAnimNotifyIndex = -1;
		SelectedAnimNotifyNameBufferIndex = -1;
		SelectedAnimNotifyNameBuffer[0] = '\0';
		bAnimNotifyDragDirty = false;
	}

	RenderSkeletonLeftPanel(SkelMeshComp, MeshData);

	ImGui::SameLine();
	ImGui::Button("##left_splitter", ImVec2(2.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		LeftPanelWidth += ImGui::GetIO().MouseDelta.x;
		LeftPanelWidth = std::clamp(LeftPanelWidth, 100.0f, FullSize.x * 0.4f);
	}
	ImGui::SameLine();

	RenderViewportPanel(SceneViewport, SRV, ImVec2(CenterWidth, 0.0f));

	ImGui::SameLine();
	ImGui::Button("##right_splitter", ImVec2(2.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		RightPanelWidth -= ImGui::GetIO().MouseDelta.x;
		RightPanelWidth = std::clamp(RightPanelWidth, 100.0f, FullSize.x * 0.4f);
	}
	ImGui::SameLine();

	RenderBoneRightPanel(SkelMeshComp);
}

void FEditorViewerWindowWidget::RenderViewportPanel(FSceneViewport& SceneViewport, ID3D11ShaderResourceView* SRV, const ImVec2& Size)
{
	ImGui::BeginChild("ViewportPanel", Size, false);

	ImVec2 ViewSize = ImGui::GetContentRegionAvail();
	ViewSize.x = std::max(ViewSize.x, 1.0f);
	ViewSize.y = std::max(ViewSize.y, 1.0f);

	ImGui::Dummy(ViewSize);
	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	const POINT ClientMin = ImGuiScreenToClientPoint(EditorEngine ? EditorEngine->GetWindow() : nullptr, Min);
	const bool bViewportHovered = ImGui::IsItemHovered();
	const bool bViewportClicked =
		bViewportHovered &&
		(ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		 ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
		 ImGui::IsMouseClicked(ImGuiMouseButton_Middle));

	FViewportRect NewRect;
	NewRect.X = (int32)ClientMin.x;
	NewRect.Y = (int32)ClientMin.y;
	NewRect.Width = (int32)(Max.x - Min.x);
	NewRect.Height = (int32)(Max.y - Min.y);

	SceneViewport.SetRect(NewRect);

	if (auto* Client = SceneViewport.GetClient())
	{
		Client->SetViewportSize((float)NewRect.Width, (float)NewRect.Height);
	}
	if (bViewportClicked)
	{
		EditorEngine->FocusViewportInput(&SceneViewport);
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ID3D11DeviceContext* DC = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
	DrawList->AddCallback(SetOpaqueBlendStateCallback, DC);
	DrawList->AddImage((ImTextureID)SRV, Min, Max);
	DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

	ImGui::EndChild();
}

void FEditorViewerWindowWidget::RenderSkeletonLeftPanel(USkeletalMeshComponent* SkelMeshComp, FSkeletalMesh* MeshData)
{
	ImGui::BeginChild("SkeletonPanel", ImVec2(LeftPanelWidth, 0), true);
	ImGui::Text("Skeleton");

	if (!MeshData)
	{
		CachedMesh = nullptr;
		Children.clear();
		BoneToSocketIndices.clear();
		if (Viewer)
		{
			Viewer->ClearSelection();
		}
		ResetMeshDirtyBaseline();
		ImGui::TextDisabled("No skeletal mesh");
	}
	else if (CachedMesh != MeshData)
	{
		CachedMesh = MeshData;
		if (Viewer)
		{
			Viewer->ClearSelection();
		}

		RebuildBoneTreeCaches(MeshData);
		ResetMeshDirtyBaseline();
	}

	if (MeshData)
	{
		ApplyPendingBoneTreeOpenState(MeshData);
		for (int32 j = 0; j < MeshData->Bones.size(); ++j)
		{
			if (MeshData->Bones[j].ParentIndex == -1)
			{
				DrawBoneNode(j, MeshData->Bones, Children);
			}
		}
	}

	if (PendingPreviewPickerSocketIdx >= 0 && !ImGui::IsPopupOpen("PickStaticMesh"))
	{
		ImGui::OpenPopup("PickStaticMesh");
	}
	DrawPreviewPickerModal();

	if (RenameSocketIdx >= 0 && !ImGui::IsPopupOpen("RenameSocket"))
	{
		ImGui::OpenPopup("RenameSocket");
	}
	DrawRenameModal();

	ImGui::Separator();
	DrawSocketInspector();

	ImGui::EndChild();
}

void FEditorViewerWindowWidget::RenderBoneRightPanel(USkeletalMeshComponent* SkelMeshComp)
{
	ImGui::BeginChild("BoneDetailsPanel", ImVec2(RightPanelWidth, 0), true);
	ImGui::Text("Details");
	ImGui::Separator();
	if (Viewer->GetSelectedBoneIndex() != -1 && SkelMeshComp)
	{
		RenderBoneDetails(SkelMeshComp);
	}
	else if (Viewer->GetSelectedSocketIndex() != -1 && SkelMeshComp)
	{
		if (CachedMesh && Viewer->GetSelectedSocketIndex() < (int32)CachedMesh->Sockets.size())
		{
			ImGui::Text("Socket: %s", CachedMesh->Sockets[Viewer->GetSelectedSocketIndex()].Name.ToString().c_str());
			ImGui::Separator();
			ImGui::Text("Selected Socket for transformation.");
		}
	}
	else
	{
		ImGui::TextDisabled("No bone or socket selected.");
	}
	ImGui::EndChild();
}

void FEditorViewerWindowWidget::SyncPreviewMeshPathBuffer()
{
	if (!Viewer)
	{
		PreviewMeshPathBufferSource.clear();
		PreviewMeshPathBuffer[0] = '\0';
		return;
	}

	const FString& Path = Viewer->GetPreviewMeshPath();
	if (PreviewMeshPathBufferSource == Path)
	{
		return;
	}

	PreviewMeshPathBufferSource = Path;
	snprintf(PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer), "%s", Path.c_str());
}

void FEditorViewerWindowWidget::LoadAnimSequenceToolbarIcons()
{
	if (bAnimSequenceToolbarIconsLoadAttempted)
	{
		return;
	}

	bAnimSequenceToolbarIconsLoadAttempted = true;
	if (!EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return;
	}

	const std::wstring IconDir = FEditorResourcePaths::ToolIconsAbsoluteDir();
	auto LoadIcon = [&](const wchar_t* FileName, TComPtr<ID3D11ShaderResourceView>& OutIcon)
	{
		const std::wstring IconPath = IconDir + FileName;
		DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, OutIcon.GetAddressOf());
	};

	LoadIcon(L"PlayControlsPlayForward.png", AnimSequencePlayIcon);
	LoadIcon(L"PlayControlsPause.png", AnimSequencePauseIcon);
	LoadIcon(L"PlayControlsPlayReverse.png", AnimSequenceReverseIcon);
	LoadIcon(L"PlayControlsToFront.png", AnimSequenceToFrontIcon);
	LoadIcon(L"PlayControlsToEnd.png", AnimSequenceToEndIcon);
	LoadIcon(L"PlayControlsLooping.png", AnimSequenceLoopingIcon);
	LoadIcon(L"PlayControlsNoLooping.png", AnimSequenceNoLoopingIcon);
	LoadIcon(L"PlayControlsToNexting.png", AnimSequenceToNextingIcon);
	LoadIcon(L"PlayControlsToPreviousing.png", AnimSequenceToPreviousingIcon);
}

bool FEditorViewerWindowWidget::DrawAnimSequenceIconButton(
	const char* Id,
	ID3D11ShaderResourceView* Icon,
	const char* Tooltip,
	const ImVec2& Size)
{
	ImGui::PushID(Id);
	const bool bClicked = ImGui::InvisibleButton("##IconButton", Size);
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(
		bActive ? ImVec4(0.16f, 0.19f, 0.24f, 1.0f) :
		bHovered ? ImVec4(0.13f, 0.16f, 0.20f, 1.0f) :
				   ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
	const ImU32 BorderColor = ImGui::GetColorU32(
		bHovered ? ImVec4(0.38f, 0.45f, 0.56f, 1.0f) : ImVec4(0.22f, 0.26f, 0.32f, 1.0f));
	DrawList->AddRectFilled(Min, Max, BgColor, 5.0f);
	DrawList->AddRect(Min, Max, BorderColor, 5.0f);

	if (Icon)
	{
		const float Padding = std::max(5.0f, Size.x * 0.18f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Min.x + Padding, Min.y + Padding),
			ImVec2(Max.x - Padding, Max.y - Padding));
	}
	else
	{
		const ImVec2 TextSize = ImGui::CalcTextSize("?");
		DrawList->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + (Size.y - TextSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
			"?");
	}

	if (bHovered && Tooltip)
	{
		ImGui::SetTooltip("%s", Tooltip);
	}

	ImGui::PopID();
	return bClicked;
}

bool FEditorViewerWindowWidget::SaveAnimSequenceAsset(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		return false;
	}

	FString SavePath = Sequence->GetAssetPath();
	if (SavePath.empty() && Viewer)
	{
		SavePath = Viewer->GetFileName();
	}

	if (SavePath.empty())
	{
		return false;
	}

	return FResourceManager::Get().SaveAnimSequence(SavePath, Sequence);
}

void FEditorViewerWindowWidget::RenderAnimSequenceToolbar(UAnimSequence* Sequence)
{
	constexpr ImGuiWindowFlags ToolbarFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::BeginChild("AnimSequenceToolbar", ImVec2(0.0f, 56.0f), false, ToolbarFlags);
	ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));

	if (!Sequence || !Viewer)
	{
		ImGui::TextDisabled("Animation sequence is not loaded.");
		ImGui::EndChild();
		return;
	}

	LoadAnimSequenceToolbarIcons();

	const bool bPlaying = Viewer->IsAnimationPlaying();
	const float PlayRate = Viewer->GetAnimationPlayRate();
	const bool bReversePlaying = bPlaying && PlayRate < 0.0f;
	const bool bForwardPlaying = bPlaying && PlayRate >= 0.0f;
	const ImVec2 ButtonSize(38.0f, 38.0f);

	if (DrawAnimSequenceIconButton("ToFront", AnimSequenceToFrontIcon.Get(), "To Front", ButtonSize))
	{
		Viewer->SetAnimationPlaying(false);
		Viewer->SetAnimationTime(0.0f);
	}
	ImGui::SameLine();
	if (DrawAnimSequenceIconButton(
		"Reverse",
		bReversePlaying ? AnimSequencePauseIcon.Get() : AnimSequenceReverseIcon.Get(),
		bReversePlaying ? "Pause" : "Reverse",
		ButtonSize))
	{
		if (bReversePlaying)
		{
			Viewer->SetAnimationPlaying(false);
		}
		else
		{
			const float ReverseRate = PlayRate == 0.0f ? -1.0f : -std::abs(PlayRate);
			Viewer->SetAnimationPlayRate(ReverseRate);
			Viewer->SetAnimationPlaying(true);
		}
	}
	ImGui::SameLine();
	if (DrawAnimSequenceIconButton(
		"Play",
		bForwardPlaying ? AnimSequencePauseIcon.Get() : AnimSequencePlayIcon.Get(),
		bForwardPlaying ? "Pause" : "Play",
		ButtonSize))
	{
		if (bForwardPlaying)
		{
			Viewer->SetAnimationPlaying(false);
		}
		else
		{
			const float ForwardRate = PlayRate == 0.0f ? 1.0f : std::abs(PlayRate);
			Viewer->SetAnimationPlayRate(ForwardRate);
			Viewer->SetAnimationPlaying(true);
		}
	}

	ImGui::SameLine();
	if (DrawAnimSequenceIconButton("ToEnd", AnimSequenceToEndIcon.Get(), "To End", ButtonSize))
	{
		Viewer->SetAnimationPlaying(false);
		Viewer->SetAnimationTime(std::max(0.0f, Viewer->GetAnimationLength()));
	}
	ImGui::SameLine(0.0f, 14.0f);
	const bool bLooping = Viewer->IsAnimationLooping();
	if (DrawAnimSequenceIconButton(
		"Loop",
		bLooping ? AnimSequenceLoopingIcon.Get() : AnimSequenceNoLoopingIcon.Get(),
		bLooping ? "Looping" : "No Looping",
		ButtonSize))
	{
		Viewer->SetAnimationLooping(!bLooping);
	}

	ImGui::SameLine(0.0f, 18.0f);
	const float CurrentTime = Viewer->GetAnimationCurrentTime();
	const float Length = std::max(0.0f, Viewer->GetAnimationLength());
	ImGui::Text("%.3f / %.3f sec", CurrentTime, Length);

	ImGui::EndChild();
}

void FEditorViewerWindowWidget::RenderAnimSequenceTimeline(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("No sequence");
		return;
	}

	const float Length = std::max(0.0f, Sequence->GetPlayLength());
	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	const float FrameRate = DataModel ? DataModel->GetFrameRate().AsDecimal() : 0.0f;
	const int32 FrameCount = DataModel ? DataModel->GetNumberOfFrames() : 0;

	ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	CanvasSize.y = std::max(CanvasSize.y, 170.0f);
	CanvasSize.x = std::max(CanvasSize.x, 1.0f);
	ImGui::InvisibleButton("##AnimTimelineCanvas", CanvasSize);

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(ImVec4(0.075f, 0.082f, 0.100f, 1.0f));
	const ImU32 TrackColor = ImGui::GetColorU32(ImVec4(0.105f, 0.116f, 0.140f, 1.0f));
	const ImU32 LineColor = ImGui::GetColorU32(ImVec4(0.32f, 0.35f, 0.42f, 1.0f));
	const ImU32 MajorLineColor = ImGui::GetColorU32(ImVec4(0.46f, 0.50f, 0.58f, 1.0f));
	const ImU32 TextColor = ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f));
	const ImU32 PlayheadColor = ImGui::GetColorU32(ImVec4(0.95f, 0.28f, 0.24f, 1.0f));
	const ImU32 NotifyColor = ImGui::GetColorU32(ImVec4(0.95f, 0.70f, 0.22f, 1.0f));
	const ImU32 NotifySelectedColor = ImGui::GetColorU32(ImVec4(1.0f, 0.84f, 0.36f, 1.0f));
	const ImU32 NotifyBodyColor = ImGui::GetColorU32(ImVec4(0.95f, 0.70f, 0.22f, 0.24f));

	DrawList->AddRectFilled(Min, Max, BgColor, 4.0f);
	const float RulerY = Min.y + 28.0f;
	const float TrackTop = Min.y + 54.0f;
	const float TrackBottom = Max.y - 18.0f;
	const float NotifyBarTop = TrackTop + 14.0f;
	const float NotifyBarBottom = TrackBottom - 14.0f;
	DrawList->AddRectFilled(ImVec2(Min.x + 8.0f, TrackTop), ImVec2(Max.x - 8.0f, TrackBottom), TrackColor, 4.0f);

	const float TrackMinX = Min.x + 12.0f;
	const float TrackMaxX = Max.x - 12.0f;
	const float TrackWidth = std::max(1.0f, TrackMaxX - TrackMinX);
	auto TimeToX = [&](float Time)
	{
		return TrackMinX + (Length > 0.0f ? std::clamp(Time / Length, 0.0f, 1.0f) : 0.0f) * TrackWidth;
	};
	auto XToTime = [&](float X)
	{
		return Length > 0.0f ? std::clamp((X - TrackMinX) / TrackWidth, 0.0f, 1.0f) * Length : 0.0f;
	};
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		PendingAnimNotifyTimeToAdd = XToTime(MousePos.x);
		ImGui::OpenPopup("AnimTimelineContextMenu");
	}

	if (ImGui::BeginPopup("AnimTimelineContextMenu"))
	{
		ImGui::Text("Add Notify at %.3f sec", PendingAnimNotifyTimeToAdd);
		ImGui::Separator();

		TArray<UClass*> NotifyClasses;
		GetSelectableAnimNotifyClasses(NotifyClasses);
		if (NotifyClasses.empty())
		{
			ImGui::TextDisabled("No AnimNotify classes are registered.");
		}
		else
		{
			for (UClass* Class : NotifyClasses)
			{
				const FString ClassName = Class->GetName();
				const FString NotifyName = Class->GetDisplayName();
				const FString ItemLabel = NotifyName + "##AddAnimNotify_" + ClassName;
				if (ImGui::Selectable(ItemLabel.c_str()))
				{
					const bool bIsStateNotify = Class->IsChildOf(UAnimNotifyState::StaticClass());
					const float Duration = bIsStateNotify
						? std::clamp(AnimNotifyDurationToAdd, 0.01f, std::max(0.01f, Length - PendingAnimNotifyTimeToAdd))
						: 0.0f;
					Sequence->AddNotify(PendingAnimNotifyTimeToAdd, FName(NotifyName), Duration, ClassName);
					SaveAnimSequenceAsset(Sequence);

					const TArray<FAnimNotifyStateEvent>& AddedNotifies = Sequence->GetNotifies();
					for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(AddedNotifies.size()); ++NotifyIndex)
					{
						const FAnimNotifyStateEvent& AddedNotify = AddedNotifies[NotifyIndex];
						if (AddedNotify.NotifyClassName == ClassName &&
							std::abs(AddedNotify.TriggerTime - PendingAnimNotifyTimeToAdd) <= 0.001f)
						{
							SelectedAnimNotifyIndex = NotifyIndex;
						}
					}

					if (Viewer)
					{
						Viewer->SetAnimationTime(PendingAnimNotifyTimeToAdd);
					}
				}
			}
		}

		ImGui::EndPopup();
	}

	const int32 DesiredTicks = std::max(2, static_cast<int32>(TrackWidth / 90.0f));
	for (int32 Tick = 0; Tick <= DesiredTicks; ++Tick)
	{
		const float Alpha = static_cast<float>(Tick) / static_cast<float>(DesiredTicks);
		const float X = TrackMinX + Alpha * TrackWidth;
		const float Time = Alpha * Length;
		const bool bMajor = (Tick % 2) == 0;
		DrawList->AddLine(ImVec2(X, RulerY), ImVec2(X, TrackBottom), bMajor ? MajorLineColor : LineColor, bMajor ? 1.2f : 1.0f);
		char Label[32];
		snprintf(Label, sizeof(Label), "%.2f", Time);
		DrawList->AddText(ImVec2(X + 3.0f, Min.y + 8.0f), TextColor, Label);
	}

	if (FrameRate > 0.0f && FrameCount > 0)
	{
		const int32 FrameStep = std::max(1, static_cast<int32>(FrameCount / std::max(1.0f, TrackWidth / 18.0f)));
		for (int32 Frame = 0; Frame <= FrameCount; Frame += FrameStep)
		{
			const float Time = static_cast<float>(Frame) / FrameRate;
			const float X = TimeToX(Time);
			DrawList->AddLine(ImVec2(X, RulerY + 14.0f), ImVec2(X, RulerY + 20.0f), LineColor, 1.0f);
		}
	}

	const TArray<FAnimNotifyStateEvent>& Notifies = Sequence->GetNotifies();
	int32 HoveredAnimNotifyIndex = -1;
	int32 HoveredAnimNotifyMode = 0;
	float HoveredPriority = FLT_MAX;

	for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
	{
		const FAnimNotifyStateEvent& Notify = Notifies[NotifyIndex];
		const float NotifyStartTime = std::clamp(Notify.TriggerTime, 0.0f, Length);
		const float NotifyDuration = std::clamp(Notify.Duration, 0.0f, std::max(0.0f, Length - NotifyStartTime));
		const float NotifyEndTime = NotifyStartTime + NotifyDuration;
		const float NotifyStartX = TimeToX(NotifyStartTime);
		const float NotifyEndX = TimeToX(NotifyEndTime);
		const bool bSelected = SelectedAnimNotifyIndex == NotifyIndex || DraggingAnimNotifyIndex == NotifyIndex;
		const ImU32 Color = bSelected ? NotifySelectedColor : NotifyColor;

		if (NotifyDuration > 0.0001f)
		{
			DrawList->AddRectFilled(
				ImVec2(NotifyStartX, NotifyBarTop),
				ImVec2(std::max(NotifyStartX + 2.0f, NotifyEndX), NotifyBarBottom),
				NotifyBodyColor,
				3.0f);
			DrawList->AddRect(
				ImVec2(NotifyStartX, NotifyBarTop),
				ImVec2(std::max(NotifyStartX + 2.0f, NotifyEndX), NotifyBarBottom),
				Color,
				3.0f,
				0,
				bSelected ? 2.0f : 1.2f);
			DrawList->AddLine(ImVec2(NotifyStartX, TrackTop - 8.0f), ImVec2(NotifyStartX, TrackBottom), Color, bSelected ? 2.2f : 1.4f);
			DrawList->AddLine(ImVec2(NotifyEndX, TrackTop), ImVec2(NotifyEndX, TrackBottom), Color, bSelected ? 2.2f : 1.4f);

			const bool bYInBar = MousePos.y >= NotifyBarTop - 6.0f && MousePos.y <= NotifyBarBottom + 6.0f;
			const float StartDistance = std::abs(MousePos.x - NotifyStartX);
			const float EndDistance = std::abs(MousePos.x - NotifyEndX);
			const bool bBodyHit = bYInBar && MousePos.x >= std::min(NotifyStartX, NotifyEndX) && MousePos.x <= std::max(NotifyStartX, NotifyEndX);

			if (bHovered && bYInBar && StartDistance <= 7.0f && StartDistance < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 2;
				HoveredPriority = StartDistance;
			}
			if (bHovered && bYInBar && EndDistance <= 7.0f && EndDistance < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 3;
				HoveredPriority = EndDistance;
			}
			if (bHovered && bBodyHit && HoveredPriority == FLT_MAX)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 1;
				HoveredPriority = 9999.0f;
			}
		}
		else
		{
			const float MarkerHalfWidth = bSelected ? 8.0f : 6.0f;
			const float MarkerHeight = bSelected ? 14.0f : 12.0f;
			const float LineThickness = bSelected ? 2.4f : 1.6f;

			DrawList->AddLine(ImVec2(NotifyStartX, TrackTop - 8.0f), ImVec2(NotifyStartX, TrackBottom), Color, LineThickness);
			DrawList->AddTriangleFilled(
				ImVec2(NotifyStartX, TrackTop - MarkerHeight),
				ImVec2(NotifyStartX - MarkerHalfWidth, TrackTop - 2.0f),
				ImVec2(NotifyStartX + MarkerHalfWidth, TrackTop - 2.0f),
				Color);

			const float DistanceToMouse = std::abs(MousePos.x - NotifyStartX);
			if (bHovered &&
				DistanceToMouse <= 8.0f &&
				MousePos.y >= TrackTop - 18.0f && MousePos.y <= TrackBottom &&
				DistanceToMouse < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 1;
				HoveredPriority = DistanceToMouse;
			}
		}
	}

	if (HoveredAnimNotifyIndex >= 0 && HoveredAnimNotifyIndex < static_cast<int32>(Notifies.size()))
	{
		const FAnimNotifyStateEvent& HoveredNotify = Notifies[HoveredAnimNotifyIndex];
		const char* ModeText = HoveredAnimNotifyMode == 2 ? "Drag start" : HoveredAnimNotifyMode == 3 ? "Drag end" : "Drag to move";
		ImGui::SetTooltip("Notify: %s\nStart: %.3f sec\nDuration: %.3f sec\nEnd: %.3f sec\n%s",
			HoveredNotify.NotifyName.ToString().c_str(),
			HoveredNotify.TriggerTime,
			HoveredNotify.Duration,
			HoveredNotify.GetEndTime(),
			ModeText);
	}

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoveredAnimNotifyIndex >= 0)
		{
			SelectedAnimNotifyIndex = HoveredAnimNotifyIndex;
			DraggingAnimNotifyIndex = HoveredAnimNotifyIndex;
			AnimNotifyDragMode = HoveredAnimNotifyMode;
			AnimNotifyDragGrabOffset = XToTime(MousePos.x) - Notifies[HoveredAnimNotifyIndex].TriggerTime;
			bAnimNotifyDragDirty = false;
			if (Viewer)
			{
				Viewer->SetAnimationPlaying(false);
				Viewer->SetAnimationTime(Notifies[HoveredAnimNotifyIndex].TriggerTime);
			}
		}
		else if (Viewer)
		{
			Viewer->SetAnimationTime(XToTime(MousePos.x));
		}
	}

	if (DraggingAnimNotifyIndex >= 0 && DraggingAnimNotifyIndex < static_cast<int32>(Notifies.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const FAnimNotifyStateEvent& Notify = Notifies[DraggingAnimNotifyIndex];
		const float MouseTime = XToTime(MousePos.x);
		bool bChanged = false;
		float PreviewTime = Notify.TriggerTime;

		if (AnimNotifyDragMode == 2)
		{
			const float OldEndTime = Notify.GetEndTime();
			const float NewStartTime = std::clamp(MouseTime, 0.0f, OldEndTime);
			bChanged = Sequence->SetNotifyTimeRange(DraggingAnimNotifyIndex, NewStartTime, OldEndTime - NewStartTime);
			PreviewTime = NewStartTime;
		}
		else if (AnimNotifyDragMode == 3)
		{
			const float NewEndTime = std::clamp(MouseTime, Notify.TriggerTime, Length);
			bChanged = Sequence->SetNotifyDuration(DraggingAnimNotifyIndex, NewEndTime - Notify.TriggerTime);
			PreviewTime = NewEndTime;
		}
		else
		{
			const float NewTriggerTime = std::clamp(MouseTime - AnimNotifyDragGrabOffset, 0.0f, std::max(0.0f, Length - Notify.Duration));
			bChanged = Sequence->SetNotifyTriggerTime(DraggingAnimNotifyIndex, NewTriggerTime);
			PreviewTime = NewTriggerTime;
		}

		if (bChanged)
		{
			bAnimNotifyDragDirty = true;
			if (Viewer)
			{
				Viewer->SetAnimationTime(PreviewTime);
			}
		}
	}
	else if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && Viewer)
	{
		Viewer->SetAnimationTime(XToTime(MousePos.x));
	}

	if (DraggingAnimNotifyIndex >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		int32 NewNotifyIndex = DraggingAnimNotifyIndex;
		const TArray<FAnimNotifyStateEvent>& UpdatedNotifies = Sequence->GetNotifies();
		if (DraggingAnimNotifyIndex < static_cast<int32>(UpdatedNotifies.size()))
		{
			const float FinalTriggerTime = UpdatedNotifies[DraggingAnimNotifyIndex].TriggerTime;
			if (bAnimNotifyDragDirty && Sequence->MoveNotifyAt(DraggingAnimNotifyIndex, FinalTriggerTime, &NewNotifyIndex))
			{
				SaveAnimSequenceAsset(Sequence);
			}
		}

		SelectedAnimNotifyIndex = NewNotifyIndex;
		DraggingAnimNotifyIndex = -1;
		AnimNotifyDragMode = 0;
		AnimNotifyDragGrabOffset = 0.0f;
		bAnimNotifyDragDirty = false;
	}

	const float CurrentTime = Viewer ? Viewer->GetAnimationCurrentTime() : 0.0f;
	const float PlayheadX = TimeToX(CurrentTime);
	DrawList->AddLine(ImVec2(PlayheadX, Min.y + 4.0f), ImVec2(PlayheadX, Max.y - 4.0f), PlayheadColor, 2.0f);
	DrawList->AddTriangleFilled(
		ImVec2(PlayheadX, Min.y + 4.0f),
		ImVec2(PlayheadX - 6.0f, Min.y + 16.0f),
		ImVec2(PlayheadX + 6.0f, Min.y + 16.0f),
		PlayheadColor);
}

void FEditorViewerWindowWidget::RenderAnimSequenceList(UAnimSequence* Sequence)
{
	(void)Sequence;

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Other Anim Sequences");

	const FString CurrentPath = Viewer ? FPaths::Normalize(Viewer->GetFileName()) : FString();
	const TArray<FString> Paths = FResourceManager::Get().GetAnimSequencePaths();
	int32 VisibleCount = 0;
	const float ListHeight = std::max(72.0f, ImGui::GetContentRegionAvail().y);
	if (ImGui::BeginChild("OtherAnimSequenceList", ImVec2(0.0f, ListHeight), false))
	{
		for (const FString& Path : Paths)
		{
			const FString NormalizedPath = FPaths::Normalize(Path);
			if (!CurrentPath.empty() && NormalizedPath == CurrentPath)
			{
				continue;
			}

			const FString Name = GetBaseFileNameWithoutExtension(Path);
			if (ImGui::Selectable(Name.c_str(), false))
			{
				if (EditorEngine && Viewer)
				{
					EditorEngine->GetMainPanel().ChangeViewerTarget(Viewer, NormalizedPath);
				}
			}
			++VisibleCount;
		}

		if (VisibleCount == 0)
		{
			ImGui::TextDisabled("No other .animseq files.");
		}
	}
	ImGui::EndChild();
}

void FEditorViewerWindowWidget::RenderAnimSequenceDetails(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh)
{
	ImGui::Text("Animation");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextDisabled("No animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextWrapped("%s", GetViewerAssetLabel(Viewer).c_str());
	ImGui::Text("Length: %.3f sec", Sequence->GetPlayLength());
	if (DataModel)
	{
		ImGui::Text("Sample Rate: %.3f", DataModel->GetFrameRate().AsDecimal());
		ImGui::Text("Frames: %d", DataModel->GetNumberOfFrames());
		ImGui::Text("Tracks: %d", static_cast<int32>(DataModel->GetBoneAnimationTracks().size()));
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Mesh");
	SyncPreviewMeshPathBuffer();
	if (PreviewMesh)
	{
		ImGui::TextWrapped("Loaded: %s", PreviewMesh->GetAssetPathFileName().c_str());
	}
	else
	{
		ImGui::TextWrapped("No compatible skeletal mesh is currently previewing it.");
	}
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##PreviewMeshPath", PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer));
	if (ImGui::Button("Load Preview Mesh"))
	{
		PreviewMeshPathBufferSource = PreviewMeshPathBuffer;
		Viewer->SetAnimationSequencePreviewMesh(PreviewMeshPathBufferSource);
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Notifies", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FAnimNotifyStateEvent>& Notifies = Sequence->GetNotifies();
		if (SelectedAnimNotifyIndex >= static_cast<int32>(Notifies.size()))
		{
			SelectedAnimNotifyIndex = -1;
			SelectedAnimNotifyNameBufferIndex = -1;
			SelectedAnimNotifyNameBuffer[0] = '\0';
		}

		if (Notifies.empty())
		{
			ImGui::TextDisabled("No notifies. Right-click the timeline to add one.");
		}
		else
		{
			for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
			{
				const FAnimNotifyStateEvent& Notify = Notifies[NotifyIndex];
				char Label[256];
				snprintf(
					Label,
					sizeof(Label),
					"%.3f - %.3f  %s  (%s)##AnimNotify%d",
					Notify.TriggerTime,
					Notify.GetEndTime(),
					Notify.NotifyName.ToString().c_str(),
					GetAnimNotifyClassDisplayName(Notify.NotifyClassName).c_str(),
					NotifyIndex);

				ImGui::PushID(NotifyIndex);
				if (ImGui::SmallButton("Delete"))
				{
					const int32 DeletedIndex = NotifyIndex;
					if (Sequence->RemoveNotifyAt(DeletedIndex))
					{
						SaveAnimSequenceAsset(Sequence);
						if (SelectedAnimNotifyIndex == DeletedIndex)
						{
							SelectedAnimNotifyIndex = -1;
						}
						else if (SelectedAnimNotifyIndex > DeletedIndex)
						{
							--SelectedAnimNotifyIndex;
						}

						if (DraggingAnimNotifyIndex == DeletedIndex)
						{
							DraggingAnimNotifyIndex = -1;
							AnimNotifyDragMode = 0;
							AnimNotifyDragGrabOffset = 0.0f;
							bAnimNotifyDragDirty = false;
						}
						else if (DraggingAnimNotifyIndex > DeletedIndex)
						{
							--DraggingAnimNotifyIndex;
						}

						SelectedAnimNotifyNameBufferIndex = -1;
						SelectedAnimNotifyNameBuffer[0] = '\0';
						ImGui::PopID();
						break;
					}
				}
				ImGui::SameLine();

				if (ImGui::Selectable(Label, SelectedAnimNotifyIndex == NotifyIndex))
				{
					SelectedAnimNotifyIndex = NotifyIndex;
					SelectedAnimNotifyNameBufferIndex = -1;
					if (Viewer)
					{
						Viewer->SetAnimationTime(Notify.TriggerTime);
					}
				}
				ImGui::PopID();
			}
		}

		const TArray<FAnimNotifyStateEvent>& CurrentNotifies = Sequence->GetNotifies();
		if (SelectedAnimNotifyIndex >= 0 && SelectedAnimNotifyIndex < static_cast<int32>(CurrentNotifies.size()))
		{
			ImGui::Spacing();
			ImGui::Separator();
			const FAnimNotifyStateEvent& SelectedNotify = CurrentNotifies[SelectedAnimNotifyIndex];
			ImGui::Text("Selected: %s", SelectedNotify.NotifyName.ToString().c_str());
			FString SelectedNotifyClassName = SelectedNotify.NotifyClassName.empty()
				? GetDefaultEditorNotifyClassName(SelectedNotify.Duration)
				: SelectedNotify.NotifyClassName;
			if (DrawAnimNotifyClassCombo("Class", SelectedNotifyClassName))
			{
				if (Sequence->SetNotifyClassName(SelectedAnimNotifyIndex, SelectedNotifyClassName))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}

			if (SelectedAnimNotifyNameBufferIndex != SelectedAnimNotifyIndex)
			{
				snprintf(
					SelectedAnimNotifyNameBuffer,
					sizeof(SelectedAnimNotifyNameBuffer),
					"%s",
					SelectedNotify.NotifyName.ToString().c_str());
				SelectedAnimNotifyNameBufferIndex = SelectedAnimNotifyIndex;
			}

			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::InputText("Name", SelectedAnimNotifyNameBuffer, sizeof(SelectedAnimNotifyNameBuffer)))
			{
				if (SelectedAnimNotifyNameBuffer[0] != '\0' && Sequence->SetNotifyName(SelectedAnimNotifyIndex, FName(FString(SelectedAnimNotifyNameBuffer))))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}

			float StartTime = Sequence->GetNotifies()[SelectedAnimNotifyIndex].TriggerTime;
			float Duration = Sequence->GetNotifies()[SelectedAnimNotifyIndex].Duration;
			ImGui::SetNextItemWidth(110.0f);
			if (ImGui::InputFloat("Start", &StartTime, 0.01f, 0.1f, "%.3f"))
			{
				if (Sequence->SetNotifyTriggerTime(SelectedAnimNotifyIndex, StartTime))
				{
					int32 NewNotifyIndex = SelectedAnimNotifyIndex;
					const float FinalTriggerTime = Sequence->GetNotifies()[SelectedAnimNotifyIndex].TriggerTime;
					Sequence->MoveNotifyAt(SelectedAnimNotifyIndex, FinalTriggerTime, &NewNotifyIndex);
					SelectedAnimNotifyIndex = NewNotifyIndex;
					SelectedAnimNotifyNameBufferIndex = -1;
					SaveAnimSequenceAsset(Sequence);
					if (Viewer)
					{
						Viewer->SetAnimationTime(std::clamp(StartTime, 0.0f, Sequence->GetPlayLength()));
					}
				}
			}
			ImGui::SetNextItemWidth(110.0f);
			if (ImGui::InputFloat("Duration", &Duration, 0.01f, 0.1f, "%.3f"))
			{
				if (Sequence->SetNotifyDuration(SelectedAnimNotifyIndex, Duration))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}
			ImGui::Text("End: %.3f", Sequence->GetNotifies()[SelectedAnimNotifyIndex].GetEndTime());
			ImGui::TextDisabled("Duration 0.0 is treated as an instant notify.");
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!Sequence->GetSourceFilePath().empty())
		{
			ImGui::TextWrapped("File: %s", Sequence->GetSourceFilePath().c_str());
		}
		if (!Sequence->GetSourceStackName().empty())
		{
			ImGui::TextWrapped("Stack: %s", Sequence->GetSourceStackName().c_str());
		}
		if (!Sequence->GetAssetPath().empty())
		{
			ImGui::TextWrapped("Asset: %s", Sequence->GetAssetPath().c_str());
		}
	}

	if (DataModel && ImGui::CollapsingHeader("Tracks"))
	{
		const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
		{
			const FBoneAnimationTrack& Track = Tracks[TrackIndex];
			if (ImGui::Selectable(Track.Name.ToString().c_str(), SelectedAnimTrackIndex == TrackIndex))
			{
				SelectedAnimTrackIndex = TrackIndex;
			}
		}
	}
}

void FEditorViewerWindowWidget::RenderAnimSequenceLeftPanel(UAnimSequence* Sequence, USkeletalMeshComponent* SkelMeshComp)
{
	ImGui::Text("Animation Sequence");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextWrapped("Could not load this animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextWrapped("%s", GetViewerAssetLabel(Viewer).c_str());
	ImGui::Spacing();
	ImGui::Text("Length: %.3f sec", Sequence->GetPlayLength());
	if (DataModel)
	{
		ImGui::Text("Sample Rate: %.3f", DataModel->GetFrameRate().AsDecimal());
		ImGui::Text("Frames: %d", DataModel->GetNumberOfFrames());
		ImGui::Text("Tracks: %d", static_cast<int32>(DataModel->GetBoneAnimationTracks().size()));
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Mesh");
	SyncPreviewMeshPathBuffer();
	if (SkelMeshComp && SkelMeshComp->GetSkeletalMesh())
	{
		ImGui::TextWrapped("Loaded: %s", SkelMeshComp->GetSkeletalMesh()->GetAssetPathFileName().c_str());
	}
	else
	{
		ImGui::TextWrapped("No preview mesh. Set a skeletal FBX path, or reimport the animseq with PreviewMeshPath.");
	}
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##PreviewMeshPath", PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer));
	if (ImGui::Button("Load Preview Mesh"))
	{
		PreviewMeshPathBufferSource = PreviewMeshPathBuffer;
		Viewer->SetAnimationSequencePreviewMesh(PreviewMeshPathBufferSource);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Playback");

	const bool bPlaying = Viewer->IsAnimationPlaying();
	if (ImGui::Button(bPlaying ? "Pause" : "Play"))
	{
		Viewer->SetAnimationPlaying(!bPlaying);
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
	{
		Viewer->RestartAnimation();
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		Viewer->SetAnimationPlaying(false);
		Viewer->SetAnimationTime(0.0f);
	}

	bool bLooping = Viewer->IsAnimationLooping();
	if (ImGui::Checkbox("Loop", &bLooping))
	{
		Viewer->SetAnimationLooping(bLooping);
	}

	float PlayRate = Viewer->GetAnimationPlayRate();
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::DragFloat("Play Rate", &PlayRate, 0.01f, -4.0f, 4.0f, "%.2f"))
	{
		Viewer->SetAnimationPlayRate(PlayRate);
	}

	float CurrentTime = Viewer->GetAnimationCurrentTime();
	const float Length = std::max(0.0f, Viewer->GetAnimationLength());
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::SliderFloat("Time", &CurrentTime, 0.0f, std::max(Length, 0.001f), "%.3f"))
	{
		Viewer->SetAnimationTime(CurrentTime);
	}

	if (DataModel && DataModel->GetFrameRate().AsDecimal() > 0.0f)
	{
		const float Frame = CurrentTime * DataModel->GetFrameRate().AsDecimal();
		ImGui::Text("Frame: %.1f / %d", Frame, DataModel->GetNumberOfFrames());
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Tracks");
	if (!DataModel || DataModel->GetBoneAnimationTracks().empty())
	{
		ImGui::TextDisabled("No bone tracks.");
		return;
	}

	const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
	if (SelectedAnimTrackIndex >= static_cast<int32>(Tracks.size()))
	{
		SelectedAnimTrackIndex = -1;
	}
	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
	{
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (SelectedAnimTrackIndex == TrackIndex)
		{
			Flags |= ImGuiTreeNodeFlags_Selected;
		}
		const bool bTrackOpen = ImGui::TreeNodeEx((void*)(intptr_t)TrackIndex, Flags, "%s", Tracks[TrackIndex].Name.ToString().c_str());
		if (ImGui::IsItemClicked())
		{
			SelectedAnimTrackIndex = TrackIndex;
		}
		if (bTrackOpen)
		{
			ImGui::TreePop();
		}
	}
}

void FEditorViewerWindowWidget::RenderAnimSequenceRightPanel(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh)
{
	ImGui::Text("Details");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextDisabled("No animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextDisabled("Source");
	if (!Sequence->GetSourceFilePath().empty())
	{
		ImGui::TextWrapped("File: %s", Sequence->GetSourceFilePath().c_str());
	}
	if (!Sequence->GetSourceStackName().empty())
	{
		ImGui::TextWrapped("Stack: %s", Sequence->GetSourceStackName().c_str());
	}
	if (!Sequence->GetAssetPath().empty())
	{
		ImGui::TextWrapped("Asset: %s", Sequence->GetAssetPath().c_str());
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Skeleton");
	if (PreviewMesh && PreviewMesh->HasValidMeshData())
	{
		ImGui::Text("Bones: %d", static_cast<int32>(PreviewMesh->GetBones().size()));
		ImGui::Text("Sections: %d", static_cast<int32>(PreviewMesh->GetSections().size()));
		ImGui::Text("Vertices: %d", static_cast<int32>(PreviewMesh->GetVertices().size()));
	}
	else
	{
		ImGui::TextWrapped("The sequence is loaded, but no compatible skeletal mesh is currently previewing it.");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Selected Track");
	if (!DataModel || SelectedAnimTrackIndex < 0 || SelectedAnimTrackIndex >= static_cast<int32>(DataModel->GetBoneAnimationTracks().size()))
	{
		ImGui::TextDisabled("Select a track on the left.");
		return;
	}

	const FBoneAnimationTrack& Track = DataModel->GetBoneAnimationTracks()[SelectedAnimTrackIndex];
	const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;
	ImGui::TextWrapped("Bone: %s", Track.Name.ToString().c_str());
	ImGui::Text("Position Keys: %d", static_cast<int32>(RawTrack.PosKeys.size()));
	ImGui::Text("Rotation Keys: %d", static_cast<int32>(RawTrack.RotKeys.size()));
	ImGui::Text("Scale Keys: %d", static_cast<int32>(RawTrack.ScaleKeys.size()));

	if (!RawTrack.PosKeys.empty())
	{
		const FVector3f& Key = RawTrack.PosKeys.front();
		ImGui::Text("First Pos: %.3f, %.3f, %.3f", Key.X, Key.Y, Key.Z);
	}
	if (!RawTrack.ScaleKeys.empty())
	{
		const FVector3f& Key = RawTrack.ScaleKeys.front();
		ImGui::Text("First Scale: %.3f, %.3f, %.3f", Key.X, Key.Y, Key.Z);
	}
}

void FEditorViewerWindowWidget::RenderBoneDetails(USkeletalMeshComponent* SkelComp)
{
    const int32 SelectedBoneIndex = Viewer ? Viewer->GetSelectedBoneIndex() : -1;
    if (!SkelComp || SelectedBoneIndex == -1) return;

    const FBoneInfo& Bone = SkelComp->GetSkeletalMesh()->GetMeshData()->Bones[SelectedBoneIndex];
    ImGui::Text("Bone: %s (Index: %d)", Bone.Name.c_str(), SelectedBoneIndex);
    ImGui::Spacing();

    FMatrix LocalTransform = SkelComp->GetBoneLocalTransform(SelectedBoneIndex);
    FVector Location, Scale;
    FMatrix RotationMatrix;
    LocalTransform.Decompose(Location, RotationMatrix, Scale);

    // 외부(기즈모 등)에서 회전이 변경되었는지 확인
    FVector CurrentEuler = RotationMatrix.GetEuler();
    FVector& CachedRotation = Viewer->GetCachedBoneRotation();

	if ((CurrentEuler - FMatrix::MakeRotationEuler(CachedRotation).GetEuler()).Size() > 0.01f)
    {
        CachedRotation = CurrentEuler;
    }

    bool bEdited = false;

    auto DrawTransformField = [&](const char* Label, FVector& Value, float Speed) {
        float Arr[3] = { Value.X, Value.Y, Value.Z };
        if (ImGui::DragFloat3(Label, Arr, Speed))
        {
            Value = FVector(Arr[0], Arr[1], Arr[2]);
            return true;
        }
        return false;
    };

    ImGui::Text("Transform (Local)");
    if (DrawTransformField("Location", Location, 0.1f)) bEdited = true;
    if (DrawTransformField("Rotation", CachedRotation, 0.1f)) bEdited = true;
    if (DrawTransformField("Scale", Scale, 0.01f)) bEdited = true;

    if (bEdited)
    {
        FMatrix NewLocal = FMatrix::MakeTRS(Location, FMatrix::MakeRotationEuler(CachedRotation), Scale);
        SkelComp->SetBoneLocalTransform(SelectedBoneIndex, NewLocal);

        // Gizmo 위치 업데이트
        FViewportClient* BaseClient = Viewer->GetViewport().GetClient();
        FEditorViewportClient* EditorClient = static_cast<FEditorViewportClient*>(BaseClient);
        if (UGizmoComponent* Gizmo = EditorClient->GetGizmo())
        {
            Gizmo->UpdateGizmoTransform();
        }
    }
}

void FEditorViewerWindowWidget::DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children)
{
    const FBoneInfo& Bone = Bones[BoneIndex];

    // socket까지 자식으로 그리므로 "자식 없음"은 bone-children + socket-children 모두 비어야 성립.
    const bool bHasBoneChildren   = Children[BoneIndex].size() > 0;
    const bool bHasSocketChildren = BoneIndex < static_cast<int32>(BoneToSocketIndices.size())
                                    && BoneToSocketIndices[BoneIndex].size() > 0;

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!bHasBoneChildren && !bHasSocketChildren)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (Viewer->GetSelectedBoneIndex() == BoneIndex)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool bOpen = ImGui::TreeNodeEx(
        (void*)(intptr_t)BoneIndex,
        Flags,
        "%s",
        Bone.Name.c_str());

    // 클릭 → bone 선택. socket 선택은 해제 (상호 배타).
    if (ImGui::IsItemClicked())
    {
        Viewer->SelectBone(BoneIndex);
    }

    // 우클릭 컨텍스트
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Add Socket"))
        {
            AddSocketOnBone(BoneIndex);
        }
        ImGui::Separator();

        const bool bCanToggleChildren = bHasBoneChildren || bHasSocketChildren;
        if (ImGui::MenuItem("Expand Children", nullptr, false, bCanToggleChildren))
        {
            QueueBoneSubtreeOpenState(BoneIndex, true);
        }
        if (ImGui::MenuItem("Collapse Children", nullptr, false, bCanToggleChildren))
        {
            QueueBoneSubtreeOpenState(BoneIndex, false);
        }
        ImGui::EndPopup();
    }

    if (bOpen)
    {
        // (1) 자식 bone들
        for (int32 ChildIndex : Children[BoneIndex])
        {
            DrawBoneNode(ChildIndex, Bones, Children);
        }

        // (2) 이 bone에 매달린 socket들 (자식 bone 다음에 표시)
        if (bHasSocketChildren)
        {
            for (int32 SocketIdx : BoneToSocketIndices[BoneIndex])
            {
                DrawSocketNode(SocketIdx);
            }
        }

        ImGui::TreePop();
    }
}

void FEditorViewerWindowWidget::QueueBoneSubtreeOpenState(int32 BoneIdx, bool bOpen)
{
    PendingBoneTreeOpenStateRoot = BoneIdx;
    bPendingBoneTreeOpenStateValue = bOpen;
}

void FEditorViewerWindowWidget::ApplyPendingBoneTreeOpenState(const FSkeletalMesh* MeshData)
{
    if (!MeshData || PendingBoneTreeOpenStateRoot < 0)
    {
        return;
    }

    SetBoneSubtreeOpenState(PendingBoneTreeOpenStateRoot, Children, bPendingBoneTreeOpenStateValue);
    PendingBoneTreeOpenStateRoot = -1;
}

void FEditorViewerWindowWidget::SetBoneSubtreeOpenState(
    int32 BoneIdx,
    const TArray<TArray<int32>>& InChildren,
    bool bOpen)
{
    if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(InChildren.size()))
    {
        return;
    }

    ImGuiStorage* Storage = ImGui::GetStateStorage();
    if (!Storage)
    {
        return;
    }

    const void* NodePtr = reinterpret_cast<void*>(static_cast<intptr_t>(BoneIdx));
    const ImGuiID NodeId = ImGui::GetID(NodePtr);

    // Expand는 부모가 먼저 열려야 화면에서 즉시 전체 subtree가 보인다.
    if (bOpen)
    {
        Storage->SetInt(NodeId, 1);
    }

    ImGui::PushID(NodePtr);
    for (int32 ChildIndex : InChildren[BoneIdx])
    {
        SetBoneSubtreeOpenState(ChildIndex, InChildren, bOpen);
    }
    ImGui::PopID();

    // Collapse는 자식부터 닫고 마지막에 부모를 닫아야,
    // 부모를 다시 열었을 때 이전에 열려 있던 하위 노드가 되살아나지 않는다.
    if (!bOpen)
    {
        Storage->SetInt(NodeId, 0);
    }
}

void FEditorViewerWindowWidget::DrawSocketNode(int32 SocketIdx)
{
    if (!CachedMesh) return;
    if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

    const FSkeletalMeshSocket& Socket = CachedMesh->Sockets[SocketIdx];

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_NoTreePushOnOpen;   // leaf니까 자식 push 불필요

    if (Viewer && Viewer->GetSelectedSocketIndex() == SocketIdx)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    // bone ID 공간(int32 직접)과 충돌하지 않게 high-bit 네임스페이스.
    const void* NodeId = reinterpret_cast<const void*>(
        static_cast<uintptr_t>(0x80000000u | static_cast<uint32>(SocketIdx)));

    // socket을 시각적으로 구분 — cyan-ish, "◇" prefix
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
    ImGui::TreeNodeEx(NodeId, Flags, "\xe2\x97\x87 %s", Socket.Name.ToString().c_str());   // ◇
    ImGui::PopStyleColor();

    // 클릭 → socket 선택. bone 선택은 해제.
    if (ImGui::IsItemClicked())
    {
        if (Viewer)
        {
            Viewer->SelectSocket(SocketIdx);
        }
    }

    // 우클릭 컨텍스트
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Add Preview Mesh..."))
        {
            // 모달은 popup 바깥에서 OpenPopup해야 안정적 — 여기선 트리거 idx만 기록.
            PendingPreviewPickerSocketIdx = SocketIdx;
        }

        const bool bHasPreview = HasPreview(Socket.Name);
        if (ImGui::MenuItem("Remove Preview Mesh", nullptr, false, bHasPreview))
        {
            if (EditorEngine)
            {
                Viewer->ClearSocketPreview(Socket.Name);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename"))
        {
            RenameSocketIdx = SocketIdx;
            std::snprintf(RenameBuffer, sizeof(RenameBuffer), "%s",
                          Socket.Name.ToString().c_str());
        }

        if (ImGui::MenuItem("Delete Socket"))
        {
            DeleteSocket(SocketIdx);
        }

        ImGui::EndPopup();
    }
}

void FEditorViewerWindowWidget::RebuildBoneTreeCaches(const FSkeletalMesh* MeshData)
{
    Children.clear();
    BoneToSocketIndices.clear();
    if (!MeshData) return;

    const int32 BoneCount = static_cast<int32>(MeshData->Bones.size());
    Children.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 Parent = MeshData->Bones[i].ParentIndex;
        if (Parent >= 0)
        {
            Children[Parent].push_back(i);
        }
    }

    RebuildBoneToSocketIndices(MeshData);
}

void FEditorViewerWindowWidget::RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData)
{
    BoneToSocketIndices.clear();
    if (!MeshData) return;

    const int32 BoneCount = static_cast<int32>(MeshData->Bones.size());
    BoneToSocketIndices.resize(BoneCount);

    for (int32 i = 0; i < static_cast<int32>(MeshData->Sockets.size()); ++i)
    {
        const int32 B = MeshData->Sockets[i].BoneIndex;
        if (B >= 0 && B < BoneCount)
        {
            BoneToSocketIndices[B].push_back(i);
        }
    }
}

void FEditorViewerWindowWidget::AddSocketOnBone(int32 BoneIdx)
{
    if (!CachedMesh) return;
    if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(CachedMesh->Bones.size())) return;

    FSkeletalMeshSocket NewSocket;
    NewSocket.Name = FName(GenerateUniqueSocketName());
    NewSocket.BoneIndex = BoneIdx;
    // Loc/Rot/Scale은 기본값(0, identity, 1)

    CachedMesh->Sockets.push_back(NewSocket);
    const int32 NewIdx = static_cast<int32>(CachedMesh->Sockets.size()) - 1;

    RebuildBoneToSocketIndices(CachedMesh);

    if (Viewer)
    {
        Viewer->SelectSocket(NewIdx);
    }
    bMeshDirty = true;

    // socket-attached children의 transform이 새로 계산되도록 본 자세 dirty 전파 트리거.
    if (CachedSkComp)
    {
        CachedSkComp->MarkSkinningDirty();
    }
}

FString FEditorViewerWindowWidget::GenerateUniqueSocketName(const char* Base) const
{
    if (!CachedMesh) return FString(Base);

    auto Exists = [&](const FString& Candidate) -> bool {
        const FName CandidateName(Candidate);
        for (const FSkeletalMeshSocket& S : CachedMesh->Sockets)
        {
            if (S.Name == CandidateName) return true;
        }
        return false;
    };

    FString Candidate = Base;
    if (!Exists(Candidate)) return Candidate;

    for (int32 i = 1; i < 10000; ++i)
    {
        Candidate = FString(Base) + "_" + std::to_string(i);
        if (!Exists(Candidate)) return Candidate;
    }
    return Candidate;   // 폴백 — 거의 도달 불가
}

void FEditorViewerWindowWidget::DeleteSocket(int32 SocketIdx)
{
    if (!CachedMesh) return;
    if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

    // (1) 해당 socket에 매달린 preview mesh 먼저 정리
    const FName SocketName = CachedMesh->Sockets[SocketIdx].Name;
    if (EditorEngine && Viewer)
    {
        Viewer->ClearSocketPreview(SocketName);
    }

    // (2) Sockets 배열에서 erase. 다른 socket들의 인덱스가 시프트됨.
    CachedMesh->Sockets.erase(CachedMesh->Sockets.begin() + SocketIdx);

    // (3) BoneToSocketIndices 통째 재빌드 (시프트된 인덱스 반영)
    RebuildBoneToSocketIndices(CachedMesh);

    // (4) 선택 상태 정리
    if (Viewer)
    {
        Viewer->NotifySocketDeleted(SocketIdx);
    }

    bMeshDirty = true;

    if (CachedSkComp)
    {
        CachedSkComp->MarkSkinningDirty();
    }
}

bool FEditorViewerWindowWidget::HasPreview(const FName& SocketName) const
{
    if (!EditorEngine || !Viewer) return false;
    return Viewer->FindPreviewMesh(SocketName) != nullptr;
}

void FEditorViewerWindowWidget::DrawSocketInspector()
{
    // Save 상태는 socket 선택 여부와 무관하게 항상 보이는 게 편함.
    auto DrawSaveButton = [&]() {
        const bool bCanSave = CanSaveMesh();
        if (!bCanSave) ImGui::BeginDisabled();
        const char* Label = IsMeshDirty() ? "Save Mesh *" : "Save Mesh";
        if (ImGui::Button(Label))
        {
            TriggerSaveMesh();
        }
        if (!bCanSave) ImGui::EndDisabled();
    };

    const int32 SelectedSocketIndex = Viewer ? Viewer->GetSelectedSocketIndex() : -1;
    if (!CachedMesh || SelectedSocketIndex < 0 ||
        SelectedSocketIndex >= static_cast<int32>(CachedMesh->Sockets.size()))
    {
        ImGui::TextDisabled("(no socket selected)");
        DrawSaveButton();
        return;
    }

    FSkeletalMeshSocket& Socket = CachedMesh->Sockets[SelectedSocketIndex];

    ImGui::Text("Socket: %s", Socket.Name.ToString().c_str());

    // Bone 콤보
    const TArray<FBoneInfo>& Bones = CachedMesh->Bones;
    const char* CurrentBoneName = (Socket.BoneIndex >= 0 && Socket.BoneIndex < (int32)Bones.size())
        ? Bones[Socket.BoneIndex].Name.c_str()
        : "<invalid>";

    if (ImGui::BeginCombo("Bone", CurrentBoneName))
    {
        for (int32 i = 0; i < static_cast<int32>(Bones.size()); ++i)
        {
            const bool bSelected = (Socket.BoneIndex == i);
            if (ImGui::Selectable(Bones[i].Name.c_str(), bSelected))
            {
                if (Socket.BoneIndex != i)
                {
                    Socket.BoneIndex = i;
                    RebuildBoneToSocketIndices(CachedMesh);   // 트리에서 새 본 밑으로 이동
                    bMeshDirty = true;
                    if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
                }
            }
            if (bSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Location / Rotation / Scale
    // FVector / FRotator 모두 contiguous 3 float — &X / &Pitch로 DragFloat3에 전달.
    bool bChanged = false;
    bChanged |= ImGui::DragFloat3("Location", &Socket.RelativeLocation.X, 0.5f);
    bChanged |= ImGui::DragFloat3("Rotation (P/Y/R)", &Socket.RelativeRotation.Pitch, 0.5f);
    bChanged |= ImGui::DragFloat3("Scale", &Socket.RelativeScale.X, 0.01f, 0.001f, 100.0f);

    if (bChanged)
    {
        bMeshDirty = true;
        if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
    }

    ImGui::Separator();
    DrawSaveButton();
}

void FEditorViewerWindowWidget::TriggerSaveMesh()
{
	RequestSaveMesh();
}

bool FEditorViewerWindowWidget::IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const
{
    if (!CachedMesh) return false;
    const FName CandidateName(Candidate);
    for (int32 i = 0; i < static_cast<int32>(CachedMesh->Sockets.size()); ++i)
    {
        if (i == IgnoreIdx) continue;
        if (CachedMesh->Sockets[i].Name == CandidateName) return false;
    }
    return true;
}

void FEditorViewerWindowWidget::DrawRenameModal()
{
    if (!ImGui::BeginPopupModal("RenameSocket", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    // 무효한 상태 — 즉시 닫기
    if (!CachedMesh || RenameSocketIdx < 0 ||
        RenameSocketIdx >= static_cast<int32>(CachedMesh->Sockets.size()))
    {
        RenameSocketIdx = -1;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::Text("Rename socket:");
    ImGui::InputText("##rename", RenameBuffer, sizeof(RenameBuffer));

    const FString Candidate(RenameBuffer);
    const bool bEmpty  = Candidate.empty();
    const bool bUnique = !bEmpty && IsSocketNameUnique(Candidate, RenameSocketIdx);
    const bool bValid  = !bEmpty && bUnique;

    if (bEmpty)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Name cannot be empty");
    }
    else if (!bUnique)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Name already in use");
    }

    if (!bValid) ImGui::BeginDisabled();
    if (ImGui::Button("OK"))
    {
        // Preview mesh가 이 socket에 attach되어 있다면 key가 socket name이므로,
        // 이름 변경 시 preview를 깔끔히 재attach해야 함.
        const FName OldName = CachedMesh->Sockets[RenameSocketIdx].Name;
        const FName NewName(Candidate);

        FString PreviewPath;
        if (EditorEngine && Viewer)
        {
            UStaticMeshComponent* Preview = Viewer->FindPreviewMesh(OldName);
            if (Preview && Preview->GetStaticMesh())
            {
                PreviewPath = Preview->GetStaticMesh()->GetAssetPathFileName();
                Viewer->ClearSocketPreview(OldName);
            }
        }

        CachedMesh->Sockets[RenameSocketIdx].Name = NewName;

        if (!PreviewPath.empty() && EditorEngine && Viewer)
        {
            Viewer->SetSocketPreviewMesh(NewName, PreviewPath);
        }

        if (Viewer && Viewer->GetSelectedSocketIndex() == RenameSocketIdx)
        {
            Viewer->SelectSocket(RenameSocketIdx);
        }

        bMeshDirty = true;
        if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
        RenameSocketIdx = -1;
        ImGui::CloseCurrentPopup();
    }
    if (!bValid) ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        RenameSocketIdx = -1;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void FEditorViewerWindowWidget::DrawPreviewPickerModal()
{
    if (!ImGui::BeginPopupModal("PickStaticMesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    static char Filter[256] = "";
    ImGui::InputText("Filter", Filter, sizeof(Filter));
    ImGui::Separator();

    const TArray<FString>& Paths = FResourceManager::Get().GetStaticMeshPaths();

    ImGui::BeginChild("PickList", ImVec2(420.0f, 300.0f), true);
    for (const FString& Path : Paths)
    {
        if (Filter[0] != '\0' && Path.find(Filter) == FString::npos)
        {
            continue;
        }

        if (ImGui::Selectable(Path.c_str()))
        {
            if (CachedMesh && EditorEngine && Viewer &&
                PendingPreviewPickerSocketIdx >= 0 &&
                PendingPreviewPickerSocketIdx < static_cast<int32>(CachedMesh->Sockets.size()))
            {
                const FName SocketName = CachedMesh->Sockets[PendingPreviewPickerSocketIdx].Name;
                Viewer->SetSocketPreviewMesh(SocketName, Path);
            }
            PendingPreviewPickerSocketIdx = -1;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Cancel"))
    {
        PendingPreviewPickerSocketIdx = -1;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
