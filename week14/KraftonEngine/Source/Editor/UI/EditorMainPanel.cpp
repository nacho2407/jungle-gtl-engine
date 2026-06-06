#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/Level/LevelEditorViewportClient.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Render/Types/MinimalViewInfo.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Engine/Platform/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputSystem.h"

#include "Editor/Slate/SlateApplication.h"
#include "Editor/UI/Util/ImGuiSetting.h"
#include "Editor/UI/Util/NotificationToast.h"

#include "Editor/UI/Asset/Curve/FloatCurveEditorWidget.h"
#include "Editor/UI/Asset/CameraShake/CameraShakeEditorWidget.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Editor/UI/Asset/Mesh/StaticMeshEditorWidget.h"
#include "Editor/UI/Asset/Animation/AnimGraphEditorWidget.h"
#include "Editor/UI/Asset/Particle/ParticleSystemEditorWidget.h"
#include "Editor/UI/Asset/Material/MaterialEditorWidget.h"
#include "Editor/UI/Asset/Physics/PhysicsAssetEditorWidget.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
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
	{ "Camera", FLevelViewportLayout::EViewportPlaceActorType::Camera },
	{ "Cine Camera", FLevelViewportLayout::EViewportPlaceActorType::CineCamera },
	{ "Player Start", FLevelViewportLayout::EViewportPlaceActorType::PlayerStart },
	{ "Character",     FLevelViewportLayout::EViewportPlaceActorType::Character },
	{ "Lua Character", FLevelViewportLayout::EViewportPlaceActorType::LuaCharacter },
};

const char* BoolText(bool bValue)
{
	return bValue ? "Y" : "N";
}

FString SanitizeFooterLogMessage(const char* Message)
{
	if (!Message || Message[0] == '\0')
	{
		return "";
	}

	FString Result = Message;
	const size_t FirstLineBreak = Result.find_first_of("\r\n");
	if (FirstLineBreak != FString::npos)
	{
		Result.resize(FirstLineBreak);
	}

	while (!Result.empty() && std::isspace(static_cast<unsigned char>(Result.back())))
	{
		Result.pop_back();
	}
	return Result;
}

ImVec4 GetWallRunStatusColor(const char* Status)
{
	if (!Status)
	{
		return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
	}
	if (std::strcmp(Status, "ACTIVE") == 0)
	{
		return ImVec4(0.20f, 1.0f, 0.42f, 1.0f);
	}
	if (std::strncmp(Status, "NO_", 3) == 0 ||
		std::strncmp(Status, "BAD_", 4) == 0 ||
		std::strncmp(Status, "ENDED_", 6) == 0)
	{
		return ImVec4(1.0f, 0.28f, 0.22f, 1.0f);
	}
	if (std::strcmp(Status, "LOW_SPEED") == 0 || std::strcmp(Status, "NOT_FALLING") == 0)
	{
		return ImVec4(1.0f, 0.78f, 0.22f, 1.0f);
	}
	return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
}

UCharacterMovementComponent* FindWallRunDebugMovement(UEditorEngine* EditorEngine, AActor*& OutActor)
{
	OutActor = nullptr;
	if (!EditorEngine)
	{
		return nullptr;
	}

	auto TryActor = [&](AActor* Actor) -> UCharacterMovementComponent*
	{
		if (!Actor || !IsAliveObject(Actor))
		{
			return nullptr;
		}

		if (UCharacterMovementComponent* Movement = Actor->GetComponentByClass<UCharacterMovementComponent>())
		{
			OutActor = Actor;
			return Movement;
		}
		return nullptr;
	};

	if (UCharacterMovementComponent* Movement = TryActor(EditorEngine->GetSelectionManager().GetPrimarySelection()))
	{
		return Movement;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (UCharacterMovementComponent* Movement = TryActor(Actor))
		{
			return Movement;
		}
	}

	return nullptr;
}

void DrawDebugVectorLine(const char* Label, const FVector& Value)
{
	ImGui::Text("%s: %.2f, %.2f, %.2f", Label, Value.X, Value.Y, Value.Z);
}

constexpr float GEditorShortcutRepeatInitialDelay = 0.35f;
constexpr float GEditorShortcutRepeatInterval = 0.08f;

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
	IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ImGuiStyle& Style = ImGui::GetStyle();
	ImVec4* Colors = Style.Colors;
	
	Colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
	Colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
	Colors[ImGuiCol_CheckMark] = ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
	Colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
	ContentBrowserWidget.Initialize(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
	ShadowMapDebugWidget.Initialize(InEditorEngine);
	AnimationDebugWidget.Initialize(InEditorEngine);
	GameBuildWidget.Initialize(InEditorEngine);

	AssetEditorManager.RegisterEditor<FFloatCurveEditorWidget>();
	AssetEditorManager.RegisterEditor<FCameraShakeEditorWidget>();
	AssetEditorManager.RegisterEditor<FMeshEditorWidget>();
	AssetEditorManager.RegisterEditor<FStaticMeshEditorWidget>();
	AssetEditorManager.RegisterEditor<FAnimGraphEditorWidget>();
	AssetEditorManager.RegisterEditor<FParticleSystemEditorWidget>();
	AssetEditorManager.RegisterEditor<FMaterialEditorWidget>();
	AssetEditorManager.RegisterEditor<FPhysicsAssetEditorWidget>();
}

void FEditorMainPanel::Release()
{
	AssetEditorManager.CloseAll();
	GameBuildWidget.Shutdown();
	ConsoleWidget.Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::SaveToSettings() const
{
	ContentBrowserWidget.SaveToSettings();
}

void FEditorMainPanel::TickAssetEditors(float DeltaTime)
{
	AssetEditorManager.Tick(DeltaTime);
}

void FEditorMainPanel::CollectAssetEditorPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (EditorEngine && EditorEngine->IsPlayingInEditor())
	{
		return;
	}

	AssetEditorManager.CollectPreviewViewportClients(OutClients);
	FEditorMeshThumbnailManager::Get().CollectPreviewViewports(OutClients);
	FEditorMaterialThumbnailManager::Get().CollectPreviewViewports(OutClients);
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderMainMenuBar();

	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	RenderMainDockSpace(FooterHeight);

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
		EditorEngine->RenderViewportUI(DeltaTime);

		if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
		{
			EditorEngine->GetOverlayStatSystem().RenderImGui(*EditorEngine, ActiveViewport->GetViewportScreenRect());
		}
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

	if (!bHideEditorWindows && Settings.UI.bAnimationDebug)
	{
		SCOPE_STAT_CAT("AnimationDebugWidget.Render", "5_UI");
		AnimationDebugWidget.Render(DeltaTime);
	}

	if (Settings.UI.bWallRunDebug)
	{
		RenderWallRunDebugWindow();
	}

	ProjectSettingsWidget.Render();
	WorldSettingsWidget.Render();
	GameBuildWidget.Render(DeltaTime);

	if (!bHideEditorWindows)
	{
		RenderEditorDebugPanel();
	}

	RenderShortcutOverlay();
	RenderConsoleDrawer(DeltaTime);
	RenderFooterOverlay(DeltaTime);

	AssetEditorManager.Render(DeltaTime);
	UIEditorWidget.Render(DeltaTime);

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
		ImGui::Checkbox("Animation Debug", &Settings.UI.bAnimationDebug);
		ImGui::Checkbox("Wall Run Debug", &Settings.UI.bWallRunDebug);
		ImGui::Checkbox("IMGUI_Setting", &Settings.UI.bImGUISettings);
		ImGui::EndPopup();
	}
	else
	{
		bShowWidgetList = false;
	}

	if (ImGui::BeginMenu("Build"))
	{
		if (ImGui::MenuItem("Game Build..."))
		{
			GameBuildWidget.Open();
		}
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("Project Settings"))
	{
		ProjectSettingsWidget.bOpen = true;
	}

	if (ImGui::MenuItem("World Settings"))
	{
		WorldSettingsWidget.bOpen = true;
	}

	if (ImGui::MenuItem("Shortcut"))
	{
		bShowShortcutOverlay = !bShowShortcutOverlay;
	}

	ImGui::EndMainMenuBar();
}

void FEditorMainPanel::RenderMainDockSpace(float ReservedBottomHeight)
{
	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	ImVec2 DockSpaceSize = MainViewport->WorkSize;
	DockSpaceSize.y = max(1.0f, DockSpaceSize.y - ReservedBottomHeight);

	ImGui::SetNextWindowPos(MainViewport->WorkPos);
	ImGui::SetNextWindowSize(DockSpaceSize);
	ImGui::SetNextWindowViewport(MainViewport->ID);

	ImGuiWindowFlags HostWindowFlags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoNavFocus;

	char Label[32];
	std::snprintf(Label, sizeof(Label), "WindowOverViewport_%08X", MainViewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(Label, nullptr, HostWindowFlags);
	ImGui::PopStyleVar(3);

	ImGuiID DockSpaceId = ImGui::GetID("DockSpace");
	ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f));

	ImGui::End();
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
	ImGui::TextUnformatted("Ctrl+A : Select All Actors");
	ImGui::TextUnformatted("Ctrl+D : Duplicate Selected Actors");
	ImGui::TextUnformatted("Ctrl+Z : Undo");
	ImGui::TextUnformatted("Ctrl+Y / Ctrl+Shift+Z : Redo");
	ImGui::Separator();
	ImGui::TextUnformatted("Delete : Delete Selected Actor / Component");
	ImGui::TextUnformatted("` : Focus console input / open console drawer");
	ImGui::TextUnformatted("F5 : Play / Stop PIE");
	ImGui::TextUnformatted("F8 : Release / Capture PIE Input");
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
					// D.3: 컴포넌트가 아닌 POV 통화로 read.
					FMinimalViewInfo POV;
					if (EditorEngine->GetActiveViewportPOV(POV))
					{
						FVector CameraForward = POV.Rotation.GetForwardVector();
						CameraForward.Z = 0.0f;
						if (CameraForward.Length() > 0.0001f)
						{
							CameraForward.Normalize();
							GridForward = CameraForward;
							GridRight = FVector(-CameraForward.Y, CameraForward.X, 0.0f);
						}
						GridOrigin = POV.Location + POV.Rotation.GetForwardVector() * DebugCameraForwardDistance;
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

void FEditorMainPanel::RenderWallRunDebugWindow()
{
	FEditorSettings& Settings = FEditorSettings::Get();

	ImGui::SetNextWindowSize(ImVec2(460.0f, 360.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Wall Run Debug", &Settings.UI.bWallRunDebug))
	{
		ImGui::End();
		return;
	}

	AActor* TargetActor = nullptr;
	UCharacterMovementComponent* Movement = FindWallRunDebugMovement(EditorEngine, TargetActor);
	if (!Movement)
	{
		ImGui::TextDisabled("No CharacterMovementComponent found.");
		ImGui::TextDisabled("Select a character or run a scene with a character.");
		ImGui::End();
		return;
	}

	FWallRunDebugSnapshot Snapshot = Movement->GetWallRunDebugSnapshot();

	ImGui::Text("Target: %s", TargetActor ? TargetActor->GetName().c_str() : "(none)");
	ImGui::Text("Mode: %s", Snapshot.MovementModeName);
	ImGui::SameLine();
	ImGui::TextColored(GetWallRunStatusColor(Snapshot.StatusName), "Status: %s", Snapshot.StatusName);
	ImGui::Separator();

	ImGui::Checkbox("Enable Wall Run", &Movement->bEnableWallRun);
	ImGui::Checkbox("Draw Distance Ring", &Movement->bDrawWallRunDistanceDebug);
	ImGui::Checkbox("UE_LOG Diagnostics", &Movement->bLogWallRunDiagnostics);
	ImGui::Checkbox("Legacy Screen Text", &Movement->bShowWallRunStatusText);

	ImGui::Separator();
	ImGui::Text("Speed: %.2f   Planar: %.2f   Along: %.2f / %.2f",
		Snapshot.Speed,
		Snapshot.PlanarSpeed,
		Snapshot.AlongWallSpeed,
		Snapshot.MinStartSpeed);
	ImGui::Text("Check Distance: %.2f   Sphere Radius: %.2f",
		Snapshot.WallCheckDistance,
		Snapshot.WallCheckSphereRadius);
	ImGui::Text("Elapsed: %.2f / %.2f   Side: %s",
		Snapshot.WallRunElapsedTime,
		Snapshot.MaxWallRunTime,
		Snapshot.bOnRightSide ? "Right" : "Left");

	if (Snapshot.bHasHit)
	{
		ImGui::Text("Hit: Y   Distance: %.2f   UpDot: %.2f",
			Snapshot.HitDistance,
			Snapshot.HitUpDot);
		ImGui::Text("Actor: %s", Snapshot.HitActorName.c_str());
		ImGui::Text("Component: %s", Snapshot.HitComponentName.c_str());
	}
	else
	{
		ImGui::TextDisabled("Hit: N");
	}

	if (ImGui::CollapsingHeader("Vectors", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawDebugVectorLine("Velocity", Snapshot.Velocity);
		DrawDebugVectorLine("Wall Normal", Snapshot.WallNormal);
		DrawDebugVectorLine("Wall Direction", Snapshot.WallDirection);
		DrawDebugVectorLine("Hit Normal", Snapshot.HitNormal);
	}

	if (ImGui::CollapsingHeader("Runtime Flags"))
	{
		ImGui::Text("WallRunEnabled: %s", BoolText(Snapshot.bWallRunEnabled));
		ImGui::Text("IsWallRunning: %s", BoolText(Snapshot.bIsWallRunning));
		ImGui::Text("DrawDistanceDebug: %s", BoolText(Snapshot.bDrawDistanceDebug));
		ImGui::Text("LogDiagnostics: %s", BoolText(Snapshot.bLogDiagnostics));
		ImGui::Text("LegacyScreenText: %s", BoolText(Snapshot.bLegacyScreenText));
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
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.5f);

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
		const bool bFooterConsoleInputActive = ImGui::IsItemActive() || ImGui::IsItemFocused();
		if (bFocusConsoleInputNextFrame)
		{
			ConsoleBacktickCycleState = bConsoleDrawerVisible ? 2 : 1;
		}
		bFocusConsoleInputNextFrame = false;

		ImGui::SameLine();
		ImGui::Text("Domain: %s", EditorEngine && EditorEngine->IsPlayingInEditor() ? "PIE" : "Editor");
		const ImVec2 DomainTextMin = ImGui::GetItemRectMin();
		const ImVec2 DomainTextMax = ImGui::GetItemRectMax();

		const FString LevelLabel = EditorEngine && EditorEngine->HasCurrentLevelFilePath()
			? FString("Level: ") + EditorEngine->GetCurrentLevelFilePath()
			: FString("Level: Unsaved");
		const float LevelWidth = ImGui::CalcTextSize(LevelLabel.c_str()).x;
		const float LevelX = MainViewport->WorkSize.x - ImGui::GetStyle().WindowPadding.x - LevelWidth;

		const FString LatestLog = SanitizeFooterLogMessage(ConsoleWidget.GetLatestLogMessage());
		if (!bFooterConsoleInputActive && !LatestLog.empty())
		{
			const ImVec2 WindowPos = ImGui::GetWindowPos();
			const float LogMinX = DomainTextMax.x + 16.0f;
			const float LogMaxX = WindowPos.x + LevelX - 16.0f;
			if (LogMaxX > LogMinX + 24.0f)
			{
				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				const ImVec2 ClipMin(LogMinX, FooterPos.y);
				const ImVec2 ClipMax(LogMaxX, FooterPos.y + FooterHeight);
				const ImVec2 TextPos(LogMinX, DomainTextMin.y);
				DrawList->PushClipRect(ClipMin, ClipMax, true);
				DrawList->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), LatestLog.c_str());
				DrawList->PopClipRect();
			}
		}

		ImGui::SameLine(LevelX);
		ImGui::TextUnformatted(LevelLabel.c_str());
	}
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void FEditorMainPanel::Update(float DeltaTime)
{
	HandleGlobalShortcuts(DeltaTime);
	ProcessPendingDebugActions();

	ImGuiIO& IO = ImGui::GetIO();

	// GuiState 는 ImGui IO 의 충실한 미러 한 곳뿐.
	// "뷰포트 위면 해제" 핵은 제거 — 입력 소유권은 이제 FSlateApplication 의
	// ImGui 인지 hover 가 단독으로 결정한다.
	InputSystem::Get().GetGuiInputState().bUsingMouse     = IO.WantCaptureMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard  = IO.WantCaptureKeyboard || bShowShortcutOverlay;
	InputSystem::Get().GetGuiInputState().bUsingTextInput = IO.WantTextInput;

	// ImGui 사실을 입력 소유권 중재자에 주입
	FSlateApplication::Get().SetTextInputActive(IO.WantTextInput);

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

void FEditorMainPanel::HandleGlobalShortcuts(float DeltaTime)
{
	if (!EditorEngine)
	{
		ResetGlobalShortcutRepeat();
		return;
	}

	InputSystem& Input = InputSystem::Get();
	if (EditorEngine->ConsumePlayInEditorShortcut(Input.GetKey(VK_F5)))
	{
		// F5는 PIE 입력 캡처 상태나 마우스 위치와 무관하게 Play/Stop을 토글합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		ResetGlobalShortcutRepeat();
		return;
	}

	if (EditorEngine->IsPIEPossessedMode())
	{
		ResetGlobalShortcutRepeat();
		return;
	}

	ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput)
	{
		ResetGlobalShortcutRepeat();
		return;
	}

	if (!EditorEngine->IsPlayingInEditor() && Input.GetKeyDown(VK_DELETE))
	{
		// Delete는 전역 단축키로 처리해 마우스가 UI 패널 위에 있어도 선택 대상 삭제가 동작하게 합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		if (!PropertyWidget.DeleteSelectedComponentWithUndo())
		{
			EditorEngine->DeleteSelectedActorsWithUndo();
		}
		ResetGlobalShortcutRepeat();
		return;
	}

	if (!Input.GetKey(VK_CONTROL))
	{
		ResetGlobalShortcutRepeat();
		return;
	}

	const bool bShift = Input.GetKey(VK_SHIFT);
	if (!EditorEngine->IsPlayingInEditor() && ConsumeGlobalShortcutPressOrRepeat('Z', DeltaTime))
	{
		// Details 패널에서 막 끝난 속성 변경이 있으면 undo 실행 전에 먼저 stack에 확정합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		if (bShift)
		{
			EditorEngine->Redo();
		}
		else
		{
			EditorEngine->Undo();
		}
	}
	else if (!EditorEngine->IsPlayingInEditor() && ConsumeGlobalShortcutPressOrRepeat('Y', DeltaTime))
	{
		// Ctrl+Y도 같은 전역 경로에서 처리해 viewport focus 여부와 무관하게 동작하게 합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		EditorEngine->Redo();
	}
	else if (!EditorEngine->IsPlayingInEditor() && Input.GetKeyDown('D'))
	{
		// 복제도 전역 단축키에서 처리해 마우스가 UI 패널 위에 있어도 동작하게 합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		EditorEngine->DuplicateSelectedActorsWithUndo();
		ResetGlobalShortcutRepeat();
	}
	else if (!EditorEngine->IsPlayingInEditor() && Input.GetKeyDown('A'))
	{
		// Details 패널 편집 직후의 pending undo를 먼저 확정한 뒤 선택 상태를 교체합니다.
		PropertyWidget.FlushPendingDetailsUndoTransaction();
		EditorEngine->GetSelectionManager().SelectAllActors();
		ResetGlobalShortcutRepeat();
	}
	else if (Input.GetKeyDown('N'))
	{
		EditorEngine->NewScene();
		ResetGlobalShortcutRepeat();
	}
	else if (Input.GetKeyDown('O'))
	{
		EditorEngine->LoadSceneWithDialog();
		ResetGlobalShortcutRepeat();
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
		ResetGlobalShortcutRepeat();
	}
}

bool FEditorMainPanel::ConsumeGlobalShortcutPressOrRepeat(int32 KeyCode, float DeltaTime)
{
	InputSystem& Input = InputSystem::Get();
	if (Input.GetKeyDown(KeyCode))
	{
		// 최초 key down 프레임은 즉시 실행하고 이후 반복 입력 타이머를 준비합니다.
		RepeatingShortcutKey = KeyCode;
		RepeatingShortcutElapsed = 0.0f;
		RepeatingShortcutNextFireTime = GEditorShortcutRepeatInitialDelay;
		return true;
	}

	if (!Input.GetKey(KeyCode))
	{
		if (RepeatingShortcutKey == KeyCode)
		{
			ResetGlobalShortcutRepeat();
		}
		return false;
	}

	if (RepeatingShortcutKey != KeyCode)
	{
		// key down edge 없이 이미 눌린 키는 반복 입력으로 시작하지 않습니다.
		return false;
	}

	// 최초 지연 후 일정 간격으로 반복 실행합니다.
	RepeatingShortcutElapsed += DeltaTime;
	if (RepeatingShortcutElapsed < RepeatingShortcutNextFireTime)
	{
		return false;
	}

	do
	{
		RepeatingShortcutNextFireTime += GEditorShortcutRepeatInterval;
	} while (RepeatingShortcutElapsed >= RepeatingShortcutNextFireTime);

	return true;
}

void FEditorMainPanel::ResetGlobalShortcutRepeat()
{
	RepeatingShortcutKey = 0;
	RepeatingShortcutElapsed = 0.0f;
	RepeatingShortcutNextFireTime = 0.0f;
}

void FEditorMainPanel::HideEditorWindows()
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
	Settings.UI.bShadowMapDebug = false;
}

void FEditorMainPanel::ShowEditorWindows()
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

void FEditorMainPanel::HideEditorWindowsForPIE()
{
	HideEditorWindows();
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
	ShowEditorWindows();
}

void FEditorMainPanel::OpenAssetEditorForObject(UObject* Object)
{
	AssetEditorManager.OpenEditorForObject(Object);
}

void FEditorMainPanel::OpenUIEditor(const std::filesystem::path& Path)
{
	UIEditorWidget.Open(Path);
}
