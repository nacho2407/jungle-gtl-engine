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

#include <utility>

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

	// Lifecycle overrides
	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;
	virtual void WorldTick(float DeltaTime) override;

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }

	// 퍼스펙티브 카메라(인덱스 0)를 반환합니다.
	FViewportCamera* GetCamera();
	const FViewportCamera* GetCamera() const;

	void ClearScene();
	int32 DeleteActors(const TArray<AActor*>& Actors);

	FEditorUndoSystem& GetUndoSystem() { return UndoSystem; }
	const FEditorUndoSystem& GetUndoSystem() const { return UndoSystem; }
	void ResetViewport();
	void CloseScene();
	void NewScene();

	bool CreateDefaultSceneAsset(const FString& FilePath);

	virtual void SetActiveWorld(const FName& Handle) override;
	void ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld = nullptr);

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

	// 포커스된 뷰포트가 참조하는 월드를 반환합니다.
	// 편집 중이면 에디터 월드, PIE 중이면 PIE 월드가 됩니다.
	UWorld* GetFocusedWorld() const
	{
        const FEditorViewportClient* FocusedClient =
            ViewportLayout.GetViewportClient(ViewportLayout.GetLastFocusedViewportIndex());
        return FocusedClient ? FocusedClient->GetFocusedWorld() : nullptr;
	}

	// 주의! Editor State가 따로 존재하는 것이 아닙니다. 에디터가 현재 포커스한 뷰포트의 상태를 Get/Set합니다.
	EViewportPlayState GetEditorState() const
	{
        const int32 StateViewportIndex =
            PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
        const FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(StateViewportIndex);
        return FocusedClient ? FocusedClient->GetPlayState() : EViewportPlayState::Editing;
	}

	void SetEditorState(EViewportPlayState InState)
	{
        const int32 StateViewportIndex =
            PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
        if (FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(StateViewportIndex))
        {
            FocusedClient->SetPlayState(InState);
        }
	}

	// PIE 모드 컨트롤 함수
	void StartPlaySession();
	void PausePlaySession();
	void ResumePlaySession();
	void StopPlaySession();

	FWorldContext& RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handler, const FString& Name);
	void UnregisterWorld(const FName& Handle);
	FName GetEditorWorldHandle() const;

private:
	friend class FEditorUndoSystem;

	void ProcessQueuedPlaySessionRequests();
	void StartPlaySessionNow();
	void StopPlaySessionNow();
	APlayerController* SpawnPIEPlayerController(UWorld* PIEWorld, FEditorViewportClient* FocusedClient);
	FString CaptureSceneSnapshot() const;
	bool RestoreSceneSnapshot(const FString& Snapshot);
	void OnSceneWorldWillUnload(UWorld* OldWorld) override;
	void OnSceneWorldLoaded(UWorld* NewWorld) override;

	FSelectionManager SelectionManager;
    
	FEditorMainPanel  MainPanel;
	FEditorViewportLayout   ViewportLayout;

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
