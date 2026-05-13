#pragma once

#include "Engine/Runtime/Engine.h"

#include "Engine/Input/InputRouter.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/PIE/PIESession.h"
#include "Editor/Asset/EditorAssetService.h"
#include "Editor/Command/EditorCommandSystem.h"
#include "Editor/Notification/EditorNotificationService.h"
#include "Editor/Scene/EditorSceneService.h"
#include "Editor/Undo/EditorUndoSystem.h"
#include "Camera/ViewportCamera.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Editor/Viewer/EditorViewer.h"

class UGizmoComponent;
class FEditorRenderPipeline;
class AActor;
class APlayerController;

class UEditorEngine : public UEngine
{
public:
	DECLARE_CLASS(UEditorEngine, UEngine)

	UEditorEngine() = default;
	~UEditorEngine() override = default;

	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;
	bool CanCloseApplication() override;
	void WorldTick(float DeltaTime) override;

    FEditorViewer* CreateViewer(FString InFileName);
    void RemoveViewer(FEditorViewer* InViewer);

	// 퍼스펙티브 카메라(인덱스 0)를 반환합니다.
	FViewportCamera* GetCamera();
	const FViewportCamera* GetCamera() const;

	UWorld* GetFocusedWorld() const;
    FWorldContext* GetFocusedWorldContext();
	EViewportPlayState GetEditorState() const;
	void SetEditorState(EViewportPlayState InState);

	void ClearScene();
	int32 DeleteActors(const TArray<AActor*>& Actors);
	void ResetViewport();
	void CloseScene();

	void SetActiveWorld(const FName& Handle) override;
	void ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld = nullptr);

	FEditorUndoSystem& GetUndoSystem() { return UndoSystem; }
	const FEditorUndoSystem& GetUndoSystem() const { return UndoSystem; }
	FEditorCommandSystem& GetCommandSystem() { return CommandSystem; }
	const FEditorCommandSystem& GetCommandSystem() const { return CommandSystem; }
	FEditorAssetService& GetAssetService() { return AssetService; }
	const FEditorAssetService& GetAssetService() const { return AssetService; }
	FEditorNotificationService& GetNotificationService() { return NotificationService; }
	const FEditorNotificationService& GetNotificationService() const { return NotificationService; }
	FEditorSceneService& GetSceneService() { return SceneService; }
	const FEditorSceneService& GetSceneService() const { return SceneService; }

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FEditorViewportLayout& GetViewportLayout() { return ViewportLayout; }
	const FEditorViewportLayout& GetViewportLayout() const { return ViewportLayout; }

	FEditorRenderPipeline* GetEditorRenderPipeline() const;

	FEditorMainPanel& GetMainPanel() { return MainPanel; }

	void RenderUI(float DeltaTime);
	void RegisterViewportInputTargets();

	void RequestPIEViewportInputFocus(int32 FrameCount = 3);
	FPIESession& GetPIESession() { return PIESession; }
	const FPIESession& GetPIESession() const { return PIESession; }

	void StartPlaySession();
	void PausePlaySession();
	void ResumePlaySession();
	void StopPlaySession();

	FWorldContext& RegisterWorld(
		UWorld* InWorld,
		EWorldType Type,
		const FName& Handle,
		const FString& Name
	);
	void UnregisterWorld(const FName& Handle);
	FName GetEditorWorldHandle() const;

	const TArray<std::unique_ptr<FEditorViewer>>& GetViewers() const { return Viewers; }
    TArray<std::unique_ptr<FEditorViewer>>& GetViewers() { return Viewers; }

private:
	friend class FEditorUndoSystem;
	friend class FEditorSceneService;

	void ProcessQueuedPlaySessionRequests();
	void StartPlaySessionNow();
	void StopPlaySessionNow();
	void NewScene();
	bool CreateDefaultSceneAsset(const FString& FilePath);
	APlayerController* SpawnPIEPlayerController(
		UWorld* PIEWorld,
		FEditorViewportClient* FocusedClient
	);
	FString CaptureSceneSnapshot() const;
	bool RestoreSceneSnapshot(const FString& Snapshot, const FName& RestoreWorldHandle = FName::None);
	void OnSceneWorldWillUnload(UWorld* OldWorld) override;
	void OnSceneWorldLoaded(UWorld* NewWorld) override;

	FEditorMainPanel MainPanel;
	FEditorViewportLayout ViewportLayout;
    TArray<std::unique_ptr<FEditorViewer>> Viewers;	

	FInputPolicyRouter EditorInputRouter;
	FPIESession PIESession;
	FEditorCommandSystem CommandSystem;
	FEditorAssetService AssetService;
	FEditorNotificationService NotificationService;
	FEditorSceneService SceneService;

    FEditorUndoSystem UndoSystem;

	bool bStartPlaySessionQueued = false;
	bool bStopPlaySessionQueued = false;

	int32 ActorDestroyedListenerId = 0;
	UWorld* ActorDestroyedListenerWorld = nullptr;

private:
	void HandleActorDestroyed(AActor* Actor);
	void BindActorDestroyedListener(UWorld* World);
	void UnbindActorDestroyedListener(UWorld* World);
};
