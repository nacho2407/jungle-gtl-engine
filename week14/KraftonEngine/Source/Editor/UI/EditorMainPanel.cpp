#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputSystem.h"

#include "Editor/UI/ImGuiSetting.h"
#include "Editor/UI/NotificationToast.h"

#include <algorithm>
#include <random>
#include <utility>

namespace
{
struct FDebugPlaceActorOption
{
	const char* Label = "";
	FLevelViewportLayout::EViewportPlaceActorType Type = FLevelViewportLayout::EViewportPlaceActorType::Cube;
};

const FDebugPlaceActorOption GDebugPlaceActorOptions[] = {
	{ "Cube", FLevelViewportLayout::EViewportPlaceActorType::Cube },
	{ "Sphere", FLevelViewportLayout::EViewportPlaceActorType::Sphere },
	{ "Cylinder", FLevelViewportLayout::EViewportPlaceActorType::Cylinder },
	{ "Decal", FLevelViewportLayout::EViewportPlaceActorType::Decal },
	{ "Height Fog", FLevelViewportLayout::EViewportPlaceActorType::HeightFog },
	{ "Ambient Light", FLevelViewportLayout::EViewportPlaceActorType::AmbientLight },
	{ "Directional Light", FLevelViewportLayout::EViewportPlaceActorType::DirectionalLight },
	{ "Point Light", FLevelViewportLayout::EViewportPlaceActorType::PointLight },
	{ "Spot Light", FLevelViewportLayout::EViewportPlaceActorType::SpotLight },
};

bool IsAliveObject(const UObject* Object)
{
	return Object && std::find(GUObjectArray.begin(), GUObjectArray.end(), Object) != GUObjectArray.end();
}
}

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiSetting::LoadSetting();

	ImGuiIO& IO = ImGui::GetIO();
	IO.IniFilename = "Settings/imgui.ini";
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;
	EditorEngine = InEditorEngine;

	// 한글 지원 폰트 로드 (시스템 맑은 고딕)
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
	ContentBrowserWidget.Initialize(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
	ShadowMapDebugWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
	ConsoleWidget.Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::SaveToSettings() const
{
	ContentBrowserWidget.SaveToSettings();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
	RenderMainMenuBar();

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
		EditorEngine->RenderViewportUI(DeltaTime);
	}

	const FEditorSettings& Settings = FEditorSettings::Get();

	if (!bHideEditorWindows && Settings.UI.bImGUISettings)
	{
		ImGuiSetting::ShowSetting();
	}

	if (!bHideEditorWindows && Settings.UI.bControl)
	{
		SCOPE_STAT_CAT("ControlWidget.Render", "5_UI");
		ControlWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bProperty)
	{
		SCOPE_STAT_CAT("PropertyWidget.Render", "5_UI");
		PropertyWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bScene)
	{
		SCOPE_STAT_CAT("SceneWidget.Render", "5_UI");
		SceneWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bStat)
	{
		SCOPE_STAT_CAT("StatWidget.Render", "5_UI");
		StatWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bContentBrowser)
	{
		SCOPE_STAT_CAT("ContentBrowserWidget.Render", "5_UI");
		ContentBrowserWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bShadowMapDebug)
	{
		ShadowMapDebugWidget.Render(DeltaTime);
	}

	ProjectSettingsWidget.Render();

	if (!bHideEditorWindows)
	{
		RenderEditorDebugPanel();
	}

	RenderShortcutOverlay();
	RenderConsoleDrawer(DeltaTime);
	RenderFooterOverlay(DeltaTime);

	// 토스트 알림 (항상 최상위에 표시)
	FNotificationToast::Render();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderMainMenuBar()
{
	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Scene", "Ctrl+N") && EditorEngine)
		{
			EditorEngine->NewScene();
		}
		if (ImGui::MenuItem("Open Scene...", "Ctrl+O") && EditorEngine)
		{
			EditorEngine->LoadSceneWithDialog();
		}
		if (ImGui::MenuItem("Save Scene", "Ctrl+S") && EditorEngine)
		{
			EditorEngine->SaveScene();
		}
		if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S") && EditorEngine)
		{
			EditorEngine->SaveSceneAsWithDialog();
		}

		ImGui::Separator();
		const char* CurrentSceneLabel = "Current: Unsaved Scene";
		FString CurrentScenePath;
		FString CurrentSceneText;
		if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
		{
			CurrentScenePath = EditorEngine->GetCurrentLevelFilePath();
			CurrentSceneText = FString("Current: ") + CurrentScenePath;
			CurrentSceneLabel = CurrentSceneText.c_str();
		}
		ImGui::BeginDisabled();
		ImGui::MenuItem(CurrentSceneLabel, nullptr, false, false);
		ImGui::EndDisabled();
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("Windows"))
	{
		bShowWidgetList = true;
		ImGui::OpenPopup("##WidgetListPopup");
	}
	if (ImGui::BeginPopup("##WidgetListPopup"))
	{
		ImGui::Checkbox("Control", &Settings.UI.bControl);
		ImGui::Checkbox("Property", &Settings.UI.bProperty);
		ImGui::Checkbox("Scene", &Settings.UI.bScene);
		ImGui::Checkbox("Stat", &Settings.UI.bStat);
		ImGui::Checkbox("ContentBrowser", &Settings.UI.bContentBrowser);
		ImGui::Checkbox("Editor Debug", &Settings.UI.bEditorDebug);
		ImGui::Checkbox("Shadow Map Debug", &Settings.UI.bShadowMapDebug);
		ImGui::Separator();
		ImGui::Checkbox("IMGUI_Setting", &Settings.UI.bImGUISettings);
		ImGui::EndPopup();
	}
	else
	{
		bShowWidgetList = false;
	}

	if (ImGui::MenuItem("Project Settings"))
	{
		ProjectSettingsWidget.bOpen = true;
	}

	if (ImGui::MenuItem("Shortcut"))
	{
		bShowShortcutOverlay = !bShowShortcutOverlay;
	}

	ImGui::EndMainMenuBar();
}

void FEditorMainPanel::RenderShortcutOverlay()
{
	if (!bShowShortcutOverlay)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(320.0f, 150.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Shortcut Help", &bShowShortcutOverlay, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("File");
	ImGui::Separator();
	ImGui::TextUnformatted("Ctrl+N : New Scene");
	ImGui::TextUnformatted("Ctrl+O : Open Scene");
	ImGui::TextUnformatted("Ctrl+S : Save Scene");
	ImGui::TextUnformatted("Ctrl+Shift+S : Save Scene As");
	ImGui::Separator();
	ImGui::TextUnformatted("` : Focus console input / open console drawer");
	ImGui::TextUnformatted("F : Focus on selection");
	ImGui::TextUnformatted("Ctrl + LMB : Multi Picking (Toggle)");
	ImGui::TextUnformatted("Ctrl + Alt + LMB Drag : Area Selection");

	ImGui::End();
}

void FEditorMainPanel::RenderEditorDebugPanel()
{
	FEditorSettings& Settings = FEditorSettings::Get();
	if (!Settings.UI.bEditorDebug || !EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(520.0f, 320.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Editor Debug", &Settings.UI.bEditorDebug))
	{
		ImGui::End();
		return;
	}

	if (ImGui::CollapsingHeader("Place Actors (Grid)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const int32 OptionCount = static_cast<int32>(sizeof(GDebugPlaceActorOptions) / sizeof(GDebugPlaceActorOptions[0]));
		if (DebugPlaceActorTypeIndex < 0)
		{
			DebugPlaceActorTypeIndex = 0;
		}
		if (DebugPlaceActorTypeIndex >= OptionCount)
		{
			DebugPlaceActorTypeIndex = OptionCount - 1;
		}

		const char* CurrentActorLabel = GDebugPlaceActorOptions[DebugPlaceActorTypeIndex].Label;
		if (ImGui::BeginCombo("Actor Type", CurrentActorLabel))
		{
			for (int32 Index = 0; Index < OptionCount; ++Index)
			{
				const bool bSelected = (DebugPlaceActorTypeIndex == Index);
				if (ImGui::Selectable(GDebugPlaceActorOptions[Index].Label, bSelected))
				{
					DebugPlaceActorTypeIndex = Index;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::DragInt("Rows", &DebugGridRows, 1.0f, 1, 1024, "%d");
		ImGui::DragInt("Cols", &DebugGridCols, 1.0f, 1, 1024, "%d");
		ImGui::DragInt("Layers", &DebugGridLayers, 1.0f, 1, 256, "%d");
		ImGui::DragFloat("Grid Spacing", &DebugGridSpacing, 0.1f, 0.1f, 1000.0f, "%.2f");
		ImGui::Checkbox("Center Grid Around Origin", &bDebugGridCenter);

		ImGui::Separator();
		ImGui::Checkbox("Use Camera Forward Origin", &bDebugUseCameraOrigin);
		if (bDebugUseCameraOrigin)
		{
			ImGui::DragFloat("Camera Forward Distance", &DebugCameraForwardDistance, 0.5f, 0.0f, 100000.0f, "%.1f");
		}
		else
		{
			ImGui::DragFloat3("Manual Origin", &DebugManualGridOrigin.X, 0.1f, -100000.0f, 100000.0f, "%.2f");
		}

		ImGui::Separator();
		ImGui::Checkbox("Random Yaw", &bDebugRandomYaw);
		ImGui::BeginDisabled(!bDebugRandomYaw);
		ImGui::DragFloat("Yaw Range (+/-)", &DebugRandomYawRange, 1.0f, 0.0f, 180.0f, "%.1f");
		ImGui::EndDisabled();

		ImGui::Checkbox("Apply Position Jitter", &bDebugApplyJitter);
		ImGui::BeginDisabled(!bDebugApplyJitter);
		ImGui::DragFloat("Jitter XY", &DebugJitterXY, 0.05f, 0.0f, 1000.0f, "%.2f");
		ImGui::DragFloat("Jitter Z", &DebugJitterZ, 0.05f, 0.0f, 1000.0f, "%.2f");
		ImGui::EndDisabled();

		if (DebugGridRows < 1) DebugGridRows = 1;
		if (DebugGridCols < 1) DebugGridCols = 1;
		if (DebugGridLayers < 1) DebugGridLayers = 1;
		if (DebugGridSpacing < 0.1f) DebugGridSpacing = 0.1f;
		if (DebugRandomYawRange < 0.0f) DebugRandomYawRange = 0.0f;
		if (DebugRandomYawRange > 180.0f) DebugRandomYawRange = 180.0f;
		if (DebugJitterXY < 0.0f) DebugJitterXY = 0.0f;
		if (DebugJitterZ < 0.0f) DebugJitterZ = 0.0f;

		const long long TotalSpawnCount =
			static_cast<long long>(DebugGridRows) *
			static_cast<long long>(DebugGridCols) *
			static_cast<long long>(DebugGridLayers);
		ImGui::Text("Total Actors: %lld", TotalSpawnCount);
		ImGui::Text("Last Batch: %u", static_cast<uint32>(DebugLastSpawnedActors.size()));

		if (ImGui::Button("Spawn Grid Actors"))
		{
			UWorld* World = EditorEngine->GetWorld();
			if (!World)
			{
				FEditorConsoleWidget::AddLog("Grid spawn failed: invalid world\n");
			}
			else
			{
				FVector GridOrigin = DebugManualGridOrigin;
				FVector GridRight(1.0f, 0.0f, 0.0f);
				FVector GridForward(0.0f, 1.0f, 0.0f);
				if (bDebugUseCameraOrigin)
				{
					if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
					{
						if (UCameraComponent* ActiveCamera = ActiveViewport->GetCamera())
						{
							FVector CameraForward = ActiveCamera->GetForwardVector();
							CameraForward.Z = 0.0f;
							if (CameraForward.Length() > 0.0001f)
							{
								CameraForward.Normalize();
								GridForward = CameraForward;
								GridRight = FVector(-CameraForward.Y, CameraForward.X, 0.0f);
							}
							GridOrigin = ActiveCamera->GetWorldLocation() + ActiveCamera->GetForwardVector() * DebugCameraForwardDistance;
						}
					}
				}

				const float RowOffset = bDebugGridCenter ? (static_cast<float>(DebugGridRows - 1) * 0.5f) : 0.0f;
				const float ColOffset = bDebugGridCenter ? (static_cast<float>(DebugGridCols - 1) * 0.5f) : 0.0f;
				const float LayerOffset = bDebugGridCenter ? (static_cast<float>(DebugGridLayers - 1) * 0.5f) : 0.0f;

				std::mt19937 RNG{ std::random_device{}() };
				std::uniform_real_distribution<float> YawDist(-DebugRandomYawRange, DebugRandomYawRange);
				std::uniform_real_distribution<float> JitterXYDist(-DebugJitterXY, DebugJitterXY);
				std::uniform_real_distribution<float> JitterZDist(-DebugJitterZ, DebugJitterZ);

				TArray<AActor*> SpawnedActors;
				SpawnedActors.reserve(static_cast<size_t>(TotalSpawnCount));
				int32 SpawnedCount = 0;
				const FDebugPlaceActorOption& Option = GDebugPlaceActorOptions[DebugPlaceActorTypeIndex];

				for (int32 Layer = 0; Layer < DebugGridLayers; ++Layer)
				{
					for (int32 Row = 0; Row < DebugGridRows; ++Row)
					{
						for (int32 Col = 0; Col < DebugGridCols; ++Col)
						{
							FVector SpawnLocation = GridOrigin
								+ GridRight * ((static_cast<float>(Col) - ColOffset) * DebugGridSpacing)
								+ GridForward * ((static_cast<float>(Row) - RowOffset) * DebugGridSpacing)
								+ FVector(0.0f, 0.0f, (static_cast<float>(Layer) - LayerOffset) * DebugGridSpacing);

							if (bDebugApplyJitter)
							{
								SpawnLocation += GridRight * JitterXYDist(RNG)
									+ GridForward * JitterXYDist(RNG)
									+ FVector(0.0f, 0.0f, JitterZDist(RNG));
							}

							AActor* SpawnedActor = EditorEngine->SpawnPlaceActor(Option.Type, SpawnLocation);
							if (!SpawnedActor)
							{
								continue;
							}

							if (bDebugRandomYaw)
							{
								SpawnedActor->SetActorRotation(FVector(0.0f, YawDist(RNG), 0.0f));
							}

							SpawnedActors.push_back(SpawnedActor);
							++SpawnedCount;
						}
					}
				}

				DebugLastSpawnedActors = std::move(SpawnedActors);
				FEditorConsoleWidget::AddLog("Grid placed: %d actors\n", SpawnedCount);
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear Last Batch"))
		{
			bPendingClearLastBatch = true;
		}
	}

	ImGui::End();
}

void FEditorMainPanel::RenderConsoleDrawer(float DeltaTime)
{
	constexpr float DrawerMaxHeight = 320.0f;
	constexpr float AnimSpeed = 16.0f;

	const float TargetAnim = bConsoleDrawerVisible ? 1.0f : 0.0f;
	float Alpha = DeltaTime * AnimSpeed;
	if (Alpha > 1.0f)
	{
		Alpha = 1.0f;
	}
	ConsoleDrawerAnim += (TargetAnim - ConsoleDrawerAnim) * Alpha;
	if (!bConsoleDrawerVisible && ConsoleDrawerAnim < 0.001f)
	{
		ConsoleDrawerAnim = 0.0f;
	}
	if (ConsoleDrawerAnim <= 0.001f)
	{
		return;
	}

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	const float DrawerHeight = DrawerMaxHeight * ConsoleDrawerAnim;
	if (DrawerHeight <= 1.0f)
	{
		return;
	}

	const ImVec2 DrawerPos(
		MainViewport->WorkPos.x,
		MainViewport->WorkPos.y + MainViewport->WorkSize.y - FooterHeight - DrawerHeight);
	const ImVec2 DrawerSize(MainViewport->WorkSize.x, DrawerHeight);
	ImGui::SetNextWindowPos(DrawerPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(DrawerSize, ImGuiCond_Always);
	if (bBringConsoleDrawerToFrontNextFrame)
	{
		ImGui::SetNextWindowFocus();
	}

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.98f));
	if (ImGui::Begin("##ConsoleDrawer", nullptr, Flags))
	{
		ConsoleWidget.RenderDrawerToolbar();
		ImGui::Separator();
		ConsoleWidget.RenderLogContents(0.0f);
	}
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);

	bBringConsoleDrawerToFrontNextFrame = false;
}

void FEditorMainPanel::RenderFooterOverlay(float DeltaTime)
{
	(void)DeltaTime;

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	const ImVec2 FooterPos(
		MainViewport->WorkPos.x,
		MainViewport->WorkPos.y + MainViewport->WorkSize.y - FooterHeight);
	const ImVec2 FooterSize(MainViewport->WorkSize.x, FooterHeight);

	ImGui::SetNextWindowPos(FooterPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(FooterSize, ImGuiCond_Always);
	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoNav;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.065f, 0.075f, 0.98f));
	if (ImGui::Begin("##EditorFooter", nullptr, Flags))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
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

		if (bFocusConsoleButtonNextFrame)
		{
			ImGui::SetKeyboardFocusHere();
			bFocusConsoleButtonNextFrame = false;
		}

		if (ImGui::SmallButton("Console"))
		{
			ToggleConsoleDrawer(true);
		}

		ImGui::SameLine();
		const bool bDrawerOpen = ConsoleDrawerAnim > 0.5f;
		const float InputWidth = MainViewport->WorkSize.x * (bDrawerOpen ? 0.35f : 0.175f);
		ConsoleWidget.RenderInputLine("##FooterConsoleInput", InputWidth, bFocusConsoleInputNextFrame);
		if (bFocusConsoleInputNextFrame)
		{
			ConsoleBacktickCycleState = bConsoleDrawerVisible ? 2 : 1;
		}
		bFocusConsoleInputNextFrame = false;

		ImGui::SameLine();
		ImGui::Text("Domain: %s", EditorEngine && EditorEngine->IsPlayingInEditor() ? "PIE" : "Editor");

		const FString LevelLabel = EditorEngine && EditorEngine->HasCurrentLevelFilePath()
			? FString("Level: ") + EditorEngine->GetCurrentLevelFilePath()
			: FString("Level: Unsaved");
		const float LevelWidth = ImGui::CalcTextSize(LevelLabel.c_str()).x;
		const float LevelX = MainViewport->WorkSize.x - ImGui::GetStyle().WindowPadding.x - LevelWidth;

		const char* LatestLog = ConsoleWidget.GetLatestLogMessage();
		if (LatestLog && LatestLog[0] != '\0')
		{
			const float LogWidth = ImGui::CalcTextSize(LatestLog).x;
			float LogX = LevelX - 16.0f - LogWidth;
			const float MinLogX = ImGui::GetCursorPosX() + 8.0f;
			if (LogX < MinLogX)
			{
				LogX = MinLogX;
			}
			ImGui::SameLine(LogX);
			ImGui::TextUnformatted(LatestLog);
		}

		ImGui::SameLine(LevelX);
		ImGui::TextUnformatted(LevelLabel.c_str());
	}
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void FEditorMainPanel::Update()
{
	HandleGlobalShortcuts();
	ProcessPendingDebugActions();

	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 bUsingMouse를 해제해야 TickInteraction이 동작
	bool bWantMouse = IO.WantCaptureMouse;
	bool bWantKeyboard = IO.WantCaptureKeyboard || bShowShortcutOverlay;
	if (EditorEngine && EditorEngine->IsMouseOverViewport())
	{
		bWantMouse = false;
		if (!IO.WantTextInput && !bShowShortcutOverlay)
		{
			bWantKeyboard = false;
		}
	}
	InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = bWantKeyboard;

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			ImmAssociateContext(hWnd, NULL);
		}
	}
}

void FEditorMainPanel::ToggleConsoleDrawer(bool bFocusInput)
{
	bConsoleDrawerVisible = !bConsoleDrawerVisible;
	bBringConsoleDrawerToFrontNextFrame = bConsoleDrawerVisible;
	bFocusConsoleInputNextFrame = bConsoleDrawerVisible && bFocusInput;
	ConsoleBacktickCycleState = bConsoleDrawerVisible ? 2 : 0;
	if (!bConsoleDrawerVisible)
	{
		bFocusConsoleButtonNextFrame = true;
	}
}

void FEditorMainPanel::ProcessPendingDebugActions()
{
	if (!bPendingClearLastBatch || !EditorEngine)
	{
		return;
	}
	bPendingClearLastBatch = false;

	UWorld* World = EditorEngine->GetWorld();
	int32 DestroyedCount = 0;
	if (!World)
	{
		DebugLastSpawnedActors.clear();
		FEditorConsoleWidget::AddLog("Grid cleared: 0 actors\n");
		return;
	}

	EditorEngine->GetSelectionManager().ClearSelection();
	for (AActor* Actor : DebugLastSpawnedActors)
	{
		if (!Actor || !IsAliveObject(Actor) || Actor->GetWorld() != World)
		{
			continue;
		}

		World->DestroyActor(Actor);
		++DestroyedCount;
	}

	DebugLastSpawnedActors.clear();
	FEditorConsoleWidget::AddLog("Grid cleared: %d actors\n", DestroyedCount);
}

void FEditorMainPanel::HandleGlobalShortcuts()
{
	if (!EditorEngine)
	{
		return;
	}

	ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput)
	{
		return;
	}

	InputSystem& Input = InputSystem::Get();
	if (!Input.GetKey(VK_CONTROL))
	{
		return;
	}

	const bool bShift = Input.GetKey(VK_SHIFT);
	if (Input.GetKeyDown('N'))
	{
		EditorEngine->NewScene();
	}
	else if (Input.GetKeyDown('O'))
	{
		EditorEngine->LoadSceneWithDialog();
	}
	else if (Input.GetKeyDown('S'))
	{
		if (bShift)
		{
			EditorEngine->SaveSceneAsWithDialog();
		}
		else
		{
			EditorEngine->SaveScene();
		}
	}
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
	if (bHasSavedUIVisibility)
	{
		bHideEditorWindows = true;
		bShowWidgetList = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	SavedUIVisibility = Settings.UI;
	bSavedShowWidgetList = bShowWidgetList;
	bHasSavedUIVisibility = true;
	bHideEditorWindows = true;
	bShowWidgetList = false;

	Settings.UI.bConsole = false;
	Settings.UI.bControl = false;
	Settings.UI.bProperty = false;
	Settings.UI.bScene = false;
	Settings.UI.bStat = false;
	Settings.UI.bContentBrowser = false;
	Settings.UI.bImGUISettings = false;
	Settings.UI.bEditorDebug = false;
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
	if (!bHasSavedUIVisibility)
	{
		bHideEditorWindows = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	Settings.UI = SavedUIVisibility;
	bShowWidgetList = bSavedShowWidgetList;
	bHideEditorWindows = false;
	bHasSavedUIVisibility = false;
}
