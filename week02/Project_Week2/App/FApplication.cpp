#include "FApplication.h"

#include "WindowsApplication.h"

#include "World/UWorld.h"
#include "World/FScene.h"
#include "URenderer.h"

#include "Actor/AActor.h"
#include "Component/UCameraComponent.h"
#include "Component/USphereComponent.h"
#include "Geometry/Sphere.h"
#include "Resource/UStaticMesh.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/BuiltInMeshFactory.h"

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
    , SphereMesh(nullptr)
    , CubeMesh(nullptr)
    , TriangleMesh(nullptr)
    , AxesMesh(nullptr)
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

    if (!InitializeResources())
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

bool FApplication::InitializeResources()
{
    SphereMesh = BuiltInMeshFactory::CreateSphereMesh();
    CubeMesh = BuiltInMeshFactory::CreateCubeMesh();
    TriangleMesh = BuiltInMeshFactory::CreateTriangleMesh();
    AxesMesh = BuiltInMeshFactory::CreateAxesMesh();

    return true;
}

bool FApplication::InitializeScene()
{
    // 카메라 액터
    CameraActor = new AActor();
    MainCamera = new UCameraComponent();

    // 카메라가 바라보는 월드 수정 
    MainCamera->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
    MainCamera->SetRelativeRotation(FVector(0.0f, 0.6f, 0.0f));
    MainCamera->SetFieldOfView(90.0f);
    MainCamera->SetAspectRatio(
        static_cast<float>(WindowApp->GetClientWidth()) /
        static_cast<float>(WindowApp->GetClientHeight()));
    MainCamera->SetNearClip(0.1f);
    MainCamera->SetFarClip(1000.0f);

    CameraActor->AddComponent(MainCamera);
    CameraActor->SetRootComponent(MainCamera);

    World->AddActor(CameraActor);

    // Sphere
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(SphereMesh);
        MeshComp->SetRelativeLocation(FVector(-2.0f, 0.0f, 5.0f));
        MeshComp->SetRelativeScale(FVector(3.0f, 3.0f, 3.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Cube
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(CubeMesh);
        MeshComp->SetRelativeLocation(FVector(2.0f, 0.0f, 5.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Axes
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(AxesMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

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



        // test
        FVector Rot = MainCamera->GetRelativeRotation();
        Rot.Y += 0.01f;
        MainCamera->SetRelativeRotation(Rot);

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