#include "ObjViewerShell.h"

#include <algorithm>
#include <commdlg.h>
#include <cstdio>
#include <filesystem>

#include "ObjViewerEngine.h"

#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Renderer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "Comdlg32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
	const char* GetAxisLabel(EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::PosX: return "+X";
		case EObjImportAxis::NegX: return "-X";
		case EObjImportAxis::PosY: return "+Y";
		case EObjImportAxis::NegY: return "-Y";
		case EObjImportAxis::PosZ: return "+Z";
		case EObjImportAxis::NegZ: return "-Z";
		default: return "+X";
		}
	}

	const char* GetPresetLabel(EObjImportPreset Preset)
	{
		switch (Preset)
		{
		case EObjImportPreset::Auto: return "Auto";
		case EObjImportPreset::Custom: return "Custom";
		case EObjImportPreset::Blender: return "Blender";
		case EObjImportPreset::Maya: return "Maya";
		case EObjImportPreset::ThreeDSMax: return "3ds Max";
		case EObjImportPreset::Unreal: return "Unreal";
		case EObjImportPreset::Unity: return "Unity";
		default: return "Auto";
		}
	}

	EAxis GetAxisBase(EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::PosX:
		case EObjImportAxis::NegX:
			return EAxis::X;
		case EObjImportAxis::PosY:
		case EObjImportAxis::NegY:
			return EAxis::Y;
		case EObjImportAxis::PosZ:
		case EObjImportAxis::NegZ:
			return EAxis::Z;
		default:
			return EAxis::X;
		}
	}

	void ApplyImportPreset(FObjImportSummary& ImportOptions)
	{
		switch (ImportOptions.SourcePreset)
		{
		case EObjImportPreset::Auto:
			ImportOptions.ForwardAxis = EObjImportAxis::PosX;
			ImportOptions.UpAxis = EObjImportAxis::PosZ;
			break;
		case EObjImportPreset::Unreal:
			ImportOptions.ForwardAxis = EObjImportAxis::PosX;
			ImportOptions.UpAxis = EObjImportAxis::PosZ;
			break;
		case EObjImportPreset::Blender:
			ImportOptions.ForwardAxis = EObjImportAxis::PosY;
			ImportOptions.UpAxis = EObjImportAxis::PosZ;
			break;
		case EObjImportPreset::Maya:
			ImportOptions.ForwardAxis = EObjImportAxis::PosZ;
			ImportOptions.UpAxis = EObjImportAxis::PosY;
			break;
		case EObjImportPreset::ThreeDSMax:
			ImportOptions.ForwardAxis = EObjImportAxis::PosY;
			ImportOptions.UpAxis = EObjImportAxis::PosZ;
			break;
		case EObjImportPreset::Unity:
			ImportOptions.ForwardAxis = EObjImportAxis::PosZ;
			ImportOptions.UpAxis = EObjImportAxis::PosY;
			break;
		case EObjImportPreset::Custom:
		default:
			break;
		}
	}

	bool DrawAxisCombo(const char* Label, EObjImportAxis& Axis)
	{
		constexpr EObjImportAxis AxisValues[] =
		{
			EObjImportAxis::PosX,
			EObjImportAxis::NegX,
			EObjImportAxis::PosY,
			EObjImportAxis::NegY,
			EObjImportAxis::PosZ,
			EObjImportAxis::NegZ
		};

		bool bChanged = false;
		if (ImGui::BeginCombo(Label, GetAxisLabel(Axis)))
		{
			for (EObjImportAxis Candidate : AxisValues)
			{
				const bool bSelected = Candidate == Axis;
				if (ImGui::Selectable(GetAxisLabel(Candidate), bSelected))
				{
					Axis = Candidate;
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

	bool DrawPresetCombo(FObjImportSummary& ImportOptions)
	{
		constexpr EObjImportPreset PresetValues[] =
		{
			EObjImportPreset::Auto,
			EObjImportPreset::Custom,
			EObjImportPreset::Blender,
			EObjImportPreset::Maya,
			EObjImportPreset::ThreeDSMax,
			EObjImportPreset::Unreal,
			EObjImportPreset::Unity
		};

		bool bChanged = false;
		if (ImGui::BeginCombo("Source Preset", GetPresetLabel(ImportOptions.SourcePreset)))
		{
			for (EObjImportPreset Candidate : PresetValues)
			{
				const bool bSelected = Candidate == ImportOptions.SourcePreset;
				if (ImGui::Selectable(GetPresetLabel(Candidate), bSelected))
				{
					ImportOptions.SourcePreset = Candidate;
					ApplyImportPreset(ImportOptions);
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
	bool IsViewportMouseMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			return true;
		default:
			return false;
		}
	}

	bool IsViewportKeyboardMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
			return true;
		default:
			return false;
		}
	}

	bool IsImeMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_IME_STARTCOMPOSITION:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_IME_NOTIFY:
		case WM_IME_SETCONTEXT:
		case WM_IME_REQUEST:
		case WM_INPUTLANGCHANGE:
			return true;
		default:
			return false;
		}
	}

	FString WideToUtf8(const std::wstring& WideString)
	{
		if (WideString.empty())
		{
			return "";
		}

		const int32 RequiredBytes = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (RequiredBytes <= 1)
		{
			return "";
		}

		FString Result;
		Result.resize(static_cast<size_t>(RequiredBytes));
		::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			Result.data(),
			RequiredBytes,
			nullptr,
			nullptr);
		Result.pop_back();
		return Result;
	}

	FString OpenObjFileDialog(HWND OwnerWindow)
	{
		wchar_t FileBuffer[MAX_PATH] = {};

		OPENFILENAMEW Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEW);
		Ofn.hwndOwner = OwnerWindow;
		Ofn.lpstrFile = FileBuffer;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrFilter = L"OBJ Files\0*.obj\0All Files\0*.*\0";
		Ofn.nFilterIndex = 1;
		Ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!::GetOpenFileNameW(&Ofn))
		{
			return "";
		}

		return WideToUtf8(FileBuffer);
	}

	FString GetDefaultModelExportPath(const FObjViewerEngine* Engine)
	{
		FString BaseName = "ExportedMesh";
		if (Engine && Engine->HasLoadedModel())
		{
			const std::filesystem::path SourcePath(FPaths::ToWide(Engine->GetModelState().SourceFilePath));
			const FString SourceStem = WideToUtf8(SourcePath.stem().wstring());
			if (!SourceStem.empty())
			{
				BaseName = SourceStem;
			}
		}

		std::filesystem::path ExportPath = FPaths::MeshDir() / std::filesystem::path(FPaths::ToWide(BaseName));
		ExportPath.replace_extension(L".Model");
		return WideToUtf8(ExportPath.wstring());
	}

	FString SaveModelFileDialog(HWND OwnerWindow, const FString& DefaultPath)
	{
		wchar_t FileBuffer[MAX_PATH] = {};
		wchar_t InitialDirBuffer[MAX_PATH] = {};

		const std::filesystem::path DefaultPathFs = std::filesystem::path(FPaths::ToWide(DefaultPath)).lexically_normal();
		const std::wstring DefaultFileName = DefaultPathFs.filename().wstring();
		const std::wstring InitialDir = DefaultPathFs.has_parent_path()
			? DefaultPathFs.parent_path().wstring()
			: std::wstring();

		wcsncpy_s(FileBuffer, DefaultFileName.c_str(), _TRUNCATE);
		if (!InitialDir.empty())
		{
			wcsncpy_s(InitialDirBuffer, InitialDir.c_str(), _TRUNCATE);
		}

		OPENFILENAMEW Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEW);
		Ofn.hwndOwner = OwnerWindow;
		Ofn.lpstrFile = FileBuffer;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrFilter = L"Model Files\0*.Model\0All Files\0*.*\0";
		Ofn.nFilterIndex = 1;
		Ofn.lpstrDefExt = L"Model";
		Ofn.lpstrInitialDir = InitialDir.empty() ? nullptr : InitialDirBuffer;
		Ofn.lpstrTitle = L"Export .Model";
		Ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		if (!::GetSaveFileNameW(&Ofn))
		{
			const DWORD DialogError = ::CommDlgExtendedError();
			if (DialogError != 0)
			{
				UE_LOG("[ObjViewer] GetSaveFileNameW failed. CommDlgExtendedError=%lu", static_cast<unsigned long>(DialogError));
			}
			return "";
		}

		return WideToUtf8(std::wstring(FileBuffer));
	}
}

void FObjViewerShell::Initialize(FObjViewerEngine* InEngine)
{
	Engine = InEngine;
}

void FObjViewerShell::RequestImportDialog(const FString& FilePath, const FString& ImportSource)
{
	if (FilePath.empty())
	{
		return;
	}

	PendingImportPath = FilePath;
	PendingImportOptions = Engine && Engine->HasLoadedModel()
		? Engine->GetModelState().LastImportSummary
		: FObjImportSummary{};
	PendingImportOptions.ImportSource = ImportSource;
	PendingImportOptions.bReplaceCurrentModel = true;
	PendingImportOptions.UniformScale = (std::max)(PendingImportOptions.UniformScale, 0.01f);
	if (!Engine || !Engine->HasLoadedModel())
	{
		ApplyImportPreset(PendingImportOptions);
	}
	bOpenImportDialogNextFrame = true;
}

void FObjViewerShell::SetupWindow(FWindowsWindow* InWindow)
{
	MainWindow = InWindow;
	if (bWindowSetup || MainWindow == nullptr)
	{
		return;
	}

	bWindowSetup = true;

	MainWindow->AddMessageFilter([this](HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) -> bool
	{
		if (!bAttached || !ImGui::GetCurrentContext())
		{
			return false;
		}

		ImGui_ImplWin32_WndProcHandler(Hwnd, Msg, WParam, LParam);

		switch (Msg)
		{
		case WM_RBUTTONDOWN:
			if (bViewportHovered)
			{
				bViewportCaptureActive = true;
			}
			break;
		case WM_RBUTTONUP:
		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
		case WM_KILLFOCUS:
			bViewportCaptureActive = false;
			break;
		case WM_ACTIVATEAPP:
			if (WParam == FALSE)
			{
				bViewportCaptureActive = false;
			}
			break;
		default:
			break;
		}

		const ImGuiIO& IO = ImGui::GetIO();
		const bool bIsMouseMessage = IsViewportMouseMessage(Msg);
		const bool bIsKeyboardMessage = IsViewportKeyboardMessage(Msg);
		const bool bIsImeMessage = IsImeMessage(Msg);

		if (bIsMouseMessage && WantsViewportMouseInput())
		{
			return false;
		}

		if (bIsKeyboardMessage && WantsViewportKeyboardInput() && !IO.WantTextInput)
		{
			return false;
		}

		if (bIsMouseMessage)
		{
			return IO.WantCaptureMouse;
		}

		if (bIsKeyboardMessage)
		{
			return IO.WantCaptureKeyboard || IO.WantTextInput;
		}

		if (bIsImeMessage)
		{
			return IO.WantTextInput;
		}

		return false;
	});
}

void FObjViewerShell::AttachToRenderer(FRenderer* InRenderer)
{
	if (Engine == nullptr || InRenderer == nullptr)
	{
		return;
	}

	bAttached = true;
	CurrentRenderer = InRenderer;

	const HWND Hwnd = InRenderer->GetHwnd();
	ID3D11Device* Device = InRenderer->GetDevice();
	ID3D11DeviceContext* DeviceContext = InRenderer->GetDeviceContext();

	std::filesystem::path FontPath = FPaths::ProjectRoot() / "Content" / "Fonts" / "NotoSansKR-Bold.ttf";
	std::wstring FontPathW = FontPath.wstring();

	InRenderer->SetGUICallbacks(
		[Hwnd, Device, DeviceContext, FontPath, FontPathW]()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			ImGuiIO& IO = ImGui::GetIO();
			IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			IO.IniFilename = "imgui_objviewer.ini";

			ImFont* Font = nullptr;
			FILE* File = nullptr;
			_wfopen_s(&File, FontPath.c_str(), L"rb");
			if (File)
			{
				fseek(File, 0, SEEK_END);
				size_t Size = ftell(File);
				fseek(File, 0, SEEK_SET);

				void* FontData = IM_ALLOC(Size);
				fread(FontData, 1, Size, File);
				fclose(File);

				ImFontConfig FontConfig;
				FontConfig.OversampleH = 1;
				FontConfig.OversampleV = 1;
				FontConfig.PixelSnapH = true;
				Font = IO.Fonts->AddFontFromMemoryTTF(
					FontData,
					static_cast<int>(Size),
					16.0f,
					&FontConfig,
					IO.Fonts->GetGlyphRangesKorean());
			}

			if (!Font)
			{
				MessageBoxW(nullptr, FontPathW.c_str(), L"Failed to load font", MB_OK);
				IO.Fonts->AddFontDefault();
			}

			ImGui::StyleColorsDark();

			ImGuiStyle& Style = ImGui::GetStyle();
			Style.WindowPadding = ImVec2(0, 0);
			Style.DisplayWindowPadding = ImVec2(0, 0);
			Style.DisplaySafeAreaPadding = ImVec2(0, 0);
			Style.WindowRounding = 0.0f;

			ImGui_ImplWin32_Init(Hwnd);
			ImGui_ImplDX11_Init(Device, DeviceContext);
		},
		[]()
		{
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		},
		[]()
		{
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
		},
		[]()
		{
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		});
}

void FObjViewerShell::DetachFromRenderer(FRenderer* InRenderer)
{
	bAttached = false;
	CurrentRenderer = nullptr;
	DesiredViewportWidth = 0;
	DesiredViewportHeight = 0;
	ViewportSurface.Release();

	if (InRenderer)
	{
		InRenderer->ClearSceneRenderTarget();
		InRenderer->ClearViewportCallbacks();
	}
}

void FObjViewerShell::PrepareViewportSurface(FRenderer* Renderer)
{
	if (Renderer == nullptr)
	{
		return;
	}

	ViewportSurface.SetSize(DesiredViewportWidth, DesiredViewportHeight);
	ViewportSurface.EnsureResources(Renderer->GetDevice());
}

void FObjViewerShell::Render()
{
	if (!bAttached)
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
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##ObjViewerDockHost", nullptr, HostFlags);
	ImGui::PopStyleVar(3);

	DrawMenuBar();

	const ImGuiID DockID = ImGui::GetID("ObjViewerDockSpace");
	if (!bLayoutInitialized)
	{
		bLayoutInitialized = true;
		BuildDefaultLayout(DockID);
	}

	ImGui::DockSpace(DockID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	DrawToolbarWindow();
	DrawViewportWindow();
	DetailsWindow.Render(Engine);
	DrawImportDialog();
}

void FObjViewerShell::BuildDefaultLayout(unsigned int DockID)
{
	ImGui::DockBuilderRemoveNode(DockID);
	ImGui::DockBuilderAddNode(DockID, ImGuiDockNodeFlags_DockSpace);

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderSetNodeSize(DockID, Viewport->WorkSize);

	ImGuiID MainDock = DockID;
	ImGuiID ToolbarDock = ImGui::DockBuilderSplitNode(MainDock, ImGuiDir_Up, 0.10f, nullptr, &MainDock);
	ImGuiID DetailsDock = ImGui::DockBuilderSplitNode(MainDock, ImGuiDir_Right, 0.30f, nullptr, &MainDock);

	ImGui::DockBuilderDockWindow("Toolbar", ToolbarDock);
	ImGui::DockBuilderDockWindow("Details", DetailsDock);
	ImGui::DockBuilderDockWindow("Viewport", MainDock);
	ImGui::DockBuilderFinish(DockID);
}

void FObjViewerShell::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar())
	{
		return;
	}

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Open OBJ...") && Engine)
		{
			const FString Path = OpenObjFileDialog(MainWindow ? MainWindow->GetHwnd() : nullptr);
			if (!Path.empty())
			{
				RequestImportDialog(Path, "Open Dialog");
			}
		}

		if (ImGui::MenuItem("Reload", nullptr, false, Engine && Engine->HasLoadedModel()))
		{
			Engine->ReloadLoadedModel();
		}

		if (ImGui::MenuItem("Export .Model...", nullptr, false, Engine && Engine->HasLoadedModel()))
		{
			const FString ExportPath = SaveModelFileDialog(MainWindow ? MainWindow->GetHwnd() : nullptr, GetDefaultModelExportPath(Engine));
			if (!ExportPath.empty())
			{
				Engine->ExportLoadedModelAsModel(ExportPath);
			}
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Frame Camera", nullptr, false, Engine && Engine->HasLoadedModel()))
		{
			Engine->FrameLoadedModel();
		}

		if (ImGui::MenuItem("Reset Camera"))
		{
			Engine->ResetViewerCamera();
		}

		if (Engine)
		{
			ImGui::Separator();
			bool bWireframeEnabled = Engine->IsWireframeEnabled();
			if (ImGui::MenuItem("Show Wireframe", nullptr, &bWireframeEnabled))
			{
				Engine->SetWireframeEnabled(bWireframeEnabled);
			}

			FObjViewerNormalSettings& NormalSettings = Engine->GetMutableNormalSettings();
			ImGui::MenuItem("Show Normals", nullptr, &NormalSettings.bVisible);

			FObjViewerGridSettings& GridSettings = Engine->GetMutableGridSettings();
			ImGui::MenuItem("Show Grid", nullptr, &GridSettings.bVisible);
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void FObjViewerShell::DrawToolbarWindow()
{
	if (!ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Open OBJ") && Engine)
	{
		const FString Path = OpenObjFileDialog(MainWindow ? MainWindow->GetHwnd() : nullptr);
		if (!Path.empty())
		{
			RequestImportDialog(Path, "Open Dialog");
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Reload") && Engine)
	{
		Engine->ReloadLoadedModel();
	}

	ImGui::SameLine();
	if (ImGui::Button("Export .Model") && Engine && Engine->HasLoadedModel())
	{
		const FString ExportPath = SaveModelFileDialog(MainWindow ? MainWindow->GetHwnd() : nullptr, GetDefaultModelExportPath(Engine));
		if (!ExportPath.empty())
		{
			Engine->ExportLoadedModelAsModel(ExportPath);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Frame Camera") && Engine)
	{
		Engine->FrameLoadedModel();
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Camera") && Engine)
	{
		Engine->ResetViewerCamera();
	}

	if (Engine)
	{
		ImGui::SameLine();
		bool bWireframeEnabled = Engine->IsWireframeEnabled();
		if (ImGui::Checkbox("Wireframe", &bWireframeEnabled))
		{
			Engine->SetWireframeEnabled(bWireframeEnabled);
		}

		ImGui::SameLine();
		FObjViewerNormalSettings& NormalSettings = Engine->GetMutableNormalSettings();
		ImGui::Checkbox("Normals", &NormalSettings.bVisible);
	}

	ImGui::End();
}

void FObjViewerShell::DrawViewportWindow()
{
	if (!ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		DesiredViewportWidth = 0;
		DesiredViewportHeight = 0;
		bViewportHovered = false;
		bViewportFocused = false;
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			bViewportCaptureActive = false;
		}
		ImGui::End();
		return;
	}

	bViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	bViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		bViewportCaptureActive = true;
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
	{
		bViewportCaptureActive = false;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	DesiredViewportWidth = (Available.x > 1.0f) ? static_cast<int32>(Available.x) : 0;
	DesiredViewportHeight = (Available.y > 1.0f) ? static_cast<int32>(Available.y) : 0;

	if (ViewportSurface.IsValid())
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(ViewportSurface.GetSRV()), Available);
	}
	else if (Engine && Engine->HasLoadedModel())
	{
		ImGui::Dummy(Available);
	}
	else
	{
		ImGui::Dummy(ImVec2(0.0f, 16.0f));
		ImGui::TextDisabled("Drop an OBJ file here or use File > Open OBJ.");
	}

	ImGui::PopStyleVar(2);
	ImGui::End();
}

void FObjViewerShell::DrawImportDialog()
{
	if (bOpenImportDialogNextFrame)
	{
		ImGui::OpenPopup("Import OBJ");
		bOpenImportDialogNextFrame = false;
	}

	ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Import OBJ", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	const std::filesystem::path ImportPath(FPaths::ToWide(PendingImportPath));
	const FString FileName = WideToUtf8(ImportPath.filename().wstring());

	ImGui::Text("File");
	ImGui::SameLine(110.0f);
	ImGui::TextWrapped("%s", FileName.empty() ? PendingImportPath.c_str() : FileName.c_str());

	ImGui::Text("Path");
	ImGui::SameLine(110.0f);
	ImGui::TextWrapped("%s", PendingImportPath.c_str());

	ImGui::Separator();
	ImGui::TextUnformatted("Import Settings");

	DrawPresetCombo(PendingImportOptions);

	ImGui::BeginDisabled();
	ImGui::Checkbox("Replace Current Model", &PendingImportOptions.bReplaceCurrentModel);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("(single-model viewer)");

	const bool bForwardChanged = DrawAxisCombo("Forward Axis", PendingImportOptions.ForwardAxis);
	const bool bUpChanged = DrawAxisCombo("Up Axis", PendingImportOptions.UpAxis);
	if (bForwardChanged || bUpChanged)
	{
		PendingImportOptions.SourcePreset = EObjImportPreset::Custom;
	}

	ImGui::Checkbox("Center To Origin", &PendingImportOptions.bCenterToOrigin);
	ImGui::Checkbox("Place On Ground", &PendingImportOptions.bPlaceOnGround);
	ImGui::Checkbox("Frame Camera After Import", &PendingImportOptions.bFrameCameraAfterImport);
	ImGui::DragFloat("Uniform Scale", &PendingImportOptions.UniformScale, 0.01f, 0.01f, 1000.0f, "%.2f");

	const bool bInvalidAxisPair = GetAxisBase(PendingImportOptions.ForwardAxis) == GetAxisBase(PendingImportOptions.UpAxis);
	if (bInvalidAxisPair)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Forward Axis and Up Axis must use different bases.");
	}

	ImGui::Separator();
	ImGui::BeginDisabled(bInvalidAxisPair);
	if (ImGui::Button("Import", ImVec2(120.0f, 0.0f)))
	{
		PendingImportOptions.bReplaceCurrentModel = true;
		if (Engine)
		{
			Engine->LoadModelFromFile(PendingImportPath, PendingImportOptions);
		}
		ResetPendingImportState();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
	{
		ResetPendingImportState();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void FObjViewerShell::ResetPendingImportState()
{
	PendingImportPath.clear();
	PendingImportOptions = {};
	bOpenImportDialogNextFrame = false;
}
