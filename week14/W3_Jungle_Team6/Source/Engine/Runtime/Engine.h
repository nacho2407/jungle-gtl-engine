#pragma once

#include "Object/Object.h"
#include "GameFramework/World.h"
#include "Component/Camera.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Scene/RenderBus.h"

class UEngine : public UObject
{
public:
	DECLARE_CLASS(UEngine, UObject)

	UEngine() = default;
	~UEngine() override = default;

	// Lifecycle
	virtual void Init(HWND InHWindow);
	virtual void Shutdown();
	virtual void BeginPlay();
	virtual void BeginFrame(float DeltaTime);
	virtual void Tick(float DeltaTime);
	virtual void Render(float DeltaTime);
	virtual void EndFrame();

	virtual void OnWindowResized(uint32 Width, uint32 Height);

	// Accessors
	UWorld* GetWorld() const { return Scene.empty() ? nullptr : Scene[CurrentWorld]; }
	TArray<UWorld*>& GetScene() { return Scene; }
	uint32 GetCurrentWorld() const { return CurrentWorld; }
	void SetCurrentWorld(uint32 NewWorldIndex) { CurrentWorld = NewWorldIndex; }
	UCamera* GetCamera() const { return Camera; }
	FCameraState& GetCameraState() { return Camera->GetCameraState(); }
	const FCameraState& GetCameraState() const { return Camera->GetCameraState(); }

	void SetMainLoopFPS(float InFPS) { MainLoopFPS = InFPS; }
	float GetMainLoopFPS() const { return MainLoopFPS; }

	FRenderer& GetRenderer() { return Renderer; }

	template <typename T>
	AActor* SpawnNewPrimitiveActor(FVector InitLocation)
	{
		AActor* NewActor = Scene[CurrentWorld]->SpawnActor<AActor>();
		NewActor->SetActorLocation(InitLocation);
		NewActor->AddComponent<T>();
		return NewActor;
	}

protected:
	void UpdateWorld(float DeltaTime);
	void SyncCameraFromRenderHandler();

protected:
	HWND HWindow = nullptr;
	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	uint32 CurrentWorld = 0;
	TArray<UWorld*> Scene;
	UCamera* Camera = nullptr;

	FRenderer Renderer;
	FRenderBus RenderBus;
	float MainLoopFPS = 0.0f;
};

extern UEngine* GEngine;
