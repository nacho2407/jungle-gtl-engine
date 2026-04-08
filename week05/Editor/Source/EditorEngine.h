#pragma once

#include "Core/Engine.h"
#include "Subsystem/EditorCameraSubsystem.h"
#include "Subsystem/EditorSelectionSubsystem.h"
#include "UI/EditorUI.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Viewport/PreviewViewportClient.h"
#include "Slate/SlateApplication.h"

class AActor;
class FEditorViewportClient;
class FShowFlags;

/**
 * 에디터 모드용 엔진 진입점이다.
 * 기본 엔진 프레임 루프 위에 에디터 월드, 프리뷰 월드, Slate UI,
 * 에디터 뷰포트 라우팅과 선택/카메라 서브시스템을 얹어 관리한다.
 */
class FEditorEngine : public FEngine
{
public:
	enum class ESimulationPlaybackState : uint8
	{
		Playing,
		Paused,
		Stopped
	};

	FEditorEngine() = default;
	~FEditorEngine() override;

	// 에디터 전용 서브시스템과 월드, UI 상태를 종료 순서에 맞게 정리한다.
	void Shutdown() override;
	// 현재 선택된 액터를 변경한다.
	void SetSelectedActor(AActor* InActor);
	// 현재 선택된 액터를 반환한다.
	AActor* GetSelectedActor() const;
	// 에디터 월드 시뮬레이션 재생 상태를 재생으로 전환한다.
	void PlaySimulation();
	// 에디터 월드 시뮬레이션 재생 상태를 일시정지로 전환한다.
	void PauseSimulation();
	// 에디터 월드 시뮬레이션 재생 상태를 정지로 전환한다.
	void StopSimulation();
	// 현재 에디터 월드 시뮬레이션 재생 상태를 반환한다.
	ESimulationPlaybackState GetSimulationPlaybackState() const;
	// 활성 월드를 에디터 월드로 전환한다.
	void ActivateEditorScene();
	// 이름으로 지정한 프리뷰 월드를 활성 월드로 전환한다.
	bool ActivatePreviewScene(const FString& ContextName);
	// 에디터 메인 씬을 반환한다.
	ULevel* GetEditorScene() const;
	// 이름에 대응하는 프리뷰 씬을 반환한다.
	ULevel* GetPreviewScene(const FString& ContextName) const;
	// 에디터 메인 월드를 반환한다.
	UWorld* GetEditorWorld() const;
	// 현재 생성된 프리뷰 월드 컨텍스트 목록을 반환한다.
	const TArray<FWorldContext*>& GetPreviewWorldContexts() const;
	// 새 프리뷰 월드 컨텍스트를 만들거나 기존 것을 재사용한다.
	FWorldContext* CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height);
	// 현재 활성 씬을 반환한다.
	ULevel* GetScene() const override;
	// 에디터/프리뷰 상태를 반영한 활성 씬을 반환한다.
	ULevel* GetActiveScene() const override;
	// 에디터/프리뷰 상태를 반영한 활성 월드를 반환한다.
	UWorld* GetActiveWorld() const override;
	// 현재 활성 월드 컨텍스트를 반환한다.
	const FWorldContext* GetActiveWorldContext() const override;
	// 창 크기 변경을 에디터/프리뷰 월드 카메라까지 반영한다.
	void HandleResize(int32 Width, int32 Height) override;

	const TArray<FViewport>& GetViewports() const { return ViewportRegistry.GetViewports(); }
	TArray<FViewport>& GetViewports() { return ViewportRegistry.GetViewports(); }
	const FEditorViewportRegistry& GetViewportRegistry() const { return ViewportRegistry; }
	FEditorViewportRegistry& GetViewportRegistry() { return ViewportRegistry; }
	// 현재 Slate 애플리케이션 인스턴스를 반환한다.
	FSlateApplication* GetSlateApplication() const { return SlateApplication.get(); }
	// 현재 에디터 show flag와 선택 상태를 반영한 디버그 라인 요청을 만든다.
	void BuildDebugLineRenderRequest(const FShowFlags& ShowFlags, FDebugLineRenderRequest& OutRequest);
	// 프레임 동안 누적된 디버그 드로우 데이터를 비운다.
	void ClearDebugDrawForFrame();
	// 초기 에디터 오버레이 UI를 생성한다.
	void CreateInitUI();

	bool IsPIEActive() const { return bIsPIEActive; }
	bool IsPIEPaused() const { return bIsPIEPaused; }
	bool StartPIE();
	void EndPIE();
	void TogglePIEPause();

protected:
	// 에디터 런타임 초기화 전에 ImGui/로그 라우팅을 준비한다.
	void PreInitialize() override;
	// 메인 윈도우와 EditorUI를 연결한다.
	void BindHost(FWindowsWindow* InMainWindow) override;
	// 에디터 월드와 프리뷰 월드에 필요한 컨텍스트를 만든다.
	bool InitializeWorlds() override;
	// 프리뷰, 카메라, 콘솔, 뷰포트 라우팅을 초기화한다.
	bool InitializeMode() override;
	// Slate와 초기 UI를 만들어 에디터 화면 구성을 완료한다.
	void FinalizeInitialize() override;
	// 프레임 시작 시 활성 뷰포트와 카메라 서브시스템 상태를 맞춘다.
	void PrepareFrame(float DeltaTime) override;
	// 현재 활성 월드 하나를 에디터 프레임 기준으로 Tick한다.
	void TickWorlds(float DeltaTime) override;
	bool WantsPhysicsDebugVisualization() const override { return true; }
	// 에디터 기본 ViewportClient를 생성한다.
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
	// 에디터 한 프레임의 UI/뷰포트 렌더 순서를 실행한다.
	void RenderFrame() override;
	// 플랫폼 커서 상태를 Slate 결과에 맞춰 동기화한다.
	void SyncPlatformState() override;

	// 에디터 카메라 서브시스템의 뷰포트 컨트롤러를 반환한다.
	FEditorViewportController* GetViewportController();
	// 뷰포트 ID로 활성 뷰포트를 찾는다.
	FViewport* FindViewport(FViewportId Id);

private:
	struct FPIEViewportStateBackup
	{
		FViewportId ViewportId = INVALID_VIEWPORT_ID;
		FViewportLocalState LocalState;
	};

	// 기본 프리뷰 월드와 프리뷰 클라이언트를 준비한다.
	bool InitEditorPreview();
	// 에디터 콘솔 명령 라우팅을 연결한다.
	void InitEditorConsole();
	// 에디터 카메라 서브시스템을 초기화한다.
	bool InitEditorCamera();
	// 현재 포커스와 레이아웃 기준으로 뷰포트 라우팅을 맞춘다.
	void InitEditorViewportRouting();
	// 에디터 월드 컨텍스트를 생성한다.
	bool InitEditorWorlds();
	// 에디터와 프리뷰 월드 컨텍스트를 모두 정리한다.
	void ReleaseEditorWorlds();
	// 이름으로 프리뷰 월드 컨텍스트를 찾는다.
	FWorldContext* FindPreviewWorld(const FString& ContextName);
	// 이름으로 상수 프리뷰 월드 컨텍스트를 찾는다.
	const FWorldContext* FindPreviewWorld(const FString& ContextName) const;
	// 에디터와 모든 프리뷰 월드의 카메라 종횡비를 갱신한다.
	void UpdateEditorWorldAspectRatio(float AspectRatio);
	// 현재 포커스된 뷰포트의 로컬 상태를 카메라 서브시스템에 반영한다.
	void SyncFocusedViewportLocalState();
	// Slate 커서 상태를 운영체제 커서에 반영한다.
	void SyncPlatformCursor();
	// 활성 월드 종류에 따라 Editor/Preview ViewportClient를 전환한다.
	void SyncViewportClient();
	// 선택된 액터의 BVH를 디버그 라인 요청에 추가한다.
	void AppendSelectedBVH(FDebugLineRenderRequest& InOutRequest) const;

	FEditorUI EditorUI;
	std::unique_ptr<FPreviewViewportClient> PreviewViewportClient;
	FEditorSelectionSubsystem SelectionSubsystem;
	FEditorCameraSubsystem CameraSubsystem;

	FWorldContext* EditorWorldContext = nullptr;
	FWorldContext* PIEWorldContext = nullptr;
	TArray<FWorldContext*> PreviewWorldContexts;

	FWorldContext* ActiveWorldContext = nullptr;
	FWorldContext* PrePIEActiveWorldContext = nullptr;

	FWindowsWindow* MainWindow = nullptr;
	FEditorViewportRegistry ViewportRegistry;
	FEditorViewportClient* EditorViewportClientRaw = nullptr;

	std::unique_ptr<FSlateApplication> SlateApplication = nullptr;
	TArray<FPIEViewportStateBackup> SavedPIEViewportStates;
	TObjectPtr<AActor> SavedPIESelectedActor;
	bool bWasCursorHiddenForPIE = false;
	bool bIsPIEActive = false;
	bool bIsPIEPaused = false;
};
