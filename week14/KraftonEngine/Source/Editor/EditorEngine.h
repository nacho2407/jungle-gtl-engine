#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/Level/FLevelViewportLayout.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/PIE/PIETypes.h"
#include <filesystem>
#include "Editor/Undo/EditorUndoManager.h"
#include <optional>
#include <memory>
#if STATS
#include "Editor/EditorRenderPipeline.h"
#endif
#include "Source/Editor/EditorEngine.generated.h"

class UGizmoComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;
class AActor;
class UGameViewportClient;
class IEditorPreviewViewportClient;
class FViewport;
struct FPerspectiveCameraData;

UCLASS()
class UEditorEngine : public UEngine
{
public:
	GENERATED_BODY()
	UEditorEngine() = default;
	~UEditorEngine() override = default;

	// Lifecycle overrides
	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }

	// 활성 뷰포트의 카메라 POV 통화. D.3 부터 외부에 노출되는 카메라 API 는 이것뿐.
	// 활성 뷰포트가 없으면 false 반환.
	bool GetActiveViewportPOV(struct FMinimalViewInfo& OutPOV) const;

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();
	bool LoadSceneWithDialog();
	bool LoadSceneFromPath(const FString& InScenePath);
	bool SaveScene();
	bool SaveSceneAsWithDialog();
	bool SaveSceneAs(const FString& InSceneName);
	bool HasCurrentLevelFilePath() const { return !CurrentLevelFilePath.empty(); }
	const FString& GetCurrentLevelFilePath() const { return CurrentLevelFilePath; }
	void RefreshContentBrowser() { MainPanel.RefreshContentBrowser(); }
	void OpenAssetEditorForObject(UObject* Object) { MainPanel.OpenAssetEditorForObject(Object); }
	void OpenUIEditor(const std::filesystem::path& Path) { MainPanel.OpenUIEditor(Path); }
	void SetContentBrowserIconSize(float Size) { MainPanel.SetContentBrowserIconSize(Size); }
	float GetContentBrowserIconSize() const { return MainPanel.GetContentBrowserIconSize(); }
	void HideEditorWindows() { MainPanel.HideEditorWindows(); }
	void ShowEditorWindows() { MainPanel.ShowEditorWindows(); }
	bool AreEditorWindowsHidden() const { return MainPanel.AreEditorWindowsHidden(); }
	void SetShowEditorOnlyComponents(bool bEnable) { MainPanel.SetShowEditorOnlyComponents(bEnable); }
	bool IsShowingEditorOnlyComponents() const { return MainPanel.IsShowingEditorOnlyComponents(); }
	bool IsWorldCoordSystem() const { return FEditorSettings::Get().LevelViewportSettings[0].Gizmo.CoordSystem == EEditorCoordSystem::World; }
	void ToggleCoordSystem();
	void ApplyTransformSettingsToGizmo();

	/**
	 * @brief 마지막 에디터 편집 명령을 되돌립니다.
	 *
	 * @return undo 실행 여부
	 */
	bool Undo();

	/**
	 * @brief 마지막 undo 명령을 다시 실행합니다.
	 *
	 * @return redo 실행 여부
	 */
	bool Redo();

	bool CanUndo() const { return UndoManager.CanUndo(); }
	bool CanRedo() const { return UndoManager.CanRedo(); }
	bool IsApplyingUndoRedo() const { return UndoManager.IsApplying(); }

	/**
	 * @brief 이미 실행된 에디터 편집 명령을 undo stack에 등록합니다.
	 *
	 * @param Command 등록할 undo command
	 */
	void PushExecutedUndoCommand(std::unique_ptr<IEditorUndoCommand> Command);

	/**
	 * @brief 선택된 actor들을 undo 기록과 함께 삭제합니다.
	 *
	 * @return 삭제된 actor 수
	 */
	int32 DeleteSelectedActorsWithUndo();

	/**
	 * @brief 선택된 actor들을 undo 기록과 함께 복제합니다.
	 *
	 * @return 복제된 actor 수
	 */
	int32 DuplicateSelectedActorsWithUndo();

	/**
	 * @brief 모든 undo/redo 기록을 비웁니다.
	 */
	void ClearUndoHistory();

	// GPU Occlusion readback 스테이징 데이터 무효화 — 액터 삭제 시 dangling proxy 방지
	void InvalidateOcclusionResults() { if (auto* P = GetRenderPipeline()) P->OnSceneCleared(); }

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	// 레이아웃에 위임
	const TArray<FEditorViewportClient*>& GetAllViewportClients() const { return ViewportLayout.GetAllViewportClients(); }
	const TArray<FLevelEditorViewportClient*>& GetLevelViewportClients() const { return ViewportLayout.GetLevelViewportClients(); }
	bool ShouldRenderViewportClient(const FLevelEditorViewportClient* ViewportClient) const { return ViewportLayout.ShouldRenderViewportClient(ViewportClient); }

	void SetActiveViewport(FLevelEditorViewportClient* InClient) { ViewportLayout.SetActiveViewport(InClient); }
	FLevelEditorViewportClient* GetActiveViewport() const { return ViewportLayout.GetActiveViewport(); }

	void CollectAssetEditorPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const { MainPanel.CollectAssetEditorPreviewViewportClients(OutClients); }

	void ToggleViewportSplit() { ViewportLayout.ToggleViewportSplit(); }
	bool IsSplitViewport() const { return ViewportLayout.IsSplitViewport(); }

	void RenderViewportUI(float DeltaTime) { ViewportLayout.RenderViewportUI(DeltaTime); }
	AActor* SpawnPlaceActor(FLevelViewportLayout::EViewportPlaceActorType Type, const FVector& Location)
	{
		return ViewportLayout.SpawnPlaceActor(Type, Location);
	}

	bool IsMouseOverViewport() const { return ViewportLayout.IsMouseOverViewport(); }

	void RenderUI(float DeltaTime);

	FOverlayStatSystem& GetOverlayStatSystem() { return OverlayStatSystem; }
	const FOverlayStatSystem& GetOverlayStatSystem() const { return OverlayStatSystem; }

	void SetAutoGCEnabled(bool bEnabled);
	bool IsAutoGCEnabled() const { return bAutoGCEnabled; }
	void SetGCIntervalSeconds(float Seconds);
	float GetGCIntervalSeconds() const { return GCIntervalSeconds; }
	void ForceCollectGarbage();

	// --- PIE (Play In Editor) ---
	// UE의 FRequestPlaySessionParams 대응. 요청은 단일 슬롯에 저장되고
	// 다음 Tick에서 StartQueuedPlaySessionRequest가 실제 StartPIE를 수행한다.
	void RequestPlaySession(const FRequestPlaySessionParams& InParams);
	void CancelRequestPlaySession();
	bool HasPlaySessionRequest() const { return PlaySessionRequest.has_value(); }

	void RequestEndPlayMap();
	/**
	 * @brief F5 단축키 입력으로 PIE 시작 또는 종료를 요청합니다.
	 */
	void TogglePlayInEditorShortcut();

	/**
	 * @brief F5 키 상태를 소비해 PIE 시작 또는 종료를 한 번만 토글합니다.
	 *
	 * @param bF5Down 현재 F5 키 눌림 상태
	 *
	 * @return F5 입력 소비 여부
	 */
	bool ConsumePlayInEditorShortcut(bool bF5Down);

	/**
	 * @brief PIE 게임 카메라는 유지한 채 게임 입력 캡처만 토글합니다.
	 *
	 * @return 토글 처리 여부
	 */
	bool TogglePIEInputCapture();

	bool IsPlayingInEditor() const { return PlayInEditorSessionInfo.has_value(); }
	enum class EPIEControlMode : uint8
	{
		Possessed,
		Ejected
	};
	EPIEControlMode GetPIEControlMode() const { return PIEControlMode; }
	bool IsPIEPossessedMode() const { return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Possessed; }
	bool IsPIEEjectedMode() const { return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Ejected; }
	bool TogglePIEControlMode();

	/**
	 * @brief 현재 고정된 PIE 게임 뷰포트 client를 반환합니다.
	 *
	 * @return 현재 PIE 게임 뷰포트 client. PIE가 아니거나 뷰포트가 유효하지 않으면 nullptr
	 */
	FLevelEditorViewportClient* GetPIEGameViewportClient() const;

	/**
	 * @brief 지정한 viewport가 현재 PIE 게임 뷰포트인지 확인합니다.
	 *
	 * @param Viewport 판정할 viewport
	 *
	 * @return 현재 PIE 게임 뷰포트이면 true
	 */
	bool IsPIEGameViewport(const FViewport* Viewport) const;

	/**
	 * @brief PIE 게임 입력 캡처가 현재 켜져 있는지 확인합니다.
	 *
	 * @return PIE 중이고 GameViewportClient가 입력 possession 상태이면 true
	 */
	bool IsPIEInputCaptured() const;

	// 즉시 동기 종료 — Save / NewScene / Load 등 에디터 월드를 만지는 작업 직전에 호출.
	// PIE 중이 아니면 no-op.
	void StopPlayInEditorImmediate() { if (IsPlayingInEditor()) EndPlayMap(); }

	// PIE 안에서 Lua 가 Engine.TransitionToScene 호출 시 다음 frame 경계에서 PIE world를 교체.
	void RequestTransitionToScene(const FString& InScenePath) override;
	void RequestExit() override;

	//GC
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	// Tick 내에서 호출 — 큐에 요청이 있으면 StartPlayInEditorSession 실행
	void StartQueuedPlaySessionRequest();
	void StartPlayInEditorSession(const FRequestPlaySessionParams& Params);
	void EndPlayMap();
	bool EnterPIEPossessedMode();
	bool EnterPIEEjectedMode();
	FLevelEditorViewportClient* ResolvePIEGameViewportClient() const;
	void SyncGameViewportPIEControlState(bool bPossessedMode);
	void LoadStartLevel();
	bool FindSceneViewportPOV(struct FMinimalViewInfo& OutPOV) const;
	void RestoreViewportCamera(const FPerspectiveCameraData& CamData);
	FString ResolveSceneFilePath(const FString& InNameOrPath) const;
	void ProcessPendingPIESceneTransition();
	void RunGarbageCollectionPass();

	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FLevelViewportLayout ViewportLayout;
	FOverlayStatSystem OverlayStatSystem;
	FEditorUndoManager UndoManager;

	// PIE 요청 단일 슬롯 (UE TOptional<FRequestPlaySessionParams>).
	std::optional<FRequestPlaySessionParams> PlaySessionRequest;
	// 활성 PIE 세션 정보. has_value() == IsPlayingInEditor().
	std::optional<FPlayInEditorSessionInfo> PlayInEditorSessionInfo;
	// 종료 요청 지연 플래그. Tick 선두에서 확인 후 EndPlayMap 호출.
	bool bRequestEndPlayMapQueued = false;
	bool bPendingPIESceneTransition = false;
	FString PendingPIEScenePath;
	EPIEControlMode PIEControlMode = EPIEControlMode::Possessed;
	/**
	 * @brief F5 키 hold 중 PIE 토글 재실행 차단 latch
	 */
	bool bPIEPlayShortcutConsumedWhileHeld = false;
	FString CurrentLevelFilePath;
	bool bAutoGCEnabled = false;
	float GCIntervalSeconds = 1.0f;
	float GCTimeAccumulator = 0.0f;

};
