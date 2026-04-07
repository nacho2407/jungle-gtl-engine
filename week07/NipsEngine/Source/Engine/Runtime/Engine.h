#pragma once

#include "Object/Object.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Renderer/IRenderPipeline.h"

#include <memory>

class FWindowsWindow;
class FTimer;
class UCameraComponent;

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

protected:
	void Render(float DeltaTime);
	void SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline);
	void WorldTick(float DeltaTime);

protected:
	FWindowsWindow* Window = nullptr;

	FName ActiveWorldHandle;
	TArray<FWorldContext> WorldList;

	FTimer* Timer = nullptr;

	FRenderer Renderer;

private:
	std::unique_ptr<IRenderPipeline> RenderPipeline;
};

extern UEngine* GEngine;
