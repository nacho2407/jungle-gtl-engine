#include "EditorUI.h"

#include "Core/Core.h"
#include "Object/Object.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Platform/Windows/Window.h"
#include "Renderer/Renderer.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "Core/ViewportClient.h"
#include "Core/Paths.h"

#include <windows.h>
#include <commdlg.h>

#include "Viewport/EditorViewportClient.h"
#include "Debug/EngineLog.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Serializer/SceneSerializer.h"
#include "Actor/SkySphereActor.h" 
#include "Actor/ObjActor.h"
#include "Core/ShowFlags.h"

enum class EFileDialogType
{
	Open,
	Save
};

std::string GetFilePathUsingDialog(EFileDialogType Type)
{
	char FileName[MAX_PATH] = "";
	FString ContentDir = FPaths::ContentDir().string();

	OPENFILENAMEA Ofn = {};
	Ofn.lStructSize = sizeof(OPENFILENAMEA);
	Ofn.lpstrFilter = "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
	Ofn.lpstrFile = FileName;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrDefExt = "json";
	Ofn.lpstrInitialDir = ContentDir.c_str();

	if (Type == EFileDialogType::Save)
	{
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

		if (GetSaveFileNameA(&Ofn))
			return std::string(FileName);
	}
	else // Open
	{
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&Ofn))
			return std::string(FileName);
	}

	return "";
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void CEditorUI::Initialize(CCore* InCore)
{
	Core = InCore;

	Property.OnChanged = [this](const FVector& Loc, const FVector& Rot, const FVector& Scl)
		{
			if (!Core)
			{
				return;
			}

			AActor* Selected = Core->GetSelectedActor();
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
			if (Core)
			{

				Core->GetViewportClient()->HandleFileDoubleClick(FilePath);
			}
		};

	ContentBrowser.OnFileDragEnd = [this](const FString& DraggingFilePath, const FString& ReleaseDirectory)
		{
			if (ContentBrowser.IsHovered())
			{
				if (ContentBrowser.IsMouseOnDirectory())
				{
					std::filesystem::path Src = DraggingFilePath;
					std::filesystem::path DstDir = ReleaseDirectory;

					std::filesystem::path Dst = DstDir / Src.filename();

					std::error_code ec;

					if (std::filesystem::exists(Dst))
					{
						int Result = MessageBoxW(
							nullptr,
							L"이미 같은 이름의 파일이 존재합니다.\n덮어쓰시겠습니까?",
							L"Overwrite",
							MB_YESNO | MB_ICONWARNING
						);

						if (Result != IDYES)
						{
							return; // 취소
						}

						// 덮어쓰기 위해 기존 파일 삭제
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
						UE_LOG("Moved: %s -> %s", Src.string().c_str(), Dst.string().c_str());
					}
				}
			}
		};
}

void CEditorUI::AttachToRenderer(CRenderer* InRenderer)
{
	if (!Core || !InRenderer)
	{
		return;
	}

	bViewportClientActive = true;
	CurrentRenderer = InRenderer;

	const HWND Hwnd = InRenderer->GetHwnd();
	ID3D11Device* Device = InRenderer->GetDevice();
	ID3D11DeviceContext* DeviceContext = InRenderer->GetDeviceContext();

	ContentBrowser.SetFolderIcon(CurrentRenderer->GetFolderIconSRV());
	ContentBrowser.SetFileIcon(CurrentRenderer->GetFileIconSRV());

	std::filesystem::path FontPath = FPaths::ProjectRoot() / "Content" / "Fonts" / "NotoSansKR-Bold.ttf";
	std::wstring FontPathWString = FontPath.wstring();
	InRenderer->SetGUICallbacks(
		[Hwnd, Device, DeviceContext, FontPathWString, FontPath]()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& IO = ImGui::GetIO();
			IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
			IO.IniFilename = "imgui_editor.ini";

			ImFontConfig FontConfig;
			FontConfig.OversampleH = 1;
			FontConfig.OversampleV = 1;
			FontConfig.PixelSnapH = true;

			ImFont* Font = nullptr;
			FILE* f;
			_wfopen_s(&f, FontPath.c_str(), L"rb");
			if (f) {
				// 1. 파일 크기 확인
				fseek(f, 0, SEEK_END);
				size_t size = ftell(f);
				fseek(f, 0, SEEK_SET);

				// 2. ImGui 전용 메모리 할당 (ImGui가 나중에 직접 free함)
				void* fontData = IM_ALLOC(size);
				fread(fontData, 1, size, f);
				fclose(f);

				// 3. 메모리로부터 폰트 로드
				// 마지막 인자로 한글 범위를 지정해야 화면에 한글이 출력됩니다.
				Font = IO.Fonts->AddFontFromMemoryTTF(fontData, (int)size, 16.0f, &FontConfig, IO.Fonts->GetGlyphRangesKorean());
			}
			else {
				fclose(f);
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
		},
		[]()
		{
			ImGuiIO& IO = ImGui::GetIO();
			if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	);

	InRenderer->SetGUIUpdateCallback([this]() { Render(); });

	InRenderer->SetPostRenderCallback([this](CRenderer* Renderer)
		{
			if (!Core)
			{
				return;
			}
	
			AActor* Selected = Core->GetSelectedActor();
			if (Selected && !Selected->IsPendingDestroy() && Selected->IsVisible()
				&& !Selected->IsA<ASkySphereActor>()
				&& Core->GetViewportClient()->GetShowFlags().HasFlag(EEngineShowFlags::SF_Primitives))
			{
				for (UActorComponent* Component : Selected->GetComponents())
				{
					if (!Component->IsA(UPrimitiveComponent::StaticClass()))
					{
						continue;
					}

					UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
					if (PrimitiveComponent->GetPrimitive())
					{
						Renderer->RenderOutline(
							PrimitiveComponent->GetPrimitive()->GetMeshData(),
							PrimitiveComponent->GetWorldTransform()
						);
					}
				}
			}

			const float AxisLength = 10000.0f;
			const FVector Origin = { 0.0f, 0.0f, 0.0f };
		});
	LoadEditorSettings();
}

void CEditorUI::DetachFromRenderer(CRenderer* InRenderer)
{
	bViewportClientActive = false;
	CurrentRenderer = nullptr;

	if (InRenderer)
	{
		InRenderer->ClearSceneRenderTarget();
		InRenderer->ClearViewportCallbacks();
	}
}

void CEditorUI::SetupWindow(CWindow* InWindow)
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

			if (IsViewportInteractive())
			{
				return false;
			}

			return bHandledByImGui;
		});
}

void CEditorUI::BuildDefaultLayout(uint32 DockID)
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

void CEditorUI::LoadEditorSettings()
{
	std::wstring Path = GetEditorIniPathW();
	wchar_t Buf[64];

	GetPrivateProfileStringW(L"Grid", L"GridSize", L"10.0", Buf, 64, Path.c_str());
	float GridSize = static_cast<float>(_wtof(Buf));

	GetPrivateProfileStringW(L"Grid", L"LineThickness", L"1.0", Buf, 64, Path.c_str());
	float Thickness = static_cast<float>(_wtof(Buf));

	GetPrivateProfileStringW(L"Grid", L"ShowGrid", L"1", Buf, 64, Path.c_str());
	bool bShowGrid = (_wtoi(Buf) != 0);

	if (Core && Core->GetViewportClient())
	{
		auto* VPC = static_cast<CEditorViewportClient*>(Core->GetViewportClient());
		VPC->SetGridSize(GridSize);
		VPC->SetLineThickness(Thickness);
		VPC->SetGridVisible(bShowGrid);
		FShowFlags& ShowFlags = VPC->GetShowFlags();

		GetPrivateProfileStringW(L"ShowFlags", L"Primitives", L"1", Buf, 64, Path.c_str());
		ShowFlags.SetFlag(EEngineShowFlags::SF_Primitives, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(L"ShowFlags", L"UUID", L"1", Buf, 64, Path.c_str());
		ShowFlags.SetFlag(EEngineShowFlags::SF_UUID, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(L"ShowFlags", L"DebugDraw", L"0", Buf, 64, Path.c_str());
		ShowFlags.SetFlag(EEngineShowFlags::SF_DebugDraw, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(L"ShowFlags", L"WorldAxis", L"0", Buf, 64, Path.c_str());
		ShowFlags.SetFlag(EEngineShowFlags::SF_WorldAxis, _wtoi(Buf) != 0);

		GetPrivateProfileStringW(L"ShowFlags", L"Collision", L"0", Buf, 64, Path.c_str());
		ShowFlags.SetFlag(EEngineShowFlags::SF_Collision, _wtoi(Buf) != 0);

	}

}

void CEditorUI::SaveEditorSettings()
{
	std::wstring Path = GetEditorIniPathW();
	if (!Core || !Core->GetViewportClient()) return;
	auto* VPC = static_cast<CEditorViewportClient*>(Core->GetViewportClient());

	wchar_t Buf[64];
	swprintf(Buf, 64, L"%.2f", VPC->GetGridSize());
	WritePrivateProfileStringW(L"Grid", L"GridSize", Buf, Path.c_str());

	swprintf(Buf, 64, L"%.2f", VPC->GetLineThickness());
	WritePrivateProfileStringW(L"Grid", L"LineThickness", Buf, Path.c_str());

	WritePrivateProfileStringW(L"Grid", L"ShowGrid", VPC->IsGridVisible() ? L"1" : L"0", Path.c_str());
	FShowFlags& ShowFlags = VPC->GetShowFlags();
	WritePrivateProfileStringW(L"ShowFlags", L"Primitives", ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives) ? L"1" : L"0", Path.c_str());
	WritePrivateProfileStringW(L"ShowFlags", L"UUID", ShowFlags.HasFlag(EEngineShowFlags::SF_UUID) ? L"1" : L"0", Path.c_str());
	WritePrivateProfileStringW(L"ShowFlags", L"DebugDraw", ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw) ? L"1" : L"0", Path.c_str());
	WritePrivateProfileStringW(L"ShowFlags", L"WorldAxis", ShowFlags.HasFlag(EEngineShowFlags::SF_WorldAxis) ? L"1" : L"0", Path.c_str());
	WritePrivateProfileStringW(L"ShowFlags", L"Collision", ShowFlags.HasFlag(EEngineShowFlags::SF_Collision) ? L"1" : L"0", Path.c_str());

}

std::wstring CEditorUI::GetEditorIniPathW() const
{
	return (FPaths::ProjectRoot() / "editor.ini").wstring();
}


void CEditorUI::Render()
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
	ImGui::PopStyleVar();
	ImGui::End();

	if (Core)
	{
		AActor* Selected = Core->GetSelectedActor();
		if (Selected != CachedSelectedActor)
		{
			SyncSelectedActorProperty();
		}

		const FTimer& Timer = Core->GetTimer();
		Stat.SetFPS(Timer.GetDisplayFPS());
		Stat.SetFrameTimeMs(Timer.GetFrameTimeMs());
	}

	Stat.SetObjectCount(UObject::TotalAllocationCounts);
	Stat.SetHeapUsage(UObject::TotalAllocationBytes);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene"))
			{
				if (Core)
				{
					Core->SetSelectedActor(nullptr);

					if (UCameraComponent* Cam = Core->GetActiveWorld()->GetActiveCameraComponent())
					{
						Cam->GetCamera()->SetPosition({ -5.0f, 0.0f, 2.0f });
						Cam->GetCamera()->SetRotation(0.f, 0.f);
					}
					Core->GetScene()->ClearActors();
					UE_LOG("New scene created");
				}
			}

			if (ImGui::MenuItem("Open Scene"))
			{
				if (Core && Core->GetActiveScene())
				{
					FString Path = GetFilePathUsingDialog(EFileDialogType::Open);

					if (!Path.empty())
					{
						Core->SetSelectedActor(nullptr);
						Core->GetScene()->ClearActors();

						bool bLoaded = FSceneSerializer::Load(Core->GetScene(), Path, Core->GetRenderer()->GetDevice());
						if (bLoaded)
						{
							UE_LOG("Scene loaded: %s", Path.c_str());
						}
						else
						{
							MessageBoxW(
								nullptr,
								L"Scene 정보가 잘못되었습니다.",
								L"Error",
								MB_OK | MB_ICONWARNING
							);
						}
					}
				}
			}

			if (ImGui::MenuItem("Save Scene As..."))
			{
				if (Core && Core->GetActiveScene())
				{
					FString Path = GetFilePathUsingDialog(EFileDialogType::Save);

					if (!Path.empty())
					{
						FSceneSerializer::Save(Core->GetScene(),Path);
					}
				}
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (Core && Core->GetViewportClient())
			{
				auto* VPC = static_cast<CEditorViewportClient*>(Core->GetViewportClient());
			

				IViewportClient* ViewportCli = Core->GetViewportClient();
				if (!ViewportCli) { ImGui::End(); return; }

				FShowFlags& ShowFlags = ViewportCli->GetShowFlags();
				// ===== Show Flags 섹션 =====
				ImGui::SeparatorText("Show Flags");
				// 각 플래그마다 Checkbox 하나씩
				auto ShowFlagCheckbox = [&](const char* Label, EEngineShowFlags Flag)
				{
					bool bValue = ShowFlags.HasFlag(Flag);
					if (ImGui::Checkbox(Label, &bValue))
					{
						ShowFlags.SetFlag(Flag, bValue);
						SaveEditorSettings();
					}
				};

				ShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives);
				ShowFlagCheckbox("UUID", EEngineShowFlags::SF_UUID);
				ShowFlagCheckbox("Debug Draw", EEngineShowFlags::SF_DebugDraw);
				//ShowFlagCheckbox("World Axis", EEngineShowFlags::SF_WorldAxis);
				ShowFlagCheckbox("Collision", EEngineShowFlags::SF_Collision);

				// ─── Grid ───
				ImGui::SeparatorText("Grid");
				bool bShowGrid = VPC->IsGridVisible();
				if (ImGui::Checkbox("Show Grid", &bShowGrid))
				{
					VPC->SetGridVisible(bShowGrid);
					SaveEditorSettings();
				}
				float GridSize = VPC->GetGridSize();
				if (ImGui::SliderFloat("Grid Size", &GridSize, 1.0f, 100.0f, "%.1f"))
				{
					VPC->SetGridSize(GridSize);
					SaveEditorSettings();
				}

				float Thickness = VPC->GetLineThickness();
				if (ImGui::SliderFloat("Line Thickness", &Thickness, 0.1f, 5.0f, "%.2f"))
				{
					VPC->SetLineThickness(Thickness);
					SaveEditorSettings();
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
		ImGui::EndMainMenuBar();
	}

	if (bOpenAboutPopup)
	{
		ImGui::OpenPopup("AboutPopup"); 
		ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_Always); // ← 원하는 크기로 조절
		bOpenAboutPopup = false;
	}

	if (ImGui::BeginPopupModal("AboutPopup", nullptr, ImGuiWindowFlags_NoTitleBar))
	{
		// 헤더 배경
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImVec2 WinPos = ImGui::GetWindowPos();
		ImVec2 WinSize = ImGui::GetWindowSize();
		DrawList->AddRectFilled(WinPos, ImVec2(WinPos.x + WinSize.x, WinPos.y + 60), IM_COL32(30, 30, 60, 255));

		ImGui::SetCursorPosY(12);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("Dino Engine").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Dino Engine");

		ImGui::SetCursorPosY(35);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("v1.0.0").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "v1.0.0");

		ImGui::SetCursorPosY(70);
		ImGui::SetCursorPosX(20);

		// Contributors
		ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "Contributors");
		ImGui::SameLine();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9f, 0.7f, 0.3f, 0.5f));
		ImGui::Separator();
		ImGui::PopStyleColor();

		ImGui::Spacing();

		const char* Contributors[] = { "김지수", "김태현", "박세영", "조상현" };
		for (const char* Name : Contributors)
		{
			ImGui::SetCursorPosX(20);
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.6f, 1.0f), "•");
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

	ControlPanel.Render(Core);
	Property.Render(Core);
	Console.Render();
	Stat.Render();
	Outliner.Render(Core);
	ContentBrowser.Render();
}

bool CEditorUI::GetViewportMousePosition(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const
{
	return false;
	// return Viewport.GetMousePositionInViewport(WindowMouseX, WindowMouseY, OutViewportX, OutViewportY, OutWidth, OutHeight);
}

void CEditorUI::SyncSelectedActorProperty()
{
	if (!Core)
	{
		return;
	}

	AActor* Selected = Core->GetSelectedActor();
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
}

bool CEditorUI::IsViewportInteractive() const
{
	return false;
	//return Viewport.IsVisible() && (Viewport.IsHovered() || Viewport.IsFocused());
}
