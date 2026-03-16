#define NOMINMAX
#include <algorithm>

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
#include "Resource/MeshImporter.h"

#include "FRay.h"
#include "Math/FVector4.h"
#include <cfloat>
#include <cmath>

#include "Math/FVector.h"

#include "FUObjectFactory.h"
#include "FUObjectAllocator.h"

#include "GUI/GUI.h"

#include <chrono>

namespace
{
    FVector4 LightenColor(const FVector4& C, float T)
    {
        return FVector4(
            C.X + (1.0f - C.X) * T,
            C.Y + (1.0f - C.Y) * T,
            C.Z + (1.0f - C.Z) * T,
            C.W);
    }
}

FApplication::FApplication()
    : WindowApp(nullptr)
    , Renderer(nullptr)
    , World(nullptr)
    , Scene(nullptr)
    , CameraActor(nullptr)
    , MainCamera(nullptr)
    , bIsRunning(false)
    , SphereMesh(nullptr)
    //, CubeMesh(nullptr)
    //, TriangleMesh(nullptr)
    , TorusMesh(nullptr)
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

    // ImGui 초기화
    if (!InitializeGUI())
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

bool FApplication::InitializeGUI()
{
    if (!WindowApp || !Renderer)
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(WindowApp->GetHWND()))
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(Renderer->Device, Renderer->DeviceContext))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    return true;
}

bool FApplication::InitializeResources()
{
    //SphereMesh = MeshImporter::LoadStaticMeshFromGltf("Assets/BlueSphere.gltf");
    SphereMesh = BuiltInMeshFactory::CreateSphereMesh();
    CubeMesh = BuiltInMeshFactory::CreateCubeMesh();
    //TriangleMesh = BuiltInMeshFactory::CreateTriangleMesh();
    TorusMesh = BuiltInMeshFactory::CreateTorusMesh(64, 32, 1.2f, 0.35f);
    AxesMesh = BuiltInMeshFactory::CreateAxesMesh();
    GizmoArrowMesh = BuiltInMeshFactory::CreateGizmoArrowMesh();

    //ClickCircleMesh = BuiltInMeshFactory::CreateCircleMesh(64);
    ClickCircleMesh = BuiltInMeshFactory::CreateDiscMesh(64);

    return true;
}

bool FApplication::InitializeScene()
{
    // 카메라 액터
    CameraActor = new AActor();
    MainCamera = new UCameraComponent();
    // 카메라가 바라보는 월드 수정 
    MainCamera->SetRelativeLocation(FVector(2.0f, 4.0f, -7.0f));
    MainCamera->SetRelativeRotation(FVector(0.3f, 0.0f, 0.0f)); // Pitch Yaw Roll
    MainCamera->SetFieldOfView(39.6f);
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
        MeshComp->SetRelativeLocation(FVector(-2.0f, -2.0f, 5.0f));
        //MeshComp->SetRelativeScale(FVector(3.0f, 3.0f, 3.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Sphere
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(SphereMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 2.0f, 10.0f));
        //MeshComp->SetRelativeScale(FVector(3.0f, 3.0f, 3.0f));
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

    // 도나쓰
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(TorusMesh);
        MeshComp->SetRelativeLocation(FVector(3.0f, 0.0f, 8.0f));
        MeshComp->SetRelativeScale(FVector(1.5f, 1.5f, 1.5f));
        MeshComp->SetRelativeRotation(FVector(0.6f, 0.f, 0.f));

        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // World Axes
    {
        WorldAxesActor = new AActor();

        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(AxesMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

        WorldAxesActor->AddComponent(MeshComp);
        WorldAxesActor->SetRootComponent(MeshComp);
        World->AddActor(WorldAxesActor);
    }

    // Gizmo
    // @@@ 이렇게 길게 여기서 처리하는 게 맞음????
    {
        GizmoActor = new AActor();

        GizmoXComp = new UStaticMeshComponent();
        GizmoYComp = new UStaticMeshComponent();
        GizmoZComp = new UStaticMeshComponent();

        GizmoXComp->SetStaticMesh(GizmoArrowMesh);
        GizmoYComp->SetStaticMesh(GizmoArrowMesh);
        GizmoZComp->SetStaticMesh(GizmoArrowMesh);

        GizmoXComp->SetRelativeLocation(FVector::ZeroVector);
        GizmoYComp->SetRelativeLocation(FVector::ZeroVector);
        GizmoZComp->SetRelativeLocation(FVector::ZeroVector);

        // arrow mesh가 +X 방향 기준이라고 가정
        GizmoXComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
        GizmoYComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
        GizmoZComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

        GizmoXComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
        GizmoYComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
        GizmoZComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));

        GizmoXComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
        GizmoYComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
        GizmoZComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

        GizmoXComp->SetVisibility(false);
        GizmoYComp->SetVisibility(false);
        GizmoZComp->SetVisibility(false);

        GizmoActor->AddComponent(GizmoXComp);
        GizmoActor->AddComponent(GizmoYComp);
        GizmoActor->AddComponent(GizmoZComp);
        GizmoActor->SetRootComponent(GizmoXComp);

        World->AddActor(GizmoActor);
    }

    // Click Pulse Circle
    {
        ClickCircleActor = new AActor();
        ClickCircleComp = new UStaticMeshComponent();

        ClickCircleComp->SetStaticMesh(ClickCircleMesh);
        ClickCircleComp->SetRelativeLocation(FVector::ZeroVector);
        ClickCircleComp->SetRelativeRotation(FVector::ZeroVector);
        ClickCircleComp->SetRelativeScale(FVector(0.0f, 0.0f, 0.0f));

        ClickCircleComp->SetRenderColor(FVector4(0.98f, 0.84f, 0.10f, 1.0f));
        ClickCircleComp->SetUseVertexColor(false);
        ClickCircleComp->SetDepthEnable(false);
        ClickCircleComp->SetCullMode(ERenderCullMode::None);
        ClickCircleComp->SetVisibility(false);

        ClickCircleActor->AddComponent(ClickCircleComp);
        ClickCircleActor->SetRootComponent(ClickCircleComp);

        World->AddActor(ClickCircleActor);
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
                    MainCamera->UpdateAspectRatio(Width, Height);
                }
            }
        }

        auto CurrentTime = Clock::now();
        std::chrono::duration<float> Delta = CurrentTime - PrevTime;
        PrevTime = CurrentTime;



        // test
        //FVector Rot = MainCamera->GetRelativeRotation();
        //Rot.Y += 0.01f;
        //MainCamera->SetRelativeRotation(Rot);

        Tick(Delta.count());
        RenderFrame();
    }
}

void FApplication::Tick(float DeltaTime)
{
    if (!World || !MainCamera)
    {
        return;
    }

    // 상혁 테스트
    UpdateObjectAllocationTest();

    static int PrevMouseX = 0;
    static int PrevMouseY = 0;

    int MouseX = 0;
    int MouseY = 0;

    int DownX = 0;
    int DownY = 0;
    if (WindowApp->ConsumeLeftMouseDown(DownX, DownY))
    {
        BeginPointerPulse(DownX, DownY);

        EGizmoAxis Axis = PickGizmoAxis(DownX, DownY);
        if (Axis != EGizmoAxis::None)
        {
            BeginGizmoDrag(Axis, DownX, DownY);
        }
        else
        {
            FHitProxy Proxy = Renderer->PickPrimitiveProxy(DownX, DownY);

            AActor* HitActor = nullptr;

            if (Proxy.Type == EHitProxyType::Primitive && Proxy.Primitive)
            {
                HitActor = Proxy.Primitive->GetOwner();

                if (HitActor == GizmoActor || HitActor == WorldAxesActor)
                {
                    HitActor = nullptr;
                }
            }

            SetSelectedActor(HitActor);
        }
    }

    int UpX = 0;
    int UpY = 0;
    if (WindowApp->ConsumeLeftMouseUp(UpX, UpY))
    {
        EndGizmoDrag();
        EndPointerPulse();
    }

    // 우클릭 down에도 원 시작
    int RightDownX = 0;
    int RightDownY = 0;
    if (WindowApp->ConsumeRightMouseDown(RightDownX, RightDownY))
    {
        BeginPointerPulse(RightDownX, RightDownY);

        // Orbit 시작 기준점 맞추기
        PrevMouseX = RightDownX;
        PrevMouseY = RightDownY;
    } 
    // 우클릭 up에도 원 종료
    int RightUpX = 0;
    int RightUpY = 0;
    if (WindowApp->ConsumeRightMouseUp(RightUpX, RightUpY))
    {
        EndPointerPulse();
    }

    if (bDraggingGizmo && WindowApp->IsLeftMousePressed())
    {
        WindowApp->GetMousePosition(MouseX, MouseY);
        UpdateGizmoDrag(MouseX, MouseY);
    }


    if (WindowApp->IsRightMousePressed())
    {
        WindowApp->GetMousePosition(MouseX, MouseY);

        int DeltaX = MouseX - PrevMouseX;
        int DeltaY = MouseY - PrevMouseY;

        FVector Rot = MainCamera->GetRelativeRotation();

        const float RotateSpeed = 0.005f;

        Rot.Y += DeltaX * RotateSpeed; // Yaw
        Rot.X += DeltaY * RotateSpeed; // Pitch

        Rot.X = std::clamp(Rot.X, -1.5f, 1.5f);

        MainCamera->SetRelativeRotation(Rot);

        PrevMouseX = MouseX;
        PrevMouseY = MouseY;
    }
    else
    {
        WindowApp->GetMousePosition(PrevMouseX, PrevMouseY);
    }

    FVector CameraLoc = MainCamera->GetRelativeLocation();
    FVector Rot = MainCamera->GetRelativeRotation();

    FMatrix RotMatrix = FMatrix::MakeRotationXYZ(Rot);

    FVector4 Forward4 = RotMatrix * FVector4(0, 0, 1, 0);
    FVector4 Right4 = RotMatrix * FVector4(1, 0, 0, 0);
    FVector4 Up4 = RotMatrix * FVector4(0, 1, 0, 0);

    FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);
    FVector Right(Right4.X, Right4.Y, Right4.Z);
    FVector Up(Up4.X, Up4.Y, Up4.Z);

    float Speed = 5.0f * DeltaTime;

    if (WindowApp->IsKeyDown('W'))
        CameraLoc += Forward * Speed;

    if (WindowApp->IsKeyDown('S'))
        CameraLoc -= Forward * Speed;

    if (WindowApp->IsKeyDown('A'))
        CameraLoc -= Right * Speed;

    if (WindowApp->IsKeyDown('D'))
        CameraLoc += Right * Speed;

    if (WindowApp->IsKeyDown('Q'))
        CameraLoc -= Up * Speed;

    if (WindowApp->IsKeyDown('E'))
        CameraLoc += Up * Speed;

    MainCamera->SetRelativeLocation(CameraLoc);

    float Wheel = WindowApp->ConsumeMouseWheelDelta();

    if (Wheel != 0.0f)
    {
        FVector Loc = MainCamera->GetRelativeLocation();
        FVector Rot = MainCamera->GetRelativeRotation();

        FMatrix RotMatrix = FMatrix::MakeRotationXYZ(Rot);

        FVector4 Forward4 = RotMatrix * FVector4(0, 0, 1, 0);
        FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);

        float DollySpeed = 2.0f;

        Loc += Forward * Wheel * DollySpeed;

        MainCamera->SetRelativeLocation(Loc);
    }

    UpdatePointerPulse(DeltaTime);
    World->Tick(DeltaTime);

    if (SelectedActor)
    {
        UpdateGizmoTransform();
        UpdateGizmoColors();
    }

    World->BuildScene(*Scene);
    AddSelectionOutlineRenderItem();
}

void FApplication::RenderFrame()
{
    if (!Renderer || !Scene)
    {
        return;
    }

    //World->BuildScene(*Scene);

    Renderer->BeginFrame();
    Renderer->Render(*Scene, MainCamera);

    // 여기서 메인 백버퍼 다시 바인딩
    Renderer->BindMainRenderTargetForOverlay();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderDebugUI();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    Renderer->EndFrame();
}

void FApplication::Shutdown()
{
    bIsRunning = false;

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

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

void FApplication::HandleMousePicking()
{
    if (!WindowApp || !World || !MainCamera)
    {
        return;
    }

    int MouseX = 0;
    int MouseY = 0;
    if (!WindowApp->ConsumeLeftClick(MouseX, MouseY))
    {
        return;
    }

    FRay Ray = BuildPickRay(MouseX, MouseY);
    AActor* HitActor = PickActor(Ray);

    // 빈 공간 클릭 시 해제하고 싶으면 이렇게
     SetSelectedActor(HitActor);

    // 빈 공간 클릭 시 유지하고 싶으면 이렇게
    //if (HitActor)
    //{
    //    SetSelectedActor(HitActor);
    //}
}

FRay FApplication::BuildPickRay(int MouseX, int MouseY) const
{
    FRay Ray;

    const float Width = static_cast<float>(WindowApp->GetClientWidth());
    const float Height = static_cast<float>(WindowApp->GetClientHeight());

    const float NdcX = (2.0f * static_cast<float>(MouseX) / Width) - 1.0f;
    const float NdcY = 1.0f - (2.0f * static_cast<float>(MouseY) / Height);

    const float FovRadians = MainCamera->GetFieldOfView() * 3.14159265358979323846f / 180.0f;
    const float TanHalfFov = std::tan(FovRadians * 0.5f);

    const float ViewX = NdcX * MainCamera->GetAspectRatio() * TanHalfFov;
    const float ViewY = NdcY * TanHalfFov;

    const FVector CameraLocation = MainCamera->GetRelativeLocation();
    const FVector CameraRotation = MainCamera->GetRelativeRotation();

    const FMatrix CameraRotationMatrix = FMatrix::MakeRotationXYZ(CameraRotation);

    const FVector4 Right4 = CameraRotationMatrix * FVector4(1.0f, 0.0f, 0.0f, 0.0f);
    const FVector4 Up4 = CameraRotationMatrix * FVector4(0.0f, 1.0f, 0.0f, 0.0f);
    const FVector4 Forward4 = CameraRotationMatrix * FVector4(0.0f, 0.0f, 1.0f, 0.0f);

    const FVector Right(Right4.X, Right4.Y, Right4.Z);
    const FVector Up(Up4.X, Up4.Y, Up4.Z);
    const FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);

    FVector Direction = Right * ViewX + Up * ViewY + Forward;
    Direction.Normalize();

    Ray.Origin = CameraLocation;
    Ray.Direction = Direction;
    return Ray;
}

AActor* FApplication::PickActor(const FRay& Ray) const
{
    AActor* ClosestActor = nullptr;
    float ClosestT = FLT_MAX;

    const TArray<AActor*>& Actors = World->GetActors();

    for (AActor* Actor : Actors)
    {
        // @@@ Actor==GizmoActor라는데, 이거 XYZ로 나누면서 nullptr 아닌가?
        if (!Actor || Actor == CameraActor || Actor == GizmoActor || Actor == WorldAxesActor || Actor == ClickCircleActor)
        {
            continue;
        }

        UStaticMeshComponent* MeshComp = FindStaticMeshComponent(Actor);
        if (!MeshComp || !MeshComp->IsVisible())
        {
            continue;
        }

        UStaticMesh* Mesh = MeshComp->GetStaticMesh();
        if (!Mesh)
        {
            continue;
        }

        const FVector Center = MeshComp->GetRelativeLocation();
        const FVector Scale = MeshComp->GetRelativeScale();

        float MaxRadius = 0.0f;
        for (const FVertexSimple& Vertex : Mesh->GetVertices())
        {
            const float Len = Vertex.Position.Length();
            if (Len > MaxRadius)
            {
                MaxRadius = Len;
            }
        }

        const float MaxScale = std::max(Scale.X, std::max(Scale.Y, Scale.Z));

        const float WorldRadius = MaxRadius * MaxScale;

        float T = 0.0f;
        if (IntersectRaySphere(Ray, Center, WorldRadius, T))
        {
            if (T < ClosestT)
            {
                ClosestT = T;
                ClosestActor = Actor;
            }
        }
    }

    return ClosestActor;
}

bool FApplication::IntersectRaySphere(const FRay& Ray, const FVector& Center, float Radius, float& OutT) const
{
    const FVector OC = Ray.Origin - Center;

    const float A = Ray.Direction.Dot(Ray.Direction);
    const float B = 2.0f * OC.Dot(Ray.Direction);
    const float C = OC.Dot(OC) - Radius * Radius;

    const float Discriminant = B * B - 4.0f * A * C;
    if (Discriminant < 0.0f)
    {
        return false;
    }

    const float SqrtD = std::sqrt(Discriminant);
    const float T0 = (-B - SqrtD) / (2.0f * A);
    const float T1 = (-B + SqrtD) / (2.0f * A);

    if (T0 > 0.0f)
    {
        OutT = T0;
        return true;
    }

    if (T1 > 0.0f)
    {
        OutT = T1;
        return true;
    }

    return false;
}

UStaticMeshComponent* FApplication::FindStaticMeshComponent(AActor* Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    const TArray<UActorComponent*>& Components = Actor->GetComponents();
    for (UActorComponent* Component : Components)
    {
        UStaticMeshComponent* MeshComp = dynamic_cast<UStaticMeshComponent*>(Component);
        if (MeshComp)
        {
            return MeshComp;
        }
    }

    return nullptr;
}

void FApplication::SetSelectedActor(AActor* NewSelected)
{
    if (SelectedActor == NewSelected)
    {
        SelectedActor = nullptr;
        SetGizmoVisibility(false);
        return;
    }

    SelectedActor = NewSelected;

    if (!SelectedActor)
    {
        SelectedActor = nullptr;
        SetGizmoVisibility(false);
        return;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root || !GizmoXComp || !GizmoYComp || !GizmoZComp)
    {
        return;
    }

    if (!Root || !GizmoXComp || !GizmoYComp || !GizmoZComp)
    {
        return;
    }

    UpdateGizmoTransform();

    GizmoXComp->SetVisibility(true);
    GizmoYComp->SetVisibility(true);
    GizmoZComp->SetVisibility(true);

    UpdateGizmoColors();
}

bool FApplication::ProjectWorldToScreen(const FVector& WorldPos, float& OutX, float& OutY) const
{
    if (!MainCamera || !WindowApp)
    {
        return false;
    }

    const FMatrix View = MainCamera->GetViewMatrix();
    const FMatrix Projection = MainCamera->GetProjectionMatrix();

    FVector4 ClipPos = Projection * (View * FVector4(WorldPos, 1.0f));

    if (ClipPos.W <= 0.0001f)
    {
        return false;
    }

    const float InvW = 1.0f / ClipPos.W;
    const float NdcX = ClipPos.X * InvW;
    const float NdcY = ClipPos.Y * InvW;

    const float Width = static_cast<float>(WindowApp->GetClientWidth());
    const float Height = static_cast<float>(WindowApp->GetClientHeight());

    OutX = (NdcX * 0.5f + 0.5f) * Width;
    OutY = (1.0f - (NdcY * 0.5f + 0.5f)) * Height;

    return true;
}

EGizmoAxis FApplication::PickGizmoAxis(int MouseX, int MouseY) const
{
    if (!SelectedActor || !GizmoXComp || !GizmoXComp->IsVisible())
    {
        return EGizmoAxis::None;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root)
    {
        return EGizmoAxis::None;
    }

    const FVector Origin = Root->GetRelativeLocation();
    const float AxisLength = 3.0f;

    const FVector XEnd = Origin + FVector(AxisLength, 0.0f, 0.0f);
    const FVector YEnd = Origin + FVector(0.0f, AxisLength, 0.0f);
    const FVector ZEnd = Origin + FVector(0.0f, 0.0f, AxisLength);

    float OX, OY, XX, XY, YX, YY, ZX, ZY;

    if (!ProjectWorldToScreen(Origin, OX, OY))
    {
        return EGizmoAxis::None;
    }

    bool bX = ProjectWorldToScreen(XEnd, XX, XY);
    bool bY = ProjectWorldToScreen(YEnd, YX, YY);
    bool bZ = ProjectWorldToScreen(ZEnd, ZX, ZY);

    const float Threshold = 10.0f;

    float BestDist = FLT_MAX;
    EGizmoAxis BestAxis = EGizmoAxis::None;

    if (bX && GizmoXComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D(
            static_cast<float>(MouseX), static_cast<float>(MouseY),
            OX, OY, XX, XY);

        if (Dist < Threshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::X;
        }
    }

    if (bY && GizmoYComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D(
            static_cast<float>(MouseX), static_cast<float>(MouseY),
            OX, OY, YX, YY);

        if (Dist < Threshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Y;
        }
    }

    if (bZ && GizmoZComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D(
            static_cast<float>(MouseX), static_cast<float>(MouseY),
            OX, OY, ZX, ZY);

        if (Dist < Threshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Z;
        }
    }


    return BestAxis;
}

float FApplication::DistancePointToSegment2D(float Px, float Py, float Ax, float Ay, float Bx, float By) const
{
    const float ABx = Bx - Ax;
    const float ABy = By - Ay;
    const float APx = Px - Ax;
    const float APy = Py - Ay;

    const float ABLenSq = ABx * ABx + ABy * ABy;
    if (ABLenSq <= 0.000001f)
    {
        const float Dx = Px - Ax;
        const float Dy = Py - Ay;
        return std::sqrt(Dx * Dx + Dy * Dy);
    }

    float T = (APx * ABx + APy * ABy) / ABLenSq;
    if (T < 0.0f) T = 0.0f;
    if (T > 1.0f) T = 1.0f;

    const float ClosestX = Ax + ABx * T;
    const float ClosestY = Ay + ABy * T;

    const float Dx = Px - ClosestX;
    const float Dy = Py - ClosestY;
    return std::sqrt(Dx * Dx + Dy * Dy);
}

bool FApplication::IntersectRayPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, FVector& OutHitPoint) const
{
    const float Denom = PlaneNormal.Dot(Ray.Direction);
    if (std::fabs(Denom) < 0.000001f)
    {
        return false;
    }

    const float T = PlaneNormal.Dot(PlanePoint - Ray.Origin) / Denom;
    if (T < 0.0f)
    {
        return false;
    }

    OutHitPoint = Ray.Origin + Ray.Direction * T;
    return true;
}

void FApplication::BeginGizmoDrag(EGizmoAxis Axis, int MouseX, int MouseY)
{
    if (!SelectedActor)
    {
        return;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root)
    {
        return;
    }

    FVector AxisDir = FVector::ZeroVector;
    switch (Axis)
    {
    case EGizmoAxis::X: AxisDir = FVector(1.0f, 0.0f, 0.0f); break;
    case EGizmoAxis::Y: AxisDir = FVector(0.0f, 1.0f, 0.0f); break;
    case EGizmoAxis::Z: AxisDir = FVector(0.0f, 0.0f, 1.0f); break;
    default: return;
    }

    const FMatrix CamRot = FMatrix::MakeRotationXYZ(MainCamera->GetRelativeRotation());
    const FVector4 Forward4 = CamRot * FVector4(0.0f, 0.0f, 1.0f, 0.0f);
    const FVector CameraForward(Forward4.X, Forward4.Y, Forward4.Z);

    FVector PlaneNormal = AxisDir.Cross(CameraForward.Cross(AxisDir));
    PlaneNormal.Normalize();

    if (PlaneNormal.LengthSquared() < 0.000001f)
    {
        return;
    }

    FRay Ray = BuildPickRay(MouseX, MouseY);

    FVector HitPoint;
    if (!IntersectRayPlane(Ray, Root->GetRelativeLocation(), PlaneNormal, HitPoint))
    {
        return;
    }

    bDraggingGizmo = true;
    ActiveGizmoAxis = Axis;
    DragAxisDirection = AxisDir;
    DragPlaneNormal = PlaneNormal;
    DragStartActorLocation = Root->GetRelativeLocation();
    DragStartHitPoint = HitPoint;
}

void FApplication::UpdateGizmoDrag(int MouseX, int MouseY)
{
    if (!bDraggingGizmo || !SelectedActor)
    {
        return;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root)
    {
        return;
    }

    FRay Ray = BuildPickRay(MouseX, MouseY);

    FVector HitPoint;
    if (!IntersectRayPlane(Ray, DragStartActorLocation, DragPlaneNormal, HitPoint))
    {
        return;
    }

    const FVector Delta = HitPoint - DragStartHitPoint;
    const float MoveAmount = Delta.Dot(DragAxisDirection);

    const FVector NewLocation = DragStartActorLocation + DragAxisDirection * MoveAmount;
    Root->SetRelativeLocation(NewLocation);
}

void FApplication::EndGizmoDrag()
{
    bDraggingGizmo = false;
    ActiveGizmoAxis = EGizmoAxis::None;
    DragAxisDirection = FVector::ZeroVector;
    DragPlaneNormal = FVector::ZeroVector;
}

void FApplication::AddSelectionOutlineRenderItem()
{
    if (!SelectedActor || !Scene)
    {
        return;
    }

    UStaticMeshComponent* MeshComp = FindStaticMeshComponent(SelectedActor);
    if (!MeshComp || !MeshComp->GetStaticMesh() || !MeshComp->IsVisible())
    {
        return;
    }

    const FVector BaseScale = MeshComp->GetRelativeScale();
    const FVector OutlineScale(
        BaseScale.X * 1.02f,
        BaseScale.Y * 1.02f,
        BaseScale.Z * 1.02f);

    const FMatrix Scale = FMatrix::MakeScale(OutlineScale);
    const FMatrix Rotation = FMatrix::MakeRotationXYZ(MeshComp->GetRelativeRotation());
    const FMatrix Translation = FMatrix::MakeTranslation(MeshComp->GetRelativeLocation());

    FRenderItem OutlineItem;
    OutlineItem.Mesh = MeshComp->GetStaticMesh();
    OutlineItem.WorldMatrix = Scale * Rotation * Translation;
    OutlineItem.Color = FVector4(0.953f, 0.596f, 0.184f, 1.0f);

    // backface 확장 outline 핵심
    OutlineItem.CullMode = ERenderCullMode::Back;
    OutlineItem.bDepthEnable = true;
    OutlineItem.bUseVertexColor = false;

    Scene->RenderItems.push_back(OutlineItem);
}

void FApplication::UpdateGizmoTransform()
{
    if (!SelectedActor || !GizmoXComp || !GizmoYComp || !GizmoZComp)
    {
        return;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root)
    {
        return;
    }

    const FVector Pos = Root->GetRelativeLocation();

    GizmoXComp->SetRelativeLocation(Pos);
    GizmoYComp->SetRelativeLocation(Pos);
    GizmoZComp->SetRelativeLocation(Pos);
}

void FApplication::UpdateGizmoColors()
{
    if (!GizmoXComp || !GizmoYComp || !GizmoZComp)
    {
        return;
    }

    const FVector4 XBase(1.0f, 0.0f, 0.0f, 1.0f);
    const FVector4 YBase(0.0f, 1.0f, 0.0f, 1.0f);
    const FVector4 ZBase(0.0f, 0.45f, 1.0f, 1.0f);

    EGizmoAxis HighlightAxis = EGizmoAxis::None;

    if (bDraggingGizmo)
    {
        HighlightAxis = ActiveGizmoAxis;
    }
    else
    {
        int MouseX = 0;
        int MouseY = 0;
        WindowApp->GetMousePosition(MouseX, MouseY);
        HighlightAxis = PickGizmoAxis(MouseX, MouseY);
    }

    GizmoXComp->SetRenderColor(
        HighlightAxis == EGizmoAxis::X ? LightenColor(XBase, 0.45f) : XBase);

    GizmoYComp->SetRenderColor(
        HighlightAxis == EGizmoAxis::Y ? LightenColor(YBase, 0.45f) : YBase);

    GizmoZComp->SetRenderColor(
        HighlightAxis == EGizmoAxis::Z ? LightenColor(ZBase, 0.45f) : ZBase);
}

void FApplication::SetGizmoVisibility(bool bVisible)
{
    if (GizmoXComp) GizmoXComp->SetVisibility(bVisible);
    if (GizmoYComp) GizmoYComp->SetVisibility(bVisible);
    if (GizmoZComp) GizmoZComp->SetVisibility(bVisible);
}

bool FApplication::ComputePointerPulseWorldPosition(int MouseX, int MouseY, float Distance, FVector& OutWorldPos) const
{
    if (!MainCamera)
    {
        return false;
    }

    FRay Ray = BuildPickRay(MouseX, MouseY);

    const FVector CamLoc = MainCamera->GetRelativeLocation();
    const FVector CamRot = MainCamera->GetRelativeRotation();

    const FMatrix RotMatrix = FMatrix::MakeRotationXYZ(CamRot);
    const FVector4 Forward4 = RotMatrix * FVector4(0, 0, 1, 0);
    const FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);

    const FVector PlanePoint = CamLoc + Forward * Distance;

    return IntersectRayPlane(Ray, PlanePoint, Forward, OutWorldPos);
}

void FApplication::BeginPointerPulse(int MouseX, int MouseY)
{
    PointerPulse.Phase = EPointerPulsePhase::Growing;
    PointerPulse.bDragDetected = false;
    PointerPulse.bMouseStillDown = true;
    PointerPulse.StartMouseX = MouseX;
    PointerPulse.StartMouseY = MouseY;
    PointerPulse.CurrentRadius = 0.0f;

    if (ClickCircleComp)
    {
        ClickCircleComp->SetVisibility(true);
        RefreshPointerPulseTransform();
    }
}

void FApplication::EndPointerPulse()
{
    PointerPulse.bMouseStillDown = false;

    // 드래그였다면 바로 shrink 시작
    if (PointerPulse.bDragDetected &&
        PointerPulse.Phase != EPointerPulsePhase::Hidden)
    {
        PointerPulse.Phase = EPointerPulsePhase::Shrinking;
    }
}

void FApplication::RefreshPointerPulseTransform()
{
    if (!ClickCircleComp || !MainCamera)
    {
        return;
    }
    int MouseX = PointerPulse.StartMouseX;
    int MouseY = PointerPulse.StartMouseY;

    // 드래그 중이면 현재 마우스 위치를 따라가게 함
    if (PointerPulse.bDragDetected && PointerPulse.bMouseStillDown)
    {
        WindowApp->GetMousePosition(MouseX, MouseY);
    }

    FVector WorldPos;
    if (!ComputePointerPulseWorldPosition(
        MouseX,
        MouseY,
        PointerPulse.OverlayDistance,
        WorldPos))
    {
        return;
    }

    ClickCircleComp->SetRelativeLocation(WorldPos);
    ClickCircleComp->SetRelativeRotation(MainCamera->GetRelativeRotation());

    const float S = PointerPulse.CurrentRadius;
    ClickCircleComp->SetRelativeScale(FVector(S, S, S));
}

void FApplication::UpdatePointerPulse(float DeltaTime)
{
    if (!ClickCircleComp)
    {
        return;
    }

    if (PointerPulse.Phase == EPointerPulsePhase::Hidden)
    {
        ClickCircleComp->SetVisibility(false);
        return;
    }

    int MouseX = 0;
    int MouseY = 0;
    WindowApp->GetMousePosition(MouseX, MouseY);

    const int DragDX = MouseX - PointerPulse.StartMouseX;
    const int DragDY = MouseY - PointerPulse.StartMouseY;
    const int DragThreshold = 6;

    if (PointerPulse.bMouseStillDown &&
        !PointerPulse.bDragDetected &&
        (DragDX * DragDX + DragDY * DragDY) >= (DragThreshold * DragThreshold))
    {
        PointerPulse.bDragDetected = true;
    }

    switch (PointerPulse.Phase)
    {
    case EPointerPulsePhase::Growing:
    {
        PointerPulse.CurrentRadius += PointerPulse.GrowSpeed * DeltaTime;

        if (PointerPulse.CurrentRadius >= PointerPulse.MaxRadius)
        {
            PointerPulse.CurrentRadius = PointerPulse.MaxRadius;

            if (PointerPulse.bDragDetected && PointerPulse.bMouseStillDown)
            {
                PointerPulse.Phase = EPointerPulsePhase::Holding;
            }
            else if (!PointerPulse.bMouseStillDown)
            {
                // 클릭: 이미 뗐더라도 max까지 커진 뒤 shrink
                PointerPulse.Phase = EPointerPulsePhase::Shrinking;
            }
        }
        break;
    }
    case EPointerPulsePhase::Holding:
    {
        PointerPulse.CurrentRadius = PointerPulse.MaxRadius;

        if (!PointerPulse.bMouseStillDown)
        {
            PointerPulse.Phase = EPointerPulsePhase::Shrinking;
        }
        break;
    }
    case EPointerPulsePhase::Shrinking:
    {
        PointerPulse.CurrentRadius -= PointerPulse.ShrinkSpeed * DeltaTime;

        if (PointerPulse.CurrentRadius <= 0.0f)
        {
            PointerPulse.CurrentRadius = 0.0f;
            PointerPulse.Phase = EPointerPulsePhase::Hidden;
            ClickCircleComp->SetVisibility(false);
            return;
        }
        break;
    }
    case EPointerPulsePhase::Hidden:
    default:
        return;
    }

    RefreshPointerPulseTransform();
}

// 상혁 테스트 
void FApplication::UpdateObjectAllocationTest()
{
    if (TestIntervalCounter++ > TestInterval)
    {
        if (TestDelta > 0)
        {
            auto NewObject = GUObjectFactory.CreateObject<AActor>("Test");
            TestObjects.push_back(NewObject);
        }
        else
        {
            if (!TestObjects.empty())
            {
                UObject* Garbage = TestObjects.back();
                TestObjects.pop_back();

                GUObjectArray.FreeUObjectIndox(Garbage);
                GUObjectAllocator.FreeUObject(Garbage);
            }
        }

        if (TestObjects.size() <= 0 || TestObjects.size() > 100)
        {
            TestDelta *= -1;
        }

        TestIntervalCounter = 0;
    }
}
void FApplication::RenderDebugUI()
{
    ImGui::Begin("Jungle Property Window");
    ImGui::Text("Hello Jungle World!");

    ImGui::Text("GTotalAllocationBytes: %d", FMemory::GetTotalAllocatedMemory());
    ImGui::Text("GTotalAllocationCount: %d", GUObjectArray.ElementalCount);
    ImGui::Text("ObjectCountInVector: %d", static_cast<int>(TestObjects.size()));

    if (!TestObjects.empty())
    {
        ImGui::Text("LastID: %d", TestObjects.back()->GetUUID());
    }

    ImGui::End();
}

