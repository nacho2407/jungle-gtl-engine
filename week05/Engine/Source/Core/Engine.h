#pragma once

#include "CoreMinimal.h"
#include "Level/WorldTypes.h"
#include "Windows.h"
#include "Core/Timer.h"
#include "Debug/DebugDrawManager.h"
#include "Physics/PhysicsManager.h"
#include "Renderer/RenderCommand.h"
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

class ENGINE_API FEngine
{
public:
	FEngine();
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;
	FEngine(FEngine&&) = delete;
	FEngine& operator=(const FEngine&&) = delete;

	/** 엔진 부팅의 진입점이다. 파생 클래스가 세부 단계를 채우고, 공통 런타임 초기화 순서는 베이스가 고정한다. */
	bool Initialize(const FEngineInitArgs& Args);
	/** 한 프레임 동안 입력, 물리, 월드 진행, 렌더링, 마무리 순서를 수행한다. */
	void Tick();
	/** 런타임 시스템, 월드, 뷰포트 클라이언트를 역순으로 정리한다. */
	virtual void Shutdown();
	/** 운영체제 메시지를 입력 시스템과 활성 뷰포트 클라이언트에 전달한다. */
	bool HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	/** 창 크기가 바뀌었을 때 렌더 타깃과 카메라 종횡비를 함께 갱신한다. */
	virtual void HandleResize(int32 Width, int32 Height);

	/** 현재 엔진이 소유한 렌더러를 반환한다. */
	FRenderer* GetRenderer() const;
	/** 실제로 입력/렌더 상호작용을 담당 중인 활성 뷰포트 클라이언트를 반환한다. */
	IViewportClient* GetViewportClient() const;
	/** 활성 뷰포트 클라이언트를 교체하며 Attach/Detach 훅도 함께 처리한다. */
	void SetViewportClient(IViewportClient* InViewportClient);
	/** 저수준 입력 상태를 모으는 입력 매니저를 반환한다. */
	FInputManager* GetInputManager() const;
	/** 액션/매핑 기반의 확장 입력 매니저를 반환한다. */
	FEnhancedInputManager* GetEnhancedInputManager() const;
	/** 프레임 시간과 누적 시간을 기록하는 타이머를 반환한다. */
	const FTimer& GetTimer() const;
	/** 최근 프레임 델타 타임을 초 단위로 반환한다. */
	float GetDeltaTime() const;
	/** 엔진이 소유한 모든 월드 컨텍스트 목록을 반환한다. */
	const TArray<std::unique_ptr<FWorldContext>>& GetWorldContexts() const { return WorldContexts; }

	/** 기본적으로 현재 활성 씬을 반환한다. 에디터 엔진은 이를 재정의해 다른 씬을 선택할 수 있다. */
	virtual ULevel* GetScene() const;
	/** 현재 렌더링/상호작용 대상인 활성 씬을 반환한다. */
	virtual ULevel* GetActiveScene() const;
	/** 게임 플레이용 기본 씬을 반환한다. */
	virtual ULevel* GetGameScene() const;
	/** 필요 시 게임 씬을 활성 씬으로 전환하도록 파생 클래스가 구현한다. */
	virtual void ActivateGameScene() const;

	/** 현재 활성 월드를 반환한다. */
	virtual UWorld* GetActiveWorld() const;
	/** 게임 플레이용 기본 월드를 반환한다. */
	virtual UWorld* GetGameWorld() const;
	/** 현재 활성 월드 컨텍스트를 반환한다. */
	virtual const FWorldContext* GetActiveWorldContext() const;

protected:
	/** 공통 런타임 초기화 전에 파생 클래스가 선행 준비를 할 수 있는 훅이다. */
	virtual void PreInitialize() {}
	/** 엔진이 창/호스트 객체에 의존해야 할 때 바인딩하는 훅이다. */
	virtual void BindHost(FWindowsWindow* InMainWindow) {}
	/** 필요한 월드 컨텍스트를 생성하고 초기 씬을 준비한다. */
	virtual bool InitializeWorlds(int32 Width, int32 Height);
	/** 게임/에디터별 전용 모드 초기화를 수행한다. */
	virtual bool InitializeMode() { return true; }
	/** 모든 초기화가 성공한 뒤 마지막 검증이나 후처리를 수행한다. */
	virtual void FinalizeInitialize() {}
	/** 프레임 시작 직후, 입력 처리 전에 공통 상태를 준비한다. */
	virtual void PrepareFrame(float DeltaTime);
	/** 각 파생 엔진이 자신이 관리하는 월드들을 실제로 Tick한다. */
	virtual void TickWorlds(float DeltaTime) = 0;
	/** 물리 디버그 시각화가 필요한 엔진인지 알려준다. */
	virtual bool WantsPhysicsDebugVisualization() const { return false; }
	/** 기본 뷰포트 클라이언트를 생성한다. */
	virtual std::unique_ptr<IViewportClient> CreateViewportClient() = 0;
	/** 활성 월드와 카메라를 기준으로 렌더 큐를 만들고 실제 드로우를 실행한다. */
	virtual void RenderFrame();
	/** 플랫폼별 상태 동기화가 필요할 때 파생 클래스가 채운다. */
	virtual void SyncPlatformState();
	/** 월드 타입으로 컨텍스트를 찾는다. */
	FWorldContext* FindWorldContext(EWorldType WorldType);
	/** const 버전의 월드 타입 검색 함수다. */
	const FWorldContext* FindWorldContext(EWorldType WorldType) const;
	/** 새 월드 컨텍스트를 만들고, 필요하면 기본 씬까지 바로 초기화한다. */
	FWorldContext* CreateWorldContext(const FString& ContextName, EWorldType WorldType, float AspectRatio, bool bDefaultScene);
	/** 월드 컨텍스트를 정리하고 내부 목록에서 제거한다. */
	void DestroyWorldContext(FWorldContext* Context);
	/** 월드의 카메라 종횡비를 창 크기에 맞게 맞춘다. */
	void UpdateWorldAspectRatio(UWorld* World, float AspectRatio) const;

	std::unique_ptr<IViewportClient> ViewportClient;
	/** 파생 클래스가 디버그/툴링 용도로 물리 매니저에 접근할 수 있게 열어둔다. */
	FPhysicsManager* GetPhysicsManager() const { return PhysicsManager.get(); }
	/** 디버그 선/도형을 쌓아둘 매니저를 반환한다. */
	FDebugDrawManager& GetDebugDrawManager() { return DebugDrawManager; }

private:
	/** 렌더러, 입력, 오브젝트 매니저 등 공통 런타임 시스템을 생성한다. */
	bool InitializeRuntimeSystems(HWND Hwnd, int32 Width, int32 Height);
	/** CreateViewportClient 결과를 활성 뷰포트로 등록한다. */
	bool InitializePrimaryViewport();
	/** 런타임 시스템과 월드 컨텍스트를 모두 정리한다. */
	void ReleaseRuntime();
	/** 프레임 타이머를 갱신한다. */
	void BeginFrame();
	/** 입력 상태를 갱신하고 뷰포트별 Tick을 실행한다. */
	void ProcessInput(float DeltaTime);
	/** 물리 시뮬레이션과 디버그 시각화를 진행한다. */
	void TickPhysics(float DeltaTime);
	/** 주기적 GC 같은 프레임 종료 후처리를 수행한다. */
	void FinalizeFrame(float DeltaTime);
	/** 런타임 콘솔 변수와 명령을 등록한다. */
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

	FRenderCommandQueue CommandQueue;
};
