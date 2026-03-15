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
    //, CubeMesh(nullptr)
    //, TriangleMesh(nullptr)
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
    //SphereMesh = MeshImporter::LoadStaticMeshFromGltf("Assets/BlueSphere.gltf");
    SphereMesh = BuiltInMeshFactory::CreateSphereMesh();
    CubeMesh = BuiltInMeshFactory::CreateCubeMesh();
    //TriangleMesh = BuiltInMeshFactory::CreateTriangleMesh();
    AxesMesh = BuiltInMeshFactory::CreateAxesMesh();

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
    {
        GizmoActor = new AActor();
        GizmoMeshComp = new UStaticMeshComponent();
        GizmoMeshComp->SetStaticMesh(AxesMesh);
        GizmoMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
        GizmoMeshComp->SetVisibility(false); // 처음엔 숨김
        GizmoActor->AddComponent(GizmoMeshComp);
        GizmoActor->SetRootComponent(GizmoMeshComp);
        World->AddActor(GizmoActor);
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
    if (!World)
    {
        return;
    }

    int MouseX = 0;
    int MouseY = 0;

    int DownX = 0;
    int DownY = 0;
    if (WindowApp->ConsumeLeftMouseDown(DownX, DownY))
    {
        EGizmoAxis Axis = PickGizmoAxis(DownX, DownY);
        if (Axis != EGizmoAxis::None)
        {
            BeginGizmoDrag(Axis, DownX, DownY);
        }
        else
        {
            FRay Ray = BuildPickRay(DownX, DownY);
            AActor* HitActor = PickActor(Ray);
            SetSelectedActor(HitActor);
        }
    }

    int UpX = 0;
    int UpY = 0;
    if (WindowApp->ConsumeLeftMouseUp(UpX, UpY))
    {
        EndGizmoDrag();
    }

    if (bDraggingGizmo && WindowApp->IsLeftMousePressed())
    {
        WindowApp->GetMousePosition(MouseX, MouseY);
        UpdateGizmoDrag(MouseX, MouseY);
    }

    World->Tick(DeltaTime);

    if (SelectedActor && GizmoMeshComp)
    {
        USceneComponent* Root = SelectedActor->GetRootComponent();
        if (Root)
        {
            GizmoMeshComp->SetRelativeLocation(Root->GetRelativeLocation());
            GizmoMeshComp->SetRelativeRotation(Root->GetRelativeRotation());
        }
    }

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

    const std::vector<AActor*>& Actors = World->GetActors();

    for (AActor* Actor : Actors)
    {
        if (!Actor || Actor == CameraActor || Actor == GizmoActor || Actor == WorldAxesActor)
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

    const std::vector<UActorComponent*>& Components = Actor->GetComponents();
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
        if (GizmoMeshComp)
        {
            GizmoMeshComp->SetVisibility(false);
        }
        return;
    }

    SelectedActor = NewSelected;

    if (!SelectedActor)
    {
        if (GizmoMeshComp)
        {
            GizmoMeshComp->SetVisibility(false);
        }
        return;
    }

    USceneComponent* Root = SelectedActor->GetRootComponent();
    if (!Root || !GizmoMeshComp)
    {
        return;
    }

    GizmoMeshComp->SetRelativeLocation(Root->GetRelativeLocation());
    GizmoMeshComp->SetRelativeRotation(Root->GetRelativeRotation());
    GizmoMeshComp->SetVisibility(true);
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
    if (!SelectedActor || !GizmoMeshComp || !GizmoMeshComp->IsVisible())
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

    if (bX)
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, XX, XY);
        if (Dist < Threshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::X;
        }
    }

    if (bY)
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, YX, YY);
        if (Dist < Threshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Y;
        }
    }

    if (bZ)
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, ZX, ZY);
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