#pragma once

#include "Object/Object.h"
#include "GameFramework/World.h"
#include "Component/Camera.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Scene/RenderBus.h"

class FWindowsWindow;
class FTimer;

class UEngine : public UObject
{
public:
	DECLARE_CLASS(UEngine, UObject)

	UEngine() = default;
	~UEngine() override = default;

	// Lifecycle
	virtual void Init(FWindowsWindow* InWindow);
	virtual void Shutdown();
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);

	virtual void OnWindowResized(uint32 Width, uint32 Height);

	// Accessors
	FWindowsWindow* GetWindow() const { return Window; }
	UWorld* GetWorld() const { return Scene.empty() ? nullptr : Scene[CurrentWorld]; }
	TArray<UWorld*>& GetScene() { return Scene; }
	uint32 GetCurrentWorld() const { return CurrentWorld; }
	void SetCurrentWorld(uint32 NewWorldIndex) { CurrentWorld = NewWorldIndex; }
	UCamera* GetCamera() const { return Camera; }
	FCameraState& GetCameraState() { return Camera->GetCameraState(); }
	const FCameraState& GetCameraState() const { return Camera->GetCameraState(); }

	void SetTimer(FTimer* InTimer) { Timer = InTimer; }
	FTimer* GetTimer() const { return Timer; }

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
	virtual void Render(float DeltaTime);
	void UpdateWorld(float DeltaTime);
	void SyncCameraFromRenderHandler();

protected:
	FWindowsWindow* Window = nullptr;

	uint32 CurrentWorld = 0;
	TArray<UWorld*> Scene;
	UCamera* Camera = nullptr;

	FTimer* Timer = nullptr;

	FRenderer Renderer;
	FRenderBus RenderBus;
};

extern UEngine* GEngine;
