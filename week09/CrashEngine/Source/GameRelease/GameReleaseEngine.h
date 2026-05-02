#pragma once
#include "Runtime/Engine.h"
#include "Viewport/Viewport.h"

struct FPerspectiveCameraData;
class UCameraComponent;
class UWorld;

class UGameReleaseEngine : public UEngine
{
public:
    DECLARE_CLASS(UGameReleaseEngine, UEngine)

    void PreloadDefaultObjAssets(ID3D11Device* Device);
    virtual void Init(FWindowsWindow* InWindow) override;
    virtual void Shutdown() override;
    virtual void OnWindowResized(uint32 Width, uint32 Height) override;

private:
    FWorldContext& CreateFallbackGameWorld();
    FWorldContext& LoadStartupWorld();
    UCameraComponent* FindCameraInWorld(UWorld* World) const;
    UCameraComponent* CreateFallbackCamera(UWorld* World, const FPerspectiveCameraData* CameraData = nullptr);
    void EnsureActiveCamera(UWorld* World, const FPerspectiveCameraData* CameraData = nullptr);
    void DestroyWorlds();

    FViewport GameViewport;
};
