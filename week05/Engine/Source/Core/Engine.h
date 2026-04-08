#pragma once

#include "CoreMinimal.h"
#include "Level/WorldTypes.h"
#include "Windows.h"
#include "Core/Timer.h"
#include "Debug/DebugDrawManager.h"
#include "Physics/PhysicsManager.h"
#include "Renderer/Renderer.h"
#include "ViewportClient.h"
#include "World/WorldContext.h"
#include <memory>

class FWindowsWindow;
class AActor;
class ULevel;
class UWorld;
class FInputManager;
class FEnhancedInputManager;
class FObjectManager;

struct FEngineInitArgs
{
	FWindowsWindow* MainWindow = nullptr;
	HWND Hwnd = nullptr;
	int32 Width = 0;
	int32 Height = 0;
};
/**
 * 엔진 런타임의 최상위 진입점이다.
 * 플랫폼, 월드, 입력, 물리, 렌더링 서브시스템을 연결하고
 * 한 프레임의 전체 순서를 조정한다.
 */
class ENGINE_API FEngine
{
public:
	FEngine();
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;
	FEngine(FEngine&&) = delete;
	FEngine& operator=(const FEngine&&) = delete;

	// 엔진 런타임 시스템과 기본 월드/뷰포트를 초기화한다.
	bool Initialize(const FEngineInitArgs& Args);
	// 한 프레임의 업데이트와 렌더링 순서를 실행한다.
	void Tick();
	// 엔진이 소유한 런타임 자원을 종료 순서에 맞게 해제한다.
	virtual void Shutdown();
	// 플랫폼 메시지를 입력/뷰포트 계층으로 전달한다.
	bool HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	// 창 크기 변경을 렌더러와 월드 카메라에 반영한다.
	virtual void HandleResize(int32 Width, int32 Height);

	// 현재 엔진이 사용하는 렌더러를 반환한다.
	FRenderer* GetRenderer() const;
	// 현재 활성 뷰포트 클라이언트를 반환한다.
	IViewportClient* GetViewportClient() const;
	// 활성 뷰포트 클라이언트를 교체하고 Attach/Detach를 처리한다.
	void SetViewportClient(IViewportClient* InViewportClient);
	// 기본 입력 매니저를 반환한다.
	FInputManager* GetInputManager() const;
	// 강화 입력 매니저를 반환한다.
	FEnhancedInputManager* GetEnhancedInputManager() const;
	// 현재 프레임 타이머를 반환한다.
	const FTimer& GetTimer() const;
	// 현재 프레임 델타 타임을 반환한다.
	float GetDeltaTime() const;
	const TArray<std::unique_ptr<FWorldContext>>& GetWorldContexts() const { return WorldContexts; }

	// 현재 활성 씬을 반환한다.
	virtual ULevel* GetScene() const;
	// 현재 렌더링 대상 활성 씬을 반환한다.
	virtual ULevel* GetActiveScene() const;
	// 게임 월드의 기본 씬을 반환한다.
	virtual ULevel* GetGameScene() const;
	// 게임 씬을 활성 씬으로 전환할 때 사용하는 훅이다.
	virtual void ActivateGameScene() const;

	// 현재 활성 월드를 반환한다.
	virtual UWorld* GetActiveWorld() const;
	// 게임 월드를 반환한다.
	virtual UWorld* GetGameWorld() const;
	// 현재 활성 월드 컨텍스트를 반환한다.
	virtual const FWorldContext* GetActiveWorldContext() const;

protected:
	// 파생 엔진이 런타임 초기화 전에 준비 작업을 넣을 수 있다.
	virtual void PreInitialize() {}
	// 플랫폼 호스트와 엔진을 연결할 기회를 제공한다.
	virtual void BindHost(FWindowsWindow* InMainWindow) {}
	// 월드 컨텍스트와 월드 초기화를 담당한다.
	virtual bool InitializeWorlds();
	// 게임/에디터 모드별 추가 초기화를 수행한다.
	virtual bool InitializeMode() { return true; }
	// 전체 초기화가 끝난 뒤 후처리를 수행한다.
	virtual void FinalizeInitialize() {}
	// 프레임 시작 직후 파생 엔진이 준비 작업을 할 수 있다.
	virtual void PrepareFrame(float DeltaTime);
	// 실제 월드 Tick은 파생 엔진이 구현한다.
	virtual void TickWorlds(float DeltaTime) = 0;
	// 물리 디버그 가시화가 필요한지 파생 엔진이 결정한다.
	virtual bool WantsPhysicsDebugVisualization() const { return false; }
	// 현재 모드에 맞는 기본 뷰포트 클라이언트를 생성한다.
	virtual std::unique_ptr<IViewportClient> CreateViewportClient() = 0;
	// 렌더 프레임 전체 순서를 실행한다.
	virtual void RenderFrame();
	// 프레임 이후 플랫폼 상태를 동기화한다.
	virtual void SyncPlatformState();
	// 월드 타입으로 월드 컨텍스트를 찾는다.
	FWorldContext* FindWorldContext(EWorldType WorldType);
	// 월드 타입으로 상수 월드 컨텍스트를 찾는다.
	const FWorldContext* FindWorldContext(EWorldType WorldType) const;
	// 새 월드 컨텍스트와 월드를 만들고 목록에 등록한다.
	FWorldContext* CreateWorldContext(const FString& ContextName, EWorldType WorldType, float AspectRatio, bool bDefaultScene);
	FWorldContext* CreateWorldContext(const FString& ContextName, EWorldType WorldType, UWorld* ExistingWorld);
	// 월드 컨텍스트와 그 안의 월드를 정리하고 제거한다.
	void DestroyWorldContext(FWorldContext* Context);
	// 월드의 카메라 종횡비를 현재 창 크기에 맞게 갱신한다.
	void UpdateWorldAspectRatio(UWorld* World, float AspectRatio) const;

	std::unique_ptr<IViewportClient> ViewportClient;
	// 물리 매니저 접근자다.
	FPhysicsManager* GetPhysicsManager() const { return PhysicsManager.get(); }
	// 디버그 드로우 매니저 접근자다.
	FDebugDrawManager& GetDebugDrawManager() { return DebugDrawManager; }
	float GetWindowAspectRatio() const { return (WindowHeight > 0) ? (static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight)) : 1.0f; }

private:
	// 렌더러, 입력, 오브젝트 매니저 등 런타임 코어 시스템을 만든다.
	bool InitializeRuntimeSystems(HWND Hwnd, int32 Width, int32 Height);
	// 기본 뷰포트 클라이언트를 만들고 활성화한다.
	bool InitializePrimaryViewport();
	// 런타임 시스템을 종료 순서대로 정리한다.
	void ReleaseRuntime();
	// 프레임 시작 시 타이머 등 공용 상태를 갱신한다.
	void BeginFrame();
	// 입력 시스템과 뷰포트 Tick을 처리한다.
	void ProcessInput(float DeltaTime);
	// 물리 시뮬레이션 단계를 실행한다.
	void TickPhysics(float DeltaTime);
	// GC 같은 프레임 마무리 작업을 처리한다.
	void FinalizeFrame(float DeltaTime);
	// 렌더링 및 런타임 관련 콘솔 변수를 등록한다.
	void RegisterConsoleVariables();

private:
	FDebugDrawManager						DebugDrawManager;
	std::unique_ptr<FRenderer>				Renderer;
	std::unique_ptr<FInputManager>			InputManager;
	std::unique_ptr<FEnhancedInputManager>	EnhancedInput;
	std::unique_ptr<FObjectManager>			ObjManager;
	IViewportClient*						ActiveViewportClient = nullptr;
	TArray<std::unique_ptr<FWorldContext>>	WorldContexts;
	std::unique_ptr<FPhysicsManager>		PhysicsManager;

	FTimer Timer;
	double LastGCTime = 0.0;
	double GCInterval = 30.0;
	int32 WindowWidth = 0;
	int32 WindowHeight = 0;
};
