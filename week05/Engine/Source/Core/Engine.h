#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneTypes.h"
#include "Windows.h"
#include "Core/Timer.h"
#include "Debug/DebugDrawManager.h"
#include "Physics/PhysicsManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "ViewportClient.h"
#include "World/WorldManager.h"
#include <memory>

class FWindowsWindow;
class AActor;
class UScene;
class UWorld;
class CInputManager;
class CEnhancedInputManager;
class ObjectManager;

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
	FEngine() = default;
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;

	bool Initialize(const FEngineInitArgs& Args);
	void TickFrame();
	virtual void Shutdown();
	bool HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void HandleResize(int32 Width, int32 Height);

	CRenderer* GetRenderer() const;
	IViewportClient* GetViewportClient() const;
	void SetViewportClient(IViewportClient* InViewportClient);
	CInputManager* GetInputManager() const;
	CEnhancedInputManager* GetEnhancedInputManager() const;
	const FTimer& GetTimer() const;
	float GetDeltaTime() const;
	FWorldManager* GetWorldManager() const;

	UScene* GetScene() const;
	UScene* GetActiveScene() const;
	UScene* GetEditorScene() const;
	UScene* GetGameScene() const;
	UScene* GetPreviewScene(const FString& ContextName) const;

	virtual void SetSelectedActor(AActor* InActor);
	virtual AActor* GetSelectedActor() const;
	void ActivateEditorScene() const;
	void ActivateGameScene() const;
	bool ActivatePreviewScene(const FString& ContextName) const;

	UWorld* GetActiveWorld() const;
	UWorld* GetEditorWorld() const;
	UWorld* GetGameWorld() const;
	const FWorldContext* GetActiveWorldContext() const;
	const TArray<std::unique_ptr<FEditorWorldContext>>& GetPreviewWorldContexts() const;
	FEditorWorldContext* CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height);

protected:
	virtual void PreInitialize() {}
	virtual void OnHostWindowReady(FWindowsWindow* InMainWindow) {}
	virtual void PostInitialize() {}
	virtual void Tick(float DeltaTime) {}
	virtual ESceneType GetStartupSceneType() const = 0;
	virtual std::unique_ptr<IViewportClient> CreateViewportClient() = 0;

	std::unique_ptr<IViewportClient> ViewportClient;

private:
	bool InitializeRuntime(HWND Hwnd, int32 Width, int32 Height, ESceneType StartupSceneType);
	void ReleaseRuntime();
	void BeginFrame();
	void TickInput(float DeltaTime);
	void TickPhysics(float DeltaTime);
	void TickWorld(float DeltaTime);
	void RenderFrame();
	void RunLateUpdate(float DeltaTime);
	void RegisterConsoleVariables();

private:
	FDebugDrawManager DebugDrawManager;
	std::unique_ptr<CRenderer> Renderer;
	CInputManager* InputManager = nullptr;
	CEnhancedInputManager* EnhancedInput = nullptr;
	ObjectManager* ObjManager = nullptr;
	IViewportClient* ActiveViewportClient = nullptr;
	std::unique_ptr<FWorldManager> WorldManager;
	std::unique_ptr<CPhysicsManager> PhysicsManager;

	FTimer Timer;
	double LastGCTime = 0.0;
	double GCInterval = 30.0;
	int32 WindowWidth = 0;
	int32 WindowHeight = 0;

	FRenderCommandQueue CommandQueue;
};
