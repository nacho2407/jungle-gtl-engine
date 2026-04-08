#include "EditorUI.h"

#include "EditorEngine.h"
#include "Object/Object.h"
#include "Level/Level.h"
#include "Actor/Actor.h"
#include "Component/SceneComponent.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Renderer.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "Core/ViewportClient.h"
#include "Core/Paths.h"

#include <windows.h>
#include <commdlg.h>

#include "Debug/EngineLog.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Serializer/SceneSerializer.h"
#include "Core/ShowFlags.h"
#include "Viewport/EditorViewportClient.h"
#include "World/WorldContext.h"

enum class EFileDialogType
{
	Open,
	Save
};

std::string GetFilePathUsingDialog(EFileDialogType Type)
{
	wchar_t FileName[MAX_PATH] = L"";
	const std::filesystem::path SceneDir = FPaths::SceneDir();

	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(OPENFILENAMEW);
	Ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
	Ofn.lpstrFile = FileName;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrDefExt = L"json";
	Ofn.lpstrInitialDir = SceneDir.c_str();

	if (Type == EFileDialogType::Save)
	{
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

		if (GetSaveFileNameW(&Ofn))
			return FPaths::FromWide(FileName);
	}
	else // Open
	{
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameW(&Ofn))
			return FPaths::FromWide(FileName);
	}

	return "";
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void FEditorUI::Initialize(FEditorEngine* InEngine)
{
	Engine = InEngine;
	Console.SetDebugState(&DebugState);

	Property.OnChanged = [this](const FVector& Loc, const FVector& Rot, const FVector& Scl)
		{
			if (!Engine)
			{
				return;
			}

			AActor* Selected = Engine->GetSelectedActor();
			if (!Selected)
			{
				return;
			}

			if (USceneComponent* Root = Selected->GetRootComponent())
			{
				FTransform Transform = Root->GetRelativeTransform();
				Transform.SetLocation(Loc);
				Transform.SetRotation(FRotator::MakeFromEuler(Rot));
				Transform.SetScale3D(Scl);
				Root->SetRelativeTransform(Transform);
			}
		};

	ContentBrowser.OnFileDoubleClickCallback = [this](const FString& FilePath)
		{
			if (Engine)
			{
				Engine->GetViewportClient()->HandleFileDoubleClick(FilePath);
			}
		};

	ContentBrowser.OnFileDragEnd = [this](const FString& DraggingFilePath, const FString& ReleaseDirectory)
		{
			if (ContentBrowser.IsHovered())
			{
				if (ContentBrowser.IsMouseOnDirectory())
				{
					std::filesystem::path Src = FPaths::ToPath(DraggingFilePath);
					std::filesystem::path DstDir = FPaths::ToPath(ReleaseDirectory);

					std::filesystem::path Dst = DstDir / Src.filename();

					std::error_code ec;

					if (std::filesystem::exists(Dst))
					{
						int Result = MessageBoxW(
							nullptr,
							L"A file with the same name already exists.\nDo you want to overwrite it?",
							L"Overwrite",
							MB_YESNO | MB_ICONWARNING
						);

						if (Result != IDYES)
						{
							return;
						}
						std::filesystem::remove(Dst, ec);
						if (ec)
						{
							MessageBoxW(nullptr, L"Delete Failed", L"Error", MB_OK | MB_ICONERROR);
							return;
						}
					}

					std::filesystem::rename(Src, Dst, ec);

					if (ec)
					{
						UE_LOG("Move Failed: %s", ec.message().c_str());
					}
					else
					{
						const FString SrcPath = FPaths::FromPath(Src);
						const FString DstPath = FPaths::FromPath(Dst);
						UE_LOG("Moved: %s -> %s", SrcPath.c_str(), DstPath.c_str());
					}
				}
			}
			else if (Engine && Engine->GetSlateApplication() && Engine->GetSlateApplication()->GetHoveredViewportId() != INVALID_VIEWPORT_ID)
			{
				UE_LOG("Drop On Viewport");
				if (Engine)
				{
					Engine->GetViewportClient()->HandleFileDropOnViewport(DraggingFilePath);
				}
			}
		};
}

void FEditorUI::InitializeRendererResources(FRenderer* InRenderer)
{
	if (!Engine || !InRenderer)
	{
		return;
	}

	bViewportClientActive = true;

	ContentBrowser.SetFolderIcon(InRenderer->GetFolderIconSRV());
	ContentBrowser.SetFileIcon(InRenderer->GetFileIconSRV());
	InitializeImGui(InRenderer);
}

bool FEditorUI::InitializeImGui(FRenderer* InRenderer)
{
	if (!InRenderer)
	{
		return false;
	}

	if (bImGuiInitialized)
	{
		return true;
	}

	const HWND Hwnd = InRenderer->GetHwnd();
	ID3D11Device* Device = InRenderer->GetDevice();
	ID3D11DeviceContext* DeviceContext = InRenderer->GetDeviceContext();
	if (!Hwnd || !Device || !DeviceContext)
	{
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	IO.IniFilename = "imgui_editor.ini";

	std::filesystem::path FontPath = FPaths::ProjectRoot() / "Content" / "Fonts" / "NotoSansKR-Bold.ttf";
	std::wstring FontPathWString = FontPath.wstring();

	ImFontConfig FontConfig;
	FontConfig.OversampleH = 1;
	FontConfig.OversampleV = 1;
	FontConfig.PixelSnapH = true;

	ImFont* Font = nullptr;
	FILE* FileHandle = nullptr;
	_wfopen_s(&FileHandle, FontPath.c_str(), L"rb");
	if (FileHandle)
	{
		fseek(FileHandle, 0, SEEK_END);
		const size_t FontByteSize = static_cast<size_t>(ftell(FileHandle));
		fseek(FileHandle, 0, SEEK_SET);
		void* FontData = IM_ALLOC(FontByteSize);
		if (FontData)
		{
			fread(FontData, 1, FontByteSize, FileHandle);
			Font = IO.Fonts->AddFontFromMemoryTTF(
				FontData,
				static_cast<int32>(FontByteSize),
				16.0f,
				&FontConfig,
				IO.Fonts->GetGlyphRangesKorean());
		}
		fclose(FileHandle);
	}

	if (!Font)
	{
		MessageBoxW(nullptr, FontPathWString.c_str(), L"Failed to load font", MB_OK);
		IO.Fonts->AddFontDefault();
	}

	ImGui::StyleColorsDark();
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.WindowPadding = ImVec2(0, 0);
	Style.DisplayWindowPadding = ImVec2(0, 0);
	Style.DisplaySafeAreaPadding = ImVec2(0, 0);
	Style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	Style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
	if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		Style.WindowRounding = 0.0f;
		Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(Hwnd);
	ImGui_ImplDX11_Init(Device, DeviceContext);
	bImGuiInitialized = true;
	return true;
}

void FEditorUI::ShutdownImGui()
{
	if (!bImGuiInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	if (ImGui::GetCurrentContext())
	{
		ImGui::DestroyContext();
	}
	bImGuiInitialized = false;
}

void FEditorUI::BeginFrame()
{
	if (!bViewportClientActive || !bImGuiInitialized)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void FEditorUI::EndFrame()
{
	if (!bViewportClientActive || !bImGuiInitialized)
	{
		return;
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& IO = ImGui::GetIO();
	if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void FEditorUI::OnSlateReady()
{
	if (!Engine)
	{
		return;
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	if (!Slate)
	{
		return;
	}

	Slate->OnSplitterDragEnd = [this]() { SaveEditorSettings(); };
	LoadEditorSettings();

	FViewportId PreferredViewportId = INVALID_VIEWPORT_ID;
	if (Engine)
	{
		for (const FViewportEntry& Entry : Engine->GetViewportRegistry().GetEntries())
		{
			if (!Entry.bActive)
			{
				continue;
			}

			if (Entry.LocalState.ProjectionType == EViewportType::Perspective)
			{
				PreferredViewportId = Entry.Id;
				break;
			}

			if (PreferredViewportId == INVALID_VIEWPORT_ID)
			{
				PreferredViewportId = Entry.Id;
			}
		}
	}

	Slate->FocusViewport(PreferredViewportId);
	bRequestViewportFocusOnNextRender = true;
}

void FEditorUI::ShutdownRendererResources(FRenderer* InRenderer)
{
	bViewportClientActive = false;
	(void)InRenderer;

	ShutdownImGui();
}

void FEditorUI::SetupWindow(FWindowsWindow* InWindow)
{
	MainWindow = InWindow;
	if (bWindowSetup || MainWindow == nullptr)
	{
		return;
	}

	bWindowSetup = true;

	MainWindow->AddMessageFilter([this](HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) -> bool
		{
			if (!bViewportClientActive)
			{
				return false;
			}

			const bool bIsImeMessage =
				Msg == WM_IME_STARTCOMPOSITION ||
				Msg == WM_IME_COMPOSITION ||
				Msg == WM_IME_ENDCOMPOSITION ||
				Msg == WM_IME_NOTIFY ||
				Msg == WM_IME_SETCONTEXT ||
				Msg == WM_IME_CHAR;

			const bool bIsCharMessage =
				Msg == WM_CHAR ||
				Msg == WM_SYSCHAR ||
				Msg == WM_UNICHAR;

			if (bIsImeMessage || bIsCharMessage)
			{
				if (ImGui::GetCurrentContext())
				{
					const ImGuiIO& IO = ImGui::GetIO();
					if (!IO.WantTextInput)
					{
						return true;
					}
				}
				else
				{
					return true;
				}
			}

			const bool bHandledByImGui = ImGui_ImplWin32_WndProcHandler(Hwnd, Msg, WParam, LParam) != 0;

			FSlateApplication* Slate = Engine->GetSlateApplication();
			if (Slate && (Slate->GetMouseCapturedViewportId() != INVALID_VIEWPORT_ID || Slate->IsDraggingSplitter()))
			{
				return false;
			}

			return bHandledByImGui;
		});
}

void FEditorUI::BuildDefaultLayout(uint32 DockID)
{
	ImGui::DockBuilderRemoveNode(DockID);
	ImGui::DockBuilderAddNode(DockID, ImGuiDockNodeFlags_DockSpace);

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderSetNodeSize(DockID, Viewport->WorkSize);

	ImGuiID DockBottom = 0;
	ImGuiID DockUpper = 0;
	ImGui::DockBuilderSplitNode(DockID, ImGuiDir_Down, 0.25f, &DockBottom, &DockUpper);

	ImGuiID DockLeft = 0;
	ImGuiID DockCenter = 0;
	ImGui::DockBuilderSplitNode(DockUpper, ImGuiDir_Left, 0.20f, &DockLeft, &DockCenter);

	ImGuiID DockRight = 0;
	ImGui::DockBuilderSplitNode(DockCenter, ImGuiDir_Right, 0.25f, &DockRight, &DockCenter);

	ImGuiID DockRightTop = 0;
	ImGuiID DockRightBottom = 0;
	ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Up, 0.50f, &DockRightTop, &DockRightBottom);
	ImGui::DockBuilderDockWindow("Viewport", DockCenter);
	ImGui::DockBuilderDockWindow("Viewport", DockCenter);
	ImGui::DockBuilderDockWindow("Stats", DockLeft);
	ImGui::DockBuilderDockWindow("Properties", DockRightTop);
	ImGui::DockBuilderDockWindow("Control Panel", DockRightBottom);
	ImGui::DockBuilderDockWindow("Console", DockBottom);

	ImGui::DockBuilderFinish(DockID);
}

void FEditorUI::LoadEditorSettings()
{
	std::wstring Path = GetEditorIniPathW();
	if (!Engine) return;
	FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();

	wchar_t Sec[32];
	wchar_t Buf[64];

	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		swprintf(Sec, 32, L"Viewport.%u", Entry.Id);
		FViewportLocalState& S = Entry.LocalState;

		GetPrivateProfileStringW(Sec, L"GridSize", L"10.0", Buf, 64, Path.c_str());
		S.GridSize = static_cast<float>(_wtof(Buf));

		GetPrivateProfileStringW(Sec, L"LineThickness", L"1.0", Buf, 64, Path.c_str());
		S.LineThickness = static_cast<float>(_wtof(Buf));

		GetPrivateProfileStringW(Sec, L"ShowGrid", L"1", Buf, 64, Path.c_str());
		S.bShowGrid = (_wtoi(Buf) != 0);

		GetPrivateProfileStringW(Sec, L"ViewMode", L"0", Buf, 64, Path.c_str());
		S.ViewMode = static_cast<ERenderMode>(_wtoi(Buf));

		GetPrivateProfileStringW(Sec, L"SF.Primitives", L"1", Buf, 64, Path.c_str());
		S.ShowFlags.SetFlag(EEngineShowFlags::SF_Primitives, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(Sec, L"SF.UUID", L"1", Buf, 64, Path.c_str());
		S.ShowFlags.SetFlag(EEngineShowFlags::SF_UUID, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(Sec, L"SF.DebugDraw", L"0", Buf, 64, Path.c_str());
		S.ShowFlags.SetFlag(EEngineShowFlags::SF_DebugDraw, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(Sec, L"SF.WorldAxis", L"0", Buf, 64, Path.c_str());
		S.ShowFlags.SetFlag(EEngineShowFlags::SF_WorldAxis, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(Sec, L"SF.Collision", L"0", Buf, 64, Path.c_str());
		S.ShowFlags.SetFlag(EEngineShowFlags::SF_Collision, _wtoi(Buf) != 0);
	}

	bool bAnyDebugDrawEnabled = false;
	bool bAnyCollisionEnabled = false;
	for (const FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		bAnyDebugDrawEnabled = bAnyDebugDrawEnabled || Entry.LocalState.ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw);
		bAnyCollisionEnabled = bAnyCollisionEnabled || Entry.LocalState.ShowFlags.HasFlag(EEngineShowFlags::SF_Collision);
	}

	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		Entry.LocalState.ShowFlags.SetFlag(EEngineShowFlags::SF_DebugDraw, bAnyDebugDrawEnabled);
		Entry.LocalState.ShowFlags.SetFlag(EEngineShowFlags::SF_Collision, bAnyCollisionEnabled);
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	if (Slate)
	{
		GetPrivateProfileStringW(L"Splitter", L"Layout", L"0", Buf, 64, Path.c_str());
		int32 LayoutValue = _wtoi(Buf);
		if (LayoutValue < static_cast<int32>(EViewportLayout::Single) ||
			LayoutValue > static_cast<int32>(EViewportLayout::FourGrid))
		{
			LayoutValue = static_cast<int32>(EViewportLayout::Single);
		}

		Slate->SetLayout(static_cast<EViewportLayout>(LayoutValue));
		const int32 ActiveViewportCount = Slate->GetActiveViewportCount();
		int32 EntryIndex = 0;
		for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
		{
			Entry.bActive = (EntryIndex < ActiveViewportCount);
			++EntryIndex;
		}

		for (int i = 0; i < 3; ++i)
		{
			swprintf(Sec, 32, L"Splitter%d", i);
			GetPrivateProfileStringW(Sec, L"Ratio", L"0.5", Buf, 64, Path.c_str());

			float Ratio = static_cast<float>(_wtof(Buf));
			if (Ratio < 0.05f)
			{
				Ratio = 0.05f;
			}
			else if (Ratio > 0.95f)
			{
				Ratio = 0.95f;
			}
			Slate->SetSplitterRatio(i, Ratio);
		}

		Slate->PerformLayout();
	}
}

void FEditorUI::SaveEditorSettings()
{
	std::wstring Path = GetEditorIniPathW();
	if (!Engine) return;
	const FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();

	wchar_t Sec[32];
	wchar_t Buf[64];

	for (const FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		swprintf(Sec, 32, L"Viewport.%u", Entry.Id);
		const FViewportLocalState& S = Entry.LocalState;

		swprintf(Buf, 64, L"%.2f", S.GridSize);
		WritePrivateProfileStringW(Sec, L"GridSize", Buf, Path.c_str());

		swprintf(Buf, 64, L"%.2f", S.LineThickness);
		WritePrivateProfileStringW(Sec, L"LineThickness", Buf, Path.c_str());

		WritePrivateProfileStringW(Sec, L"ShowGrid",
			S.bShowGrid ? L"1" : L"0", Path.c_str());
		WritePrivateProfileStringW(Sec, L"ViewMode",
			std::to_wstring(static_cast<int>(S.ViewMode)).c_str(), Path.c_str());

		WritePrivateProfileStringW(Sec, L"SF.Primitives",
			S.ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives) ? L"1" : L"0", Path.c_str());
		WritePrivateProfileStringW(Sec, L"SF.UUID",
			S.ShowFlags.HasFlag(EEngineShowFlags::SF_UUID) ? L"1" : L"0", Path.c_str());
		WritePrivateProfileStringW(Sec, L"SF.DebugDraw",
			S.ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw) ? L"1" : L"0", Path.c_str());
		WritePrivateProfileStringW(Sec, L"SF.WorldAxis",
			S.ShowFlags.HasFlag(EEngineShowFlags::SF_WorldAxis) ? L"1" : L"0", Path.c_str());
		WritePrivateProfileStringW(Sec, L"SF.Collision",
			S.ShowFlags.HasFlag(EEngineShowFlags::SF_Collision) ? L"1" : L"0", Path.c_str());
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();

	if (Slate)
	{
		WritePrivateProfileStringW(L"Splitter", L"Layout",
			std::to_wstring(static_cast<int>(Slate->GetCurrentLayout())).c_str(), Path.c_str());
		for (int i = 0; i < 3; i++)
		{
			swprintf(Sec, 32, L"Splitter%d", i);
			swprintf(Buf, 64, L"%.4f", Slate->GetSplitterRatio(i));
			WritePrivateProfileStringW(Sec, L"Ratio", Buf, Path.c_str());
		}
	}
}

std::wstring FEditorUI::GetEditorIniPathW() const
{
	return (FPaths::ProjectRoot() / "Editor.ini").wstring();
}


void FEditorUI::Render()
{
	static bool bOpenAboutPopup = false;

	if (!bViewportClientActive)
	{
		return;
	}

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->WorkPos);
	ImGui::SetNextWindowSize(MainViewport->WorkSize);
	ImGui::SetNextWindowViewport(MainViewport->ID);

	ImGuiWindowFlags HostFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##DockSpaceHost", nullptr, HostFlags);
	ImGui::PopStyleVar(3);
	ImGuiID DockID = ImGui::GetID("MainDockSpace");

	if (!bLayoutInitialized)
	{
		bLayoutInitialized = true;

		ImGuiDockNode* Node = ImGui::DockBuilderGetNode(DockID);
		if (!Node || Node->IsEmpty())
		{
			BuildDefaultLayout(DockID);
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::DockSpace(DockID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

	ImGuiDockNode* CentralNode = ImGui::DockBuilderGetCentralNode(DockID);
	if (CentralNode && CentralNode->Size.x > 0 && CentralNode->Size.y > 0)
	{
		ImGuiViewport* MainVP = ImGui::GetMainViewport();
		const float WinX = MainVP ? MainVP->Pos.x : 0.0f;
		const float WinY = MainVP ? MainVP->Pos.y : 0.0f;

		CentralDockRect.X      = static_cast<int32>(CentralNode->Pos.x - WinX);
		CentralDockRect.Y      = static_cast<int32>(CentralNode->Pos.y - WinY);
		CentralDockRect.Width  = static_cast<int32>(CentralNode->Size.x);
		CentralDockRect.Height = static_cast<int32>(CentralNode->Size.y);
		bHasCentralDockRect = true;
	}

	ImGui::PopStyleVar();
	ImGui::End();

	if (Engine)
	{
		const FWorldContext* ActiveWorldContext = Engine->GetActiveWorldContext();
		AActor* Selected = Engine->GetSelectedActor();
		if (Selected != CachedSelectedActor || ActiveWorldContext != CachedActiveWorldContext)
		{
			SyncSelectedActorProperty();
		}
	}

	Stat.SetObjectCount(UObject::TotalAllocationCounts);
	Stat.SetHeapUsage(UObject::TotalAllocationBytes);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene"))
			{
				if (Engine && Engine->GetEditorScene())
				{
					Engine->SetSelectedActor(nullptr);

					if (UCameraComponent* Cam = Engine->GetEditorWorld() ? Engine->GetEditorWorld()->GetActiveCameraComponent() : nullptr)
					{
						Cam->GetCamera()->SetPosition({ -5.0f, 0.0f, 2.0f });
						Cam->GetCamera()->SetRotation(0.f, 0.f);
					}
					Engine->GetEditorScene()->ClearActors();
					UE_LOG("New scene created");
				}
			}

			if (ImGui::MenuItem("Open Scene"))
			{
				if (Engine && Engine->GetEditorScene())
				{
					FString Path = GetFilePathUsingDialog(EFileDialogType::Open);

					if (!Path.empty())
					{
						Engine->SetSelectedActor(nullptr);
						Engine->GetEditorScene()->ClearActors();

						FCameraSerializeData CameraData;
						bool bLoaded = FSceneSerializer::Load(Engine->GetEditorScene(), Path,
						                                      Engine->GetRenderer()->GetDevice(), &CameraData);
						if (bLoaded)
						{
							if (CameraData.bValid)
							{
								FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();
								FViewportEntry* PerspEntry = nullptr;
								if (FSlateApplication* Slate = Engine->GetSlateApplication())
								{
									const FViewportId FocusedId = Slate->GetFocusedViewportId();
									if (FocusedId != INVALID_VIEWPORT_ID)
									{
									FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
									if (FocusedEntry &&
										FocusedEntry->bActive &&
										FocusedEntry->WorldContext &&
										FocusedEntry->WorldContext->WorldType == EWorldType::Editor &&
										FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
									{
										PerspEntry = FocusedEntry;
										}
									}
								}
							if (!PerspEntry)
							{
								for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
								{
									if (Entry.bActive &&
										Entry.WorldContext &&
										Entry.WorldContext->WorldType == EWorldType::Editor &&
										Entry.LocalState.ProjectionType == EViewportType::Perspective)
									{
										PerspEntry = &Entry;
										break;
									}
								}
							}
								if (PerspEntry)
								{
									PerspEntry->LocalState.Position  = CameraData.Location;
									PerspEntry->LocalState.Rotation  = CameraData.Rotation;
									PerspEntry->LocalState.FovY      = CameraData.FOV;
									PerspEntry->LocalState.NearPlane = CameraData.NearClip;
									PerspEntry->LocalState.FarPlane  = CameraData.FarClip;
								}
							}
							UE_LOG("Scene loaded: %s", Path.c_str());
						}
						else
						{
							MessageBoxW(
								nullptr,
								L"Failed to load the selected scene file.",
								L"Error",
								MB_OK | MB_ICONWARNING
							);
						}
					}
				}
			}

			if (ImGui::MenuItem("Save Scene As..."))
			{
				if (Engine && Engine->GetEditorScene())
				{
					FString Path = GetFilePathUsingDialog(EFileDialogType::Save);

					if (!Path.empty())
					{
						FCameraSerializeData CameraData;
						const FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();
						const FViewportEntry* PerspEntry = nullptr;
						if (const FSlateApplication* Slate = Engine->GetSlateApplication())
						{
							const FViewportId FocusedId = Slate->GetFocusedViewportId();
							if (FocusedId != INVALID_VIEWPORT_ID)
							{
							const FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
							if (FocusedEntry &&
								FocusedEntry->bActive &&
								FocusedEntry->WorldContext &&
								FocusedEntry->WorldContext->WorldType == EWorldType::Editor &&
								FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
							{
								PerspEntry = FocusedEntry;
								}
							}
						}
						if (!PerspEntry)
						{
							for (const FViewportEntry& Entry : ViewportRegistry.GetEntries())
							{
								if (Entry.bActive &&
									Entry.WorldContext &&
									Entry.WorldContext->WorldType == EWorldType::Editor &&
									Entry.LocalState.ProjectionType == EViewportType::Perspective)
								{
									PerspEntry = &Entry;
									break;
								}
							}
						}
						if (PerspEntry)
						{
							CameraData.Location  = PerspEntry->LocalState.Position;
							CameraData.Rotation  = PerspEntry->LocalState.Rotation;
							CameraData.FOV       = PerspEntry->LocalState.FovY;
							CameraData.NearClip  = PerspEntry->LocalState.NearPlane;
							CameraData.FarClip   = PerspEntry->LocalState.FarPlane;
							CameraData.bValid    = true;
						}
						FSceneSerializer::Save(Engine->GetEditorScene(), Path, CameraData);
					}
				}
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (Engine)
			{
				FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();
				if (!ViewportRegistry.GetEntries().empty())
				{
					FViewportEntry* TargetEntry = nullptr;
					FSlateApplication* Slate = Engine->GetSlateApplication();
					FViewportId ViewportID = Slate ? Slate->GetFocusedViewportId() : INVALID_VIEWPORT_ID;

					if (ViewportID == INVALID_VIEWPORT_ID)
						TargetEntry = &ViewportRegistry.GetEntries().front();
					else
						TargetEntry = ViewportRegistry.FindEntryByViewportID(ViewportID);

					if (!TargetEntry)
						TargetEntry = &ViewportRegistry.GetEntries().front();

				FShowFlags& ShowFlags = TargetEntry->LocalState.ShowFlags;
				ImGui::SeparatorText("Show Flags");
				auto ShowFlagCheckbox = [&](const char* Label, EEngineShowFlags Flag)
				{
					bool bValue = ShowFlags.HasFlag(Flag);
					if (ImGui::Checkbox(Label, &bValue))
					{
						if (Flag == EEngineShowFlags::SF_DebugDraw || Flag == EEngineShowFlags::SF_Collision)
						{
							for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
							{
								Entry.LocalState.ShowFlags.SetFlag(Flag, bValue);
							}
						}
						else
						{
							ShowFlags.SetFlag(Flag, bValue);
						}
						SaveEditorSettings();
					}
				};

				ShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives);
				ShowFlagCheckbox("UUID", EEngineShowFlags::SF_UUID);
				ShowFlagCheckbox("Debug Draw", EEngineShowFlags::SF_DebugDraw);
				ShowFlagCheckbox("Collision", EEngineShowFlags::SF_Collision);
				ImGui::SeparatorText("Grid");
				bool bShowGrid = TargetEntry->LocalState.bShowGrid;
				if (ImGui::Checkbox("Show Grid", &bShowGrid))
				{
					TargetEntry->LocalState.bShowGrid = bShowGrid;
					SaveEditorSettings();
				}
				if (ImGui::SliderFloat("Grid Size", &TargetEntry->LocalState.GridSize, 1.0f, 100.0f, "%.1f"))
				{
					SaveEditorSettings();
				}

				if (ImGui::SliderFloat("Line Thickness", &TargetEntry->LocalState.LineThickness, 0.1f, 5.0f, "%.2f"))
				{
					SaveEditorSettings();
				}
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
			{
				bOpenAboutPopup = true;
			}
			ImGui::EndMenu();
		}

		if (Engine)
		{
			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();

			const bool bIsPIEActive = Engine->IsPIEActive();
			const bool bIsPIEPaused = Engine->IsPIEPaused();
			const char* PIEPrimaryLabel = !bIsPIEActive
				? "▶ Start"
				: (bIsPIEPaused ? "▶ Resume" : "▷ Pause");

			if (ImGui::Button(PIEPrimaryLabel))
			{
				if (!bIsPIEActive)
				{
					Engine->StartPIE();
				}
				else
				{
					Engine->TogglePIEPause();
				}
			}

			ImGui::SameLine();
			if (!bIsPIEActive)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("■ Stop") && bIsPIEActive)
			{
				Engine->EndPIE();
			}
			if (!bIsPIEActive)
			{
				ImGui::EndDisabled();
			}
		}
		ImGui::EndMainMenuBar();
	}

	if (bOpenAboutPopup)
	{
		ImGui::OpenPopup("AboutPopup"); 
		ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_Always);
		bOpenAboutPopup = false;
	}

	if (ImGui::BeginPopupModal("AboutPopup", nullptr, ImGuiWindowFlags_NoTitleBar))
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImVec2 WinPos = ImGui::GetWindowPos();
		ImVec2 WinSize = ImGui::GetWindowSize();
		DrawList->AddRectFilled(WinPos, ImVec2(WinPos.x + WinSize.x, WinPos.y + 60), IM_COL32(30, 30, 60, 255));

		ImGui::SetCursorPosY(12);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("Meteor Engine").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Meteor Engine");

		ImGui::SetCursorPosY(35);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("v2.0.0").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "v2.0.0");

		ImGui::SetCursorPosY(70);
		ImGui::SetCursorPosX(20);
		ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "First Contributors (Dino Engine)");
		ImGui::SameLine();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9f, 0.7f, 0.3f, 0.5f));
		ImGui::Separator();
		ImGui::PopStyleColor();

		ImGui::Spacing();

		const char* First_Contributors[] = { "Kim Jiwoo", "Kim Sihyun", "Park Seyoon", "Cho Sanghyuk" };
		for (const char* Name : First_Contributors)
		{
			ImGui::SetCursorPosX(20);
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.6f, 1.0f), "-");
			ImGui::SameLine();
			ImGui::Text("%s", Name);
		}

		ImGui::SetCursorPosX(20);

		ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "Second Contributors");
		ImGui::SameLine();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9f, 0.7f, 0.3f, 0.5f));
		ImGui::Separator();
		ImGui::PopStyleColor();

		ImGui::Spacing();

		const char* Second_Contributors[] = { "Kang Myeongjun", "Lee Sujin", "Jung Gyuho" };
		for (const char* Name : Second_Contributors)
		{
			ImGui::SetCursorPosX(20);
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.6f, 1.0f), "-");
			ImGui::SameLine();
			ImGui::Text("%s", Name);
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1, 1, 1, 0.1f));
		ImGui::Separator();
		ImGui::PopStyleColor();
		ImGui::Spacing();

		ImGui::SetCursorPosX(20);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Copyright (c) 2026  |  MIT License");

		ImGui::Spacing();
		ImGui::Spacing();

		float ButtonWidth = 100.0f;
		ImGui::SetCursorPosX((WinSize.x - ButtonWidth) * 0.5f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		if (ImGui::Button("Close", ImVec2(ButtonWidth, 28)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		ImGui::Spacing();
		ImGui::EndPopup();
	}

	ControlPanel.Render(Engine);
	Property.Render(Engine);
	Console.Render();
	if (DebugState.Memory)
	{
		FRect StatArea;
		if (!GetCentralDockRect(StatArea))
		{
			StatArea = { 0, 0, 0, 0 };
		}
		Stat.Render(StatArea);
	}
	Outliner.Render(Engine);
	ContentBrowser.Render();

	if (bRequestViewportFocusOnNextRender)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetWindowFocus(nullptr);
			ImGui::ClearActiveID();
		}
		bRequestViewportFocusOnNextRender = false;
	}
}

bool FEditorUI::GetViewportMousePosition(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const
{
	if (!Engine)
	{
		return false;
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	if (!Slate)
	{
		return false;
	}

	const FViewportId HoveredViewportId = Slate->GetHoveredViewportId();
	if (HoveredViewportId == INVALID_VIEWPORT_ID)
	{
		return false;
	}

	const FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();
	const FViewportEntry* Entry = ViewportRegistry.FindEntryByViewportID(HoveredViewportId);
	if (!Entry || !Entry->bActive || !Entry->Viewport)
	{
		return false;
	}

	const FRect& Rect = Entry->Viewport->GetRect();
	if (!Rect.IsValid())
	{
		return false;
	}

	OutViewportX = WindowMouseX - Rect.X;
	OutViewportY = WindowMouseY - Rect.Y;
	OutWidth = Rect.Width;
	OutHeight = Rect.Height;
	return true;
}

void FEditorUI::SyncSelectedActorProperty()
{
	if (!Engine)
	{
		return;
	}

	const FWorldContext* ActiveWorldContext = Engine->GetActiveWorldContext();
	AActor* Selected = Engine->GetSelectedActor();
	if (Selected)
	{
		UWorld* SelectedWorld = Selected->GetWorld();
		UWorld* ActiveWorld = ActiveWorldContext ? ActiveWorldContext->World : nullptr;
		if (SelectedWorld != ActiveWorld)
		{
			Selected = nullptr;
		}
	}
	if (Selected)
	{
		if (USceneComponent* Root = Selected->GetRootComponent())
		{
			const FTransform Transform = Root->GetRelativeTransform();
			Property.SetTarget(
				Transform.GetLocation(),
				Transform.Rotator().Euler(),
				Transform.GetScale3D(),
				Selected->GetName().c_str()
			);
		}
	}
	else
	{
		Property.SetTarget({ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, "None");
	}

	CachedSelectedActor = Selected;
	CachedActiveWorldContext = ActiveWorldContext;
}

bool FEditorUI::GetCentralDockRect(FRect& OutRect) const
{
	if (!bHasCentralDockRect)
	{
		return false;
	}
	OutRect = CentralDockRect;
	return true;
}
