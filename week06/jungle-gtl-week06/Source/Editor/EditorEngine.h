#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/PIE/PIETypes.h"
#include <optional>
#if STATS
#include "Editor/EditorRenderPipeline.h"
#endif

class UGizmoComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;

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

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }
	UCameraComponent* GetCamera() const;

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	// 레이아웃에 위임
	const TArray<FEditorViewportClient*>& GetAllViewportClients() const { return ViewportLayout.GetAllViewportClients(); }
	const TArray<FLevelEditorViewportClient*>& GetLevelViewportClients() const { return ViewportLayout.GetLevelViewportClients(); }

	void SetActiveViewport(FLevelEditorViewportClient* InClient) { ViewportLayout.SetActiveViewport(InClient); }
	FLevelEditorViewportClient* GetActiveViewport() const { return ViewportLayout.GetActiveViewport(); }

	void ToggleViewportSplit() { ViewportLayout.ToggleViewportSplit(); }
	bool IsSplitViewport() const { return ViewportLayout.IsSplitViewport(); }

	void RenderViewportUI(float DeltaTime) { ViewportLayout.RenderViewportUI(DeltaTime); }

	bool IsMouseOverViewport() const { return ViewportLayout.IsMouseOverViewport(); }

	void RenderUI(float DeltaTime);

	FOverlayStatSystem& GetOverlayStatSystem() { return OverlayStatSystem; }
	const FOverlayStatSystem& GetOverlayStatSystem() const { return OverlayStatSystem; }

	// --- PIE (Play In Editor) ---
	// UE의 FRequestPlaySessionParams 대응. 요청은 단일 슬롯에 저장되고
	// 다음 Tick에서 StartQueuedPlaySessionRequest가 실제 StartPIE를 수행한다.
	void RequestPlaySession(const FRequestPlaySessionParams& InParams);
	void CancelRequestPlaySession();
	bool HasPlaySessionRequest() const { return PlaySessionRequest.has_value(); }

	void RequestEndPlayMap();
	bool IsPlayingInEditor() const { return PlayInEditorSessionInfo.has_value(); }

	// 즉시 동기 종료 — Save / NewScene / Load 등 에디터 월드를 만지는 작업 직전에 호출.
	// PIE 중이 아니면 no-op.
	void StopPlayInEditorImmediate() { if (IsPlayingInEditor()) EndPlayMap(); }

#if STATS
	FGPUOcclusionCulling* GetGPUOcclusion()
	{
		auto* Pipeline = static_cast<FEditorRenderPipeline*>(GetRenderPipeline());
		return Pipeline ? &Pipeline->GetGPUOcclusion() : nullptr;
	}
#endif

private:
	// Tick 내에서 호출 — 큐에 요청이 있으면 StartPlayInEditorSession 실행
	void StartQueuedPlaySessionRequest();
	void StartPlayInEditorSession(const FRequestPlaySessionParams& Params);
	void EndPlayMap();

	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FLevelViewportLayout ViewportLayout;
	FOverlayStatSystem OverlayStatSystem;

	// PIE 요청 단일 슬롯 (UE TOptional<FRequestPlaySessionParams>).
	std::optional<FRequestPlaySessionParams> PlaySessionRequest;
	// 활성 PIE 세션 정보. has_value() == IsPlayingInEditor().
	std::optional<FPlayInEditorSessionInfo> PlayInEditorSessionInfo;
	// 종료 요청 지연 플래그. Tick 선두에서 확인 후 EndPlayMap 호출.
	bool bRequestEndPlayMapQueued = false;
};
