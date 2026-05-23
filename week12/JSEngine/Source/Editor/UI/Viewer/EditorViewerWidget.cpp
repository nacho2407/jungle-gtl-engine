#include "EditorViewerWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorChromeConstants.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/SkeletalAssetEditorViewer.h"
#include "Viewport/ViewportLayout.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "imgui.h"
#include "ImGui/imgui_impl_win32.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cmath>
#include <functional>

namespace
{
constexpr float DetachedHomeAreaWidth = 82.0f;
constexpr float DetachedHomeIconSize = 62.0f;
constexpr float DetachedHomeCircleRadius = 34.0f;

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

FSkeletalAssetEditorViewer* AsSkeletalAssetViewer(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		return nullptr;
	}

	const EEditorTabKind TabKind = Viewer->GetTabKind();
	if (TabKind == EEditorTabKind::SkeletalMeshViewer || TabKind == EEditorTabKind::AnimSequenceViewer)
	{
		return static_cast<FSkeletalAssetEditorViewer*>(Viewer);
	}
	return nullptr;
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

void DrawHomeChromeIcon(
	ImDrawList* DrawList,
	ID3D11ShaderResourceView* HomeIcon,
	const ImVec2& Min,
	const ImVec2& Max,
	bool bHovered)
{
	if (!DrawList)
	{
		return;
	}

	const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
	DrawList->AddRectFilled(
		Min,
		Max,
		ImGui::GetColorU32(bHovered ? ImVec4(0.095f, 0.102f, 0.125f, 1.0f) : ImVec4(0.055f, 0.060f, 0.072f, 1.0f)));
	DrawList->AddCircleFilled(
		Center,
		DetachedHomeCircleRadius,
		ImGui::GetColorU32(bHovered ? ImVec4(0.13f, 0.14f, 0.17f, 1.0f) : ImVec4(0.075f, 0.080f, 0.095f, 1.0f)),
		68);
	if (HomeIcon)
	{
		const ImVec2 IconMin(Center.x - DetachedHomeIconSize * 0.5f, Center.y - DetachedHomeIconSize * 0.5f);
		const ImVec2 IconMax(IconMin.x + DetachedHomeIconSize, IconMin.y + DetachedHomeIconSize);
		DrawList->AddImage(reinterpret_cast<ImTextureID>(HomeIcon), IconMin, IconMax);
	}
	else
	{
		const ImU32 HomeColor = ImGui::GetColorU32(ImVec4(0.92f, 0.94f, 0.98f, 1.0f));
		DrawList->AddText(ImVec2(Center.x - 7.0f, Center.y - 7.0f), HomeColor, "JS");
	}
	DrawList->AddCircle(
		Center,
		DetachedHomeCircleRadius,
		ImGui::GetColorU32(ImVec4(0.92f, 0.94f, 0.98f, 1.0f)),
		68,
		1.8f);
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

}

void FEditorViewerWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorViewerWidget::Shutdown()
{
	Viewer = nullptr;
	bOpen = false;
}

FString FEditorViewerWidget::GetWindowName() const
{
	char WindowName[64];
	sprintf_s(WindowName, "###ViewerWindow_%p", Viewer);
	return GetViewerAssetLabel(Viewer) + WindowName;
}

bool FEditorViewerWidget::CanSaveMesh() const
{
	return false;
}

bool FEditorViewerWidget::IsMeshDirty() const
{
	return false;
}

void FEditorViewerWidget::RequestSaveMesh()
{
}

void FEditorViewerWidget::Render(float DeltaTime)
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

void FEditorViewerWidget::RenderDetachedDocumentChrome(bool& bDockRequested, bool& bCloseRequested)
{
	if (!Viewer || !ImGui::BeginMenuBar())
	{
		return;
	}

	constexpr float WindowButtonWidth = 48.0f;
	constexpr float TitleBarHeight = FEditorChromeMetrics::ApplicationTitleBarHeight;
	constexpr float MenuStartX = DetachedHomeAreaWidth;

	HWND ViewportHwnd = GetCurrentViewportHwnd();
	const ImVec2 WindowPos = ImGui::GetWindowPos();
	const ImVec2 WindowSize = ImGui::GetWindowSize();
	const float ButtonStartX = std::max(0.0f, WindowSize.x - WindowButtonWidth * 3.0f);

	ImGui_ImplWin32_CustomChromeRect ChromeRects[16] = {};
	int ChromeRectCount = 0;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 HomeMin(WindowPos.x, WindowPos.y);
	const ImVec2 HomeMax(
		WindowPos.x + DetachedHomeAreaWidth,
		WindowPos.y + TitleBarHeight + FEditorChromeMetrics::DocumentToolbarHeight);
	const bool bHomeHovered = ImGui::IsMouseHoveringRect(HomeMin, HomeMax);
	ID3D11ShaderResourceView* HomeIcon = EditorEngine
		? EditorEngine->GetMainPanel().GetHomeIconResource()
		: nullptr;
	DrawHomeChromeIcon(ImGui::GetForegroundDrawList(), HomeIcon, HomeMin, HomeMax, bHomeHovered);
	AddChromeRect(
		ChromeRects,
		ChromeRectCount,
		HomeMin,
		ImVec2(HomeMax.x, WindowPos.y + TitleBarHeight),
		WindowPos);
	if (bHomeHovered)
	{
		ImGui::SetTooltip("Level");
	}

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
		if (FSkeletalAssetEditorViewer* SkeletalViewer = AsSkeletalAssetViewer(Viewer))
		{
			FSkeletalViewerShowFlags& ShowFlags = SkeletalViewer->GetClient().GetShowFlags();
			ImGui::MenuItem("Bones", nullptr, &ShowFlags.bShowBones);
			ImGui::MenuItem("Bounding Box", nullptr, &ShowFlags.bShowBoundingBox);
			ImGui::MenuItem("Outline", nullptr, &ShowFlags.bShowOutline);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Help"))
	{
		ImGui::TextDisabled(Viewer->GetViewerLabel());
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

void FEditorViewerWidget::RenderDetachedDocumentToolbar(bool& bDockRequested)
{
	if (!Viewer || !EditorEngine)
	{
		return;
	}

	constexpr ImGuiWindowFlags ToolbarFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::BeginChild("##DetachedViewerToolbar", ImVec2(0.0f, 40.0f), false, ToolbarFlags);
	ImGui::SetCursorPos(ImVec2(DetachedHomeAreaWidth + 8.0f, 6.0f));

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

void FEditorViewerWidget::RenderEmbedded(float DeltaTime)
{
	if (!bOpen || !EditorEngine || !Viewer)
	{
		return;
	}

	RenderContent(DeltaTime);
}

void FEditorViewerWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	if (!Viewer)
	{
		return;
	}

	FSceneViewport& SceneViewport = Viewer->GetViewport();
	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();

	if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
		return;
	}

	RenderViewportPanel(SceneViewport, SRV, ImGui::GetContentRegionAvail());
}

void FEditorViewerWidget::RenderViewportPanel(FSceneViewport& SceneViewport, ID3D11ShaderResourceView* SRV, const ImVec2& Size)
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


