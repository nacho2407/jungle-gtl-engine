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

	UGizmoComponent* GetGizmo() const;

	// 퍼스펙티브 카메라(인덱스 0)를 반환합니다.
	FViewportCamera* GetCamera();
	const FViewportCamera* GetCamera() const;

	UWorld* GetFocusedWorld() const;
	EViewportPlayState GetEditorState() const;
	void SetEditorState(EViewportPlayState InState);

	void ClearScene();
	int32 DeleteActors(const TArray<AActor*>& Actors);
	void ResetViewport();
	void CloseScene();
	void NewScene();

	bool CreateDefaultSceneAsset(const FString& FilePath);

	void SetActiveWorld(const FName& Handle) override;
	void ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld = nullptr);

	FEditorUndoSystem& GetUndoSystem() { return UndoSystem; }
	const FEditorUndoSystem& GetUndoSystem() const { return UndoSystem; }

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }
	const FSelectionManager& GetSelectionManager() const { return SelectionManager; }

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

	FEditorViewer& GetViewer() { return Viewer; }

private:
	friend class FEditorUndoSystem;

	void ProcessQueuedPlaySessionRequests();
	void StartPlaySessionNow();
	void StopPlaySessionNow();
	APlayerController* SpawnPIEPlayerController(
		UWorld* PIEWorld,
		FEditorViewportClient* FocusedClient
	);
	FString CaptureSceneSnapshot() const;
	bool RestoreSceneSnapshot(const FString& Snapshot);
	void OnSceneWorldWillUnload(UWorld* OldWorld) override;
	void OnSceneWorldLoaded(UWorld* NewWorld) override;

	FSelectionManager SelectionManager;

	FEditorMainPanel MainPanel;
	FEditorViewportLayout ViewportLayout;
    FEditorViewer Viewer;	

	FInputPolicyRouter EditorInputRouter;
	FPIESession PIESession;

	bool bStartPlaySessionQueued = false;
	bool bStopPlaySessionQueued = false;

	int32 ActorDestroyedListenerId = 0;
	UWorld* ActorDestroyedListenerWorld = nullptr;

	FEditorUndoSystem UndoSystem;

private:
	void HandleActorDestroyed(AActor* Actor);
	void BindActorDestroyedListener(UWorld* World);
	void UnbindActorDestroyedListener(UWorld* World);
};
