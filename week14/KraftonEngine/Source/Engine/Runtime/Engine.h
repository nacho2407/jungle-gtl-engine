#pragma once

#include "Object/Object.h"
#include "Core/Types/EngineTypes.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Pipeline/IRenderPipeline.h"

#include "Source/Engine/Runtime/Engine.generated.h"
#include <memory>

class FWindowsWindow;
class FTimer;
class UCameraComponent;
class UGameViewportClient;
class UUserWidget;

UCLASS()
class UEngine : public UObject
{
public:
	GENERATED_BODY()
	UEngine() = default;
	~UEngine() override = default;

	// Lifecycle
	virtual void Init(FWindowsWindow* InWindow);
	virtual void Shutdown();
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);

	virtual void OnWindowResized(uint32 Width, uint32 Height);

	// Lua gameplay code에서 안전하게 호출 가능한 scene 전환 요청. 게임 빌드는 active world를
	// 다음 Tick 끝에 destroy 하고 새 scene을 로드한다. 에디터(PIE)는 현재 PIE 세션을 유지한 채
	// 다음 frame 경계에서 PIE world만 교체한다.
	// 기본은 no-op — 서브클래스(UGameEngine / UEditorEngine) 에서 적절히 override.
	virtual void RequestTransitionToScene(const FString& /*InScenePath*/) {}
	virtual void RequestExit();

	// 다음 scene transition 직후 새 PlayerCameraManager 의 fade 를 alpha 1 → 0 으로 시작하도록 예약.
	// Standalone과 PIE가 같은 transition trigger 코드를 사용할 수 있도록 엔진 공통 상태로 보관합니다.
	void SetPendingFadeIn(float Duration, FLinearColor Color = FLinearColor::Black());

	// scene 전환 중 기존 LoadingScreen RML을 world 파괴와 무관한 엔진 소유 overlay 로 표시합니다.
	void ShowTransitionLoadingScreen();
	void HideTransitionLoadingScreen();

	// World context management
	FWorldContext& CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name = "");
	void DestroyWorldContext(const FName& Handle);

	// World context lookup
	FWorldContext* GetWorldContextFromHandle(const FName& Handle);
	const FWorldContext* GetWorldContextFromHandle(const FName& Handle) const;
	FWorldContext* GetWorldContextFromWorld(const UWorld* World);

	// Active world
	void SetActiveWorld(const FName& Handle);
	FName GetActiveWorldHandle() const { return ActiveWorldHandle; }

	// Accessors
	FWindowsWindow* GetWindow() const { return Window; }
	UWorld* GetWorld() const;
	const TArray<FWorldContext>& GetWorldList() const { return WorldList; }
	TArray<FWorldContext>& GetWorldList() { return WorldList; }

	void SetTimer(FTimer* InTimer) { Timer = InTimer; }
	FTimer* GetTimer() const { return Timer; }

	FRenderer& GetRenderer() { return Renderer; }

	// Game Viewport Client — PIE/Standalone 용
	void SetGameViewportClient(UGameViewportClient* InClient) { GameViewportClient = InClient; }
	UGameViewportClient* GetGameViewportClient() const { return GameViewportClient; }

	// GC Root Set
	void AddReferencedObjects(FReferenceCollector& Collector) override;
protected:
	void Render(float DeltaTime);
	void SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline);
	IRenderPipeline* GetRenderPipeline() const { return RenderPipeline.get(); }
	void WorldTick(float DeltaTime);
	void ApplyPendingFadeIn();

protected:
	FWindowsWindow* Window = nullptr;

	FName ActiveWorldHandle;
	TArray<FWorldContext> WorldList;

	FTimer* Timer = nullptr;

	UGameViewportClient* GameViewportClient = nullptr;

	FRenderer Renderer;

	bool bHasPendingFadeIn = false;
	float PendingFadeInDuration = 0.0f;
	FLinearColor PendingFadeInColor = FLinearColor::Black();

	UUserWidget* TransitionLoadingWidget = nullptr;

private:
	std::unique_ptr<IRenderPipeline> RenderPipeline;
};

extern UEngine* GEngine;
