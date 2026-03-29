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

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	void Shutdown() override;
	void SetSelectedActor(AActor* InActor);
	AActor* GetSelectedActor() const;
	void ActivateEditorScene();
	bool ActivatePreviewScene(const FString& ContextName);
	UScene* GetEditorScene() const;
	UScene* GetPreviewScene(const FString& ContextName) const;
	UWorld* GetEditorWorld() const;
	const TArray<FWorldContext*>& GetPreviewWorldContexts() const;
	FWorldContext* CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height);
	UScene* GetScene() const override;
	UScene* GetActiveScene() const override;
	UWorld* GetActiveWorld() const override;
	const FWorldContext* GetActiveWorldContext() const override;
	void HandleResize(int32 Width, int32 Height) override;

	const TArray<FViewport>& GetViewports() const { return ViewportRegistry.GetViewports(); }
	TArray<FViewport>& GetViewports() { return ViewportRegistry.GetViewports(); }
	const FEditorViewportRegistry& GetViewportRegistry() const { return ViewportRegistry; }
	FEditorViewportRegistry& GetViewportRegistry() { return ViewportRegistry; }
	FSlateApplication* GetSlateApplication() const { return SlateApplication.get(); }
	void FlushDebugDrawForViewport(FRenderer* Renderer, const FShowFlags& ShowFlags, bool bClearAfterFlush);
	void ClearDebugDrawForFrame();

protected:
	void PreInitialize() override;
	// 에디터 UI가 나중에 사용할 메인 창 참조만 저장한다.
	void BindHost(FWindowsWindow* InMainWindow) override;
	bool InitializeWorlds(int32 Width, int32 Height) override;
	// 에디터 전용 초기화는 이 단계 하나로 모은 뒤 내부 단계로 다시 나눈다.
	bool InitializeMode() override;
	void FinalizeInitialize() override;
	void Tick(float DeltaTime) override;
	void TickWorlds(float DeltaTime) override;
	bool WantsPhysicsDebugVisualization() const override { return true; }
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
	void RenderFrame() override;

	FEditorViewportController* GetViewportController();
	FViewport* FindViewport(FViewportId Id);

private:
	// 프리뷰 씬/프리뷰 뷰포트 준비
	bool InitEditorPreview();
	// 에디터 콘솔 UI와 콘솔 변수 시스템 연결
	void InitEditorConsole();
	// 에디터 카메라와 입력 컨트롤러 준비
	bool InitEditorCamera();
	// 현재 활성 월드 타입에 맞는 뷰포트 클라이언트 선택
	void InitEditorViewportRouting();
	bool InitEditorWorlds(int32 Width, int32 Height);
	void ReleaseEditorWorlds();
	FWorldContext* FindPreviewWorld(const FString& ContextName);
	const FWorldContext* FindPreviewWorld(const FString& ContextName) const;
	void UpdateEditorWorldAspectRatio(float AspectRatio);
	void SyncViewportClient();

	FEditorUI EditorUI;
	std::unique_ptr<FPreviewViewportClient> PreviewViewportClient;
	FEditorSelectionSubsystem SelectionSubsystem;
	FEditorCameraSubsystem CameraSubsystem;
	FWorldContext* EditorWorldContext = nullptr;
	TArray<FWorldContext*> PreviewWorldContexts;
	FWorldContext* ActiveEditorWorldContext = nullptr;

	FWindowsWindow* MainWindow = nullptr;
	FEditorViewportRegistry ViewportRegistry;
	FEditorViewportClient* EditorViewportClientRaw = nullptr;

	std::unique_ptr<FSlateApplication> SlateApplication = nullptr;
};
