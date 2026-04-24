// 런타임 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/Object.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Render/Renderer.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

#include <memory>

class FWindowsWindow;
class FTimer;
class UCameraComponent;
class UGameViewportClient;

// UEngine는 런타임 영역의 핵심 동작을 담당합니다.
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

    void SetGameViewportClient(UGameViewportClient* InClient) { GameViewportClient = InClient; }
    UGameViewportClient* GetGameViewportClient() const { return GameViewportClient; }

protected:
    virtual void Render(float DeltaTime);
    virtual void OnRenderSceneCleared() {}
    void WorldTick(float DeltaTime);

protected:
    FWindowsWindow* Window = nullptr;

    FName ActiveWorldHandle;
    TArray<FWorldContext> WorldList;

    FTimer* Timer = nullptr;

    UGameViewportClient* GameViewportClient = nullptr;

    FRenderer Renderer;

protected:
    FSceneView SceneView;
    FViewportRenderTargets RenderTargets;
};

extern UEngine* GEngine;
