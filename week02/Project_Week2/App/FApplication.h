#pragma once

#include <windows.h>

class UWorld;
class URenderer;
class UCameraComponent;
class AActor;
class FScene;
class FWindowsApplication;

class FApplication
{
public:
    FApplication();
    ~FApplication();

public:
    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();

private:
    bool InitializeEngine();
    bool InitializeScene();
    void MainLoop();
    void Tick(float DeltaTime);
    void RenderFrame();

private:
    FWindowsApplication* WindowApp;
    URenderer* Renderer;
    UWorld* World;
    FScene* Scene;

    AActor* CameraActor;
    UCameraComponent* MainCamera;

    bool bIsRunning;
};