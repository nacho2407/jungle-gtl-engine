#pragma once

#include "CoreMinimal.h"
#include "Scene/WorldTypes.h"
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
class UScene;
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

	// 엔진 초기화 순서는 베이스가 고정하고, 파생형은 정해진 훅만 채운다.
	bool Initialize(const FEngineInitArgs& Args);
	void Tick();
	virtual void Shutdown();
	bool HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual void HandleResize(int32 Width, int32 Height);

	FRenderer* GetRenderer() const;
	IViewportClient* GetViewportClient() const;
	void SetViewportClient(IViewportClient* InViewportClient);
	FInputManager* GetInputManager() const;
	FEnhancedInputManager* GetEnhancedInputManager() const;
	const FTimer& GetTimer() const;
	float GetDeltaTime() const;
	const TArray<std::unique_ptr<FWorldContext>>& GetWorldContexts() const { return WorldContexts; }

	virtual UScene* GetScene() const;
	virtual UScene* GetActiveScene() const;
	virtual UScene* GetGameScene() const;
	virtual void ActivateGameScene() const;

	virtual UWorld* GetActiveWorld() const;
	virtual UWorld* GetGameWorld() const;
	virtual const FWorldContext* GetActiveWorldContext() const;

protected:
	virtual void PreInitialize() {}
	virtual void BindHost(FWindowsWindow* InMainWindow) {}
	virtual bool InitializeWorlds(int32 Width, int32 Height);
	virtual bool InitializeMode() { return true; }
	virtual void FinalizeInitialize() {}
	virtual void PrepareFrame(float DeltaTime);
	virtual void TickWorlds(float DeltaTime) = 0;
	virtual bool WantsPhysicsDebugVisualization() const { return false; }
	virtual std::unique_ptr<IViewportClient> CreateViewportClient() = 0;
	virtual void RenderFrame();
	virtual void SyncPlatformState();
	FWorldContext* FindWorldContext(EWorldType WorldType);
	const FWorldContext* FindWorldContext(EWorldType WorldType) const;
	FWorldContext* CreateWorldContext(const FString& ContextName, EWorldType WorldType, float AspectRatio, bool bDefaultScene);
	void DestroyWorldContext(FWorldContext* Context);
	void UpdateWorldAspectRatio(UWorld* World, float AspectRatio) const;

	std::unique_ptr<IViewportClient> ViewportClient;
	FPhysicsManager* GetPhysicsManager() const { return PhysicsManager.get(); }
	FDebugDrawManager& GetDebugDrawManager() { return DebugDrawManager; }

private:
	bool InitializeRuntimeSystems(HWND Hwnd, int32 Width, int32 Height);
	bool InitializePrimaryViewport();
	void ReleaseRuntime();
	void BeginFrame();
	void ProcessInput(float DeltaTime);
	void TickPhysics(float DeltaTime);
	void FinalizeFrame(float DeltaTime);
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
