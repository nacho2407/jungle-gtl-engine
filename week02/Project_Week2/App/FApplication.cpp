#include "FApplication.h"

#include "WindowsApplication.h"

#include "World/UWorld.h"
#include "World/FScene.h"
#include "URenderer.h"

#include "Actor/AActor.h"
#include "Component/UCameraComponent.h"
#include "Component/USphereComponent.h"

#include "Math/FVector.h"

#include <chrono>

FApplication::FApplication()
    : WindowApp(nullptr)
    , Renderer(nullptr)
    , World(nullptr)
    , Scene(nullptr)
    , CameraActor(nullptr)
    , MainCamera(nullptr)
    , bIsRunning(false)
{
}

FApplication::~FApplication()
{
    Shutdown();
}

bool FApplication::Initialize(HINSTANCE hInstance)
{
    WindowApp = new FWindowsApplication();
    if (!WindowApp->Initialize(hInstance, L"MyEngine", 1280, 720))
    {
        return false;
    }

    if (!InitializeEngine())
    {
        return false;
    }

    if (!InitializeScene())
    {
        return false;
    }

    bIsRunning = true;
    return true;
}

bool FApplication::InitializeEngine()
{
    Renderer = new URenderer();
    if (!Renderer->Create(WindowApp->GetHWND()))
    {
        return false;
    }

    World = new UWorld();
    Scene = new FScene();

    return true;
}

bool FApplication::InitializeScene()
{
    // 카메라 액터
    CameraActor = new AActor();
    MainCamera = new UCameraComponent();

    // 카메라가 바라보는 월드 수정 
    MainCamera->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
    MainCamera->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
    MainCamera->SetFieldOfView(90.0f);
    MainCamera->SetAspectRatio(
        static_cast<float>(WindowApp->GetClientWidth()) /
        static_cast<float>(WindowApp->GetClientHeight()));
    MainCamera->SetNearClip(0.1f);
    MainCamera->SetFarClip(1000.0f);

    CameraActor->AddComponent(MainCamera);
    CameraActor->SetRootComponent(MainCamera);

    World->AddActor(CameraActor);

    // 테스트용 구 하나
    AActor* SphereActor = new AActor();
    USphereComponent* SphereComp = new USphereComponent();

    SphereComp->SetRadius(1.0f);
    SphereComp->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));

    SphereActor->AddComponent(SphereComp);
    SphereActor->SetRootComponent(SphereComp);

    World->AddActor(SphereActor);

    return true;
}

int FApplication::Run()
{
    MainLoop();
    return 0;
}

void FApplication::MainLoop()
{
    using Clock = std::chrono::high_resolution_clock;
    auto PrevTime = Clock::now();

    while (bIsRunning)
    {
        if (!WindowApp->PumpMessages())
        {
            bIsRunning = false;
            break;
        }

        const bool bResized = WindowApp->ConsumeResizeFlag();
        if (bResized && Renderer)
        {
            const UINT Width = static_cast<UINT>(WindowApp->GetClientWidth());
            const UINT Height = static_cast<UINT>(WindowApp->GetClientHeight());

            if (Width > 0 && Height > 0)
            {
                Renderer->Resize(Width, Height);

                if (MainCamera)
                {
                    MainCamera->SetAspectRatio(
                        static_cast<float>(Width) /
                        static_cast<float>(Height));
                }
            }
        }

        auto CurrentTime = Clock::now();
        std::chrono::duration<float> Delta = CurrentTime - PrevTime;
        PrevTime = CurrentTime;

        Tick(Delta.count());
        RenderFrame();
    }
}

void FApplication::Tick(float DeltaTime)
{
    if (!World)
    {
        return;
    }

    World->Tick(DeltaTime);
    World->BuildScene(*Scene);
}

void FApplication::RenderFrame()
{
    if (!Renderer || !Scene)
    {
        return;
    }

    Renderer->BeginFrame();
    Renderer->Render(*Scene, MainCamera);
    Renderer->EndFrame();
}

void FApplication::Shutdown()
{
    bIsRunning = false;

    if (Renderer)
    {
        Renderer->Release();
        delete Renderer;
        Renderer = nullptr;
    }

    if (Scene)
    {
        delete Scene;
        Scene = nullptr;
    }

    if (World)
    {
        delete World;
        World = nullptr;
    }

    if (WindowApp)
    {
        delete WindowApp;
        WindowApp = nullptr;
    }

    CameraActor = nullptr;
    MainCamera = nullptr;
}