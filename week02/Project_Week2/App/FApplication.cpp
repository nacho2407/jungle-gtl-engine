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

#include "GUI.h"
#include "Actor/ASphere.h"
#include "Actor/ACube.h"

#include "GUI.h"

#include <chrono>
#include "Actor/AGizmoActor.h"
#include "FGUIManager.h"
#include "FInputManager.h"

#include "Panels/FPropertyPanel.h"
#include "Panels/FControlPanel.h"
#include "Panels/FConsolePanel.h"

#include "FJsonConverter.h"
#include "FWorldSaveConverter.h"

#include <filesystem>

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

    bool DrawFloat3Control(const char* Label, FVector& Value, float Speed = 0.1f)
    {
        bool bChanged = false;

        ImGui::PushID(Label);

        ImGui::SetNextItemWidth(90.0f);
        bChanged |= ImGui::DragFloat("##X", &Value.X, Speed);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        bChanged |= ImGui::DragFloat("##Y", &Value.Y, Speed);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        bChanged |= ImGui::DragFloat("##Z", &Value.Z, Speed);

        ImGui::SameLine();
        ImGui::Text("%s", Label);

        ImGui::PopID();
        return bChanged;
    }
}

FApplication::FApplication()
    : WindowApp(nullptr)
    , Renderer(nullptr)
    , World(nullptr)
    , Scene(nullptr)
    //, CameraActor(nullptr)
    //, MainCamera(nullptr)
    , bIsRunning(false)
    , CubeMesh(nullptr)
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

    if (!InitializeGUI())
    {
        return false;
    }

    if (!InitializeInput())
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

    // 실시간 화면 변경 대응을 위해 여기로 돌림
    WindowApp->OnResize = [this](int Width, int Height)
    {
        if (Width <= 0 || Height <= 0 || !Renderer)
        {
            return;
        }

        Renderer->Resize(static_cast<UINT>(Width), static_cast<UINT>(Height));

        if (Camera && Camera->GetCameraComponent())
        {
            Camera->GetCameraComponent()->UpdateAspectRatio(
                static_cast<uint32>(Width),
                static_cast<uint32>(Height));
        }

        RenderFrame();
    };

    PropertyPanel = new FPropertyPanel();
    ControlPanel = new FControlPanel();

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

    World = NewObject<UWorld>();
    Scene = new FScene();

    return true;
}

bool FApplication::InitializeGUI()
{
    GUIManager = new FGUIManager();
    if (!GUIManager)
    {
        return false;
    }

    return GUIManager->Initialize(WindowApp, Renderer);
}

bool FApplication::InitializeInput()
{
    InputManager = new FInputManager();
    if (!InputManager)
    {
        return false;
    }

    return InputManager->Initialize(WindowApp, GUIManager);
}

bool FApplication::InitializeResources()
{
    CubeMesh = BuiltInMeshFactory::CreateCubeMesh();
    TorusMesh = BuiltInMeshFactory::CreateTorusMesh(64, 32, 1.2f, 0.35f);
    AxesMesh = BuiltInMeshFactory::CreateAxesMesh();
    GridMesh = BuiltInMeshFactory::CreateGridMesh(200, 1.0f);
    GizmoArrowMesh = BuiltInMeshFactory::CreateGizmoArrowMesh();
    GizmoScaleMesh = BuiltInMeshFactory::CreateGizmoScaleMesh();
    GizmoRotateRingMesh = BuiltInMeshFactory::CreateGizmoRotateRingMesh();

    //ClickCircleMesh = BuiltInMeshFactory::CreateCircleMesh(64);
    ClickCircleMesh = BuiltInMeshFactory::CreateDiscMesh(64);

    return true;
}

bool FApplication::InitializeScene()
{
    // (*) ACamereActor로 빼기
    // 카메라 액터
 //   CameraActor = new AActor();
 //   MainCamera = new UCameraComponent();
 //   // 카메라가 바라보는 월드 수정 
 //   MainCamera->SetRelativeLocation(FVector(2.0f, 4.0f, -7.0f));
 //   MainCamera->SetRelativeRotation(FVector(0.3f, 0.0f, 0.0f)); // Pitch Yaw Roll
 //   MainCamera->SetFieldOfView(39.6f);
 //   MainCamera->SetAspectRatio(
 //       static_cast<float>(WindowApp->GetClientWidth()) /
 //       static_cast<float>(WindowApp->GetClientHeight()));
 //   MainCamera->SetNearClip(0.1f);
 //   MainCamera->SetFarClip(1000.0f);
 //   bUseOrthogonalProjection = false;
	//DebugOrthoWidth = 10.0f;
	//ApplyCameraProjectionMode();

 //   CameraActor->AddComponent(MainCamera);
 //   CameraActor->SetRootComponent(MainCamera);
    // (*) ACamereActor로 빼기

    //World->AddActor(CameraActor);
    
    World->SpawnMeshActor(ESpawnMeshType::Sphere, FVector(0.0f, 0.0f, 0.0f));

    //// World Axes
    //{
    //    WorldAxesActor = new AActor();

    //    UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
    //    MeshComp->SetStaticMesh(AxesMesh);
    //    MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    //    WorldAxesActor->AddComponent(MeshComp);
    //    WorldAxesActor->SetRootComponent(MeshComp);
    //    World->AddActor(WorldAxesActor);
    //}
    // 마우스 클릭 펄스 생성 및 World 추가
    CreatePointerPulseActor();

    UE_LOG("Hello World %d", 2026);

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
    //using Clock = std::chrono::steady_clock; // @@@ (+) 이거로 교체해보는 거 고려해보셈
    auto PrevTime = Clock::now();

    Camera = NewObject<ACamera>();
    WorldAxisActor = NewObject<AAxisActor>();
    GizmoActor = NewObject<AGizmoActor>();
    GridActor = NewObject<AGridActor>();

    check(Camera)
    check(WorldAxisActor)
    check(GizmoActor)
    check(GridActor)

    if (UStaticMeshComponent* AxesComp = FindStaticMeshComponent(WorldAxisActor))
    {
        AxesComp->SetDepthEnable(true);
        AxesComp->SetUseDepthBias(false);
        AxesComp->SetCullMode(ERenderCullMode::None);
    }

    GizmoActor->Initialize(GizmoArrowMesh, GizmoScaleMesh, GizmoRotateRingMesh);
    GizmoActor->SetMode(CurrentGizmoMode);

    // @@@ VSync 있는거임??
    // 의도적으로 프레임을 조정할 수 있는 거는 필요없나?
    while (bIsRunning)
    {
        if (!WindowApp->PumpMessages()) // OS 입력,창 이벤트 반영
        {
            bIsRunning = false;
            break;
        }

        auto CurrentTime = Clock::now();
        std::chrono::duration<float> Delta = CurrentTime - PrevTime;
        PrevTime = CurrentTime;

        Tick(Delta.count()); // 엔진 상태 업데이트
        RenderFrame(); // 화면 출력
    }
}

void FApplication::Tick(float DeltaTime)
{
    Scene->Clear();

    auto MainCamera = Camera->GetCameraComponent();
    //auto GizmoActor = World->GizmoActor;
    if (!World || !MainCamera || !InputManager)
    {
        return;
    }

    UpdateObjectAllocationTest();

    InputManager->BeginFrame();

    const bool bCanProcessMouse = InputManager->CanProcessMouse();
    const bool bCanProcessKeyboard = InputManager->CanProcessKeyboard();

    if (bCanProcessKeyboard &&
        !bDraggingGizmo &&
        InputManager->WasKeyPressed(VK_SPACE) &&
        SelectedActor &&
        GizmoActor &&
        GizmoActor->GetTargetActor())
    {
        CycleGizmoMode();
    }

    // Delete키로 선택된 Object 삭제
    if (bCanProcessKeyboard &&
        InputManager->WasKeyPressed(VK_DELETE) &&
        SelectedActor)
    {
        AActor* ActorToDelete = SelectedActor;
        EndGizmoDrag();
        SetSelectedActor(nullptr);
        World->Destroy(ActorToDelete);
    }

    int MouseX = 0;
    int MouseY = 0;
    InputManager->GetMousePosition(MouseX, MouseY);

    if (bCanProcessMouse && InputManager->WasMousePressed(EMouseButton::Left))
    {
        BeginPointerPulse(MouseX, MouseY);

        FHitProxy Proxy = Renderer->PickPrimitiveProxy(MouseX, MouseY);

        AActor* HitActor = nullptr;
        if (Proxy.Type == EHitProxyType::Primitive && Proxy.Primitive)
        {
            HitActor = Proxy.Primitive->GetOwner();
        }

        if (HitActor == GizmoActor)
        {
            EGizmoAxis Axis = PickGizmoAxis(MouseX, MouseY);
            if (Axis != EGizmoAxis::None)
            {
                BeginGizmoDrag(Axis, MouseX, MouseY);
            }
        }
        else
        {
            if (HitActor == WorldAxisActor || HitActor == GridActor || HitActor == ClickCircleActor)
            {
                HitActor = nullptr;
            }
            else
            {
                FHitProxy Proxy = Renderer->PickPrimitiveProxy(MouseX, MouseY);

                AActor* HitActor = nullptr;

                if (Proxy.Type == EHitProxyType::Primitive && Proxy.Primitive)
                {
                    HitActor = Proxy.Primitive->GetOwner();

                    if (HitActor == GizmoActor || HitActor == WorldAxisActor || HitActor == GridActor)
                    {
                        HitActor = nullptr;
                    }
                }

                SetSelectedActor(HitActor);
            }
        }
    }

    if (bCanProcessMouse && InputManager->WasMouseReleased(EMouseButton::Left))
    {
        EndGizmoDrag();
        EndPointerPulse();
    }
    check(Camera)
        check(WorldAxisActor)
        check(GizmoActor)
        check(GridActor)
    if (bCanProcessMouse && bDraggingGizmo && InputManager->IsMouseDown(EMouseButton::Left))
    {
        InputManager->GetMousePosition(MouseX, MouseY);
        UpdateGizmoDrag(MouseX, MouseY);
    }

    if (bCanProcessMouse && InputManager->WasMousePressed(EMouseButton::Right))
    {
        BeginPointerPulse(MouseX, MouseY);
        PrevMouseX = MouseX;
        PrevMouseY = MouseY;
    }

    if (bCanProcessMouse && InputManager->WasMouseReleased(EMouseButton::Right))
    {
        EndPointerPulse();
    }

    if (bCanProcessMouse && InputManager->IsMouseDown(EMouseButton::Right))
    {
        InputManager->GetMousePosition(MouseX, MouseY);

        const int DeltaX = MouseX - PrevMouseX;
        const int DeltaY = MouseY - PrevMouseY;

        FVector CamRot = MainCamera->GetRelativeRotation();
        const float RotateSpeed = 0.005f;

        CamRot.Y += DeltaX * RotateSpeed;
        CamRot.X += DeltaY * RotateSpeed;
        CamRot.X = std::clamp(CamRot.X, -1.5f, 1.5f);

        MainCamera->SetRelativeRotation(CamRot);

        PrevMouseX = MouseX;
        PrevMouseY = MouseY;
    }
    else
    {
        InputManager->GetMousePosition(PrevMouseX, PrevMouseY);
    }

    FVector CameraLoc = MainCamera->GetRelativeLocation();
    FVector Rot = MainCamera->GetRelativeRotation();

    FMatrix RotMatrix = FMatrix::MakeRotationXYZ(Rot);
    check(Camera)
        check(WorldAxisActor)
        check(GizmoActor)
        check(GridActor)
    FVector4 Forward4 = RotMatrix * FVector4(0, 0, 1, 0);
    FVector4 Right4 = RotMatrix * FVector4(1, 0, 0, 0);
    FVector4 Up4 = RotMatrix * FVector4(0, 1, 0, 0);

    FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);
    FVector Right(Right4.X, Right4.Y, Right4.Z);
    FVector Up(Up4.X, Up4.Y, Up4.Z);

    const float Speed = 5.0f * DeltaTime;

    if (bCanProcessKeyboard)
    {
        if (InputManager->IsKeyDown('W')) CameraLoc += Forward * Speed;
        if (InputManager->IsKeyDown('S')) CameraLoc -= Forward * Speed;
        if (InputManager->IsKeyDown('A')) CameraLoc -= Right * Speed;
        if (InputManager->IsKeyDown('D')) CameraLoc += Right * Speed;
        if (InputManager->IsKeyDown('Q')) CameraLoc -= Up * Speed;
        if (InputManager->IsKeyDown('E')) CameraLoc += Up * Speed;
    }

    MainCamera->SetRelativeLocation(CameraLoc);

    const float Wheel = InputManager->GetMouseWheelDelta();
    if (bCanProcessMouse && Wheel != 0.0f)
    {
        FVector Loc = MainCamera->GetRelativeLocation();
        Loc += Forward * Wheel * 2.0f;
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
    TempFunc(WorldAxisActor);
    TempFunc(GridActor);
    TempFunc(GizmoActor);

    AddSelectionOutlineRenderItem();

    InputManager->EndFrame();
}

void FApplication::TempFunc(AActor* Actor)
{
    check(Actor)
    const TArray<UActorComponent*>& Components = Actor->GetComponents();
    for (UActorComponent* Component : Components)
    {
        // @@@ 아직 이거는 Primitive 외에 확장성은 고려하지 않은건가? <- BuildScene이라 필요없네ㅎ,ㅎ
        UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
        if (!PrimitiveComponent)
        {
            continue;
        }

        if (!PrimitiveComponent->IsVisible())
        {
            continue;
        }

        //FRenderItem Item = PrimitiveComponent->CreateRenderItem();
        //if (Item.Mesh == nullptr)
        //{
        //    continue;
        //}

        //if (dynamic_cast<AGizmoActor*>(Actor))
        //{
        //    Item.bIsGizmo = true;
        //}

        //Scene->RenderItems.push_back(Item);
        FRenderItem Item = PrimitiveComponent->CreateRenderItem();
        if (Item.Mesh == nullptr)
        {
            continue;
        }

        // world matrix 3x3 determinant
        const FMatrix& M = Item.WorldMatrix;
        const float Det3x3 =
            M.M[0][0] * (M.M[1][1] * M.M[2][2] - M.M[1][2] * M.M[2][1]) -
            M.M[0][1] * (M.M[1][0] * M.M[2][2] - M.M[1][2] * M.M[2][0]) +
            M.M[0][2] * (M.M[1][0] * M.M[2][1] - M.M[1][1] * M.M[2][0]);

        if (Det3x3 < 0.0f)
        {
            if (Item.CullMode == ERenderCullMode::Back)
            {
                Item.CullMode = ERenderCullMode::Front;
            }
            else if (Item.CullMode == ERenderCullMode::Front)
            {
                Item.CullMode = ERenderCullMode::Back;
            }
        }

        if (dynamic_cast<AGizmoActor*>(Actor))
        {
            Item.bIsGizmo = true;
        }
        Scene->RenderItems.push_back(Item);
    }
}

void FApplication::RenderFrame()
{
    auto MainCamera = Camera->GetCameraComponent();
    if (!Renderer || !Scene)
    {
        return;
    }

    Renderer->BeginFrame();
    Renderer->Render(*Scene, MainCamera);

    // 여기서 메인 백버퍼 다시d 바인딩
    Renderer->BindMainRenderTargetForOverlay();

    //Renderer->BeginOverlayRenderState(); // 오버레이 변경 -> Gizmo, Mesh 오버레이 무시
    //Renderer->RenderGizmo(*Scene, Maincamera); //
    //Renderer->EndOverlayRenderState(); // 

    GUIManager->BeginFrame();
    RenderDebugUI();
    RenderEditorUI();
    GUIManager->EndFrame();


    Renderer->EndFrame();
}

void FApplication::Shutdown()
{
    bIsRunning = false;

    if (PropertyPanel)
    {
        delete PropertyPanel;
        PropertyPanel = nullptr;
    }

    if (ControlPanel)
    {
        delete ControlPanel;
        ControlPanel = nullptr;
    }

    if (GUIManager)
    {
        delete GUIManager;
        GUIManager = nullptr;
    }

    if (InputManager)
    {
        delete InputManager;
        InputManager = nullptr;
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
}

const FString& FApplication::GetSceneFileNameInput() const
{
    return SceneFileNameInput;
}

void FApplication::SetSceneFileNameInput(const FString& InSceneFileName)
{
    SceneFileNameInput = InSceneFileName;
}

void FApplication::NewScene()
{
    if (!World)
    {
        return;
    }

    // 현재 선택된 Actor를 선택 해제하여 Gizmo 등 해제..?
    SetSelectedActor(nullptr);
    World->Clear();
    
    // World를 초기화하기 때문에 일단 마우스 펄스도 월드에 다시 추가
    CreatePointerPulseActor();
}

bool FApplication::SaveScene()
{
    if (!World)
    {
        return false;
    }

    const FWorldSaveData SaveData = FWorldSaveConverter::FromWorld(World);
    return FJsonConverter::SaveToFile(BuildSceneFilePath(), SaveData, 2);
}

bool FApplication::LoadScene()
{
    if (!World)
    {
        return false;
    }

    FWorldSaveData SaveData;

    // 멤버 변수인 SceneFileNameInput를 이용해서 파일 저장 위치 확인
    if (!FJsonConverter::LoadFromFile(BuildSceneFilePath(), SaveData))
    {
        return false;
    }

    SetSelectedActor(nullptr);

    if (!FWorldSaveConverter::ToWorld(SaveData, World))
    {
        return false;
    }

    // FWorldSaveConverter::ToWorld는 World 초기화 후 Scene 파일에 있는 Actor들만 World에 추가하기 때문에 마우스 펄스도 여기서 다시 추가
    CreatePointerPulseActor();
    return true;
}

void FApplication::HandleMousePicking()
{
    auto MainCamera = Camera->GetCameraComponent();

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
    auto MainCamera = Camera->GetCameraComponent();

    FRay Ray;

    const float Width = static_cast<float>(WindowApp->GetClientWidth());
    const float Height = static_cast<float>(WindowApp->GetClientHeight());

    const float NdcX = (2.0f * static_cast<float>(MouseX) / Width) - 1.0f;
    const float NdcY = 1.0f - (2.0f * static_cast<float>(MouseY) / Height);

    const FVector CameraLocation = MainCamera->GetRelativeLocation();
    const FVector CameraRotation = MainCamera->GetRelativeRotation();

    const FMatrix CameraRotationMatrix = FMatrix::MakeRotationXYZ(CameraRotation);

    const FVector4 Right4 = CameraRotationMatrix * FVector4(1.0f, 0.0f, 0.0f, 0.0f);
    const FVector4 Up4 = CameraRotationMatrix * FVector4(0.0f, 1.0f, 0.0f, 0.0f);
    const FVector4 Forward4 = CameraRotationMatrix * FVector4(0.0f, 0.0f, 1.0f, 0.0f);
    
    const FVector Right(Right4.X, Right4.Y, Right4.Z);
    const FVector Up(Up4.X, Up4.Y, Up4.Z);
    const FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);

    if (MainCamera->IsOrthogonal())
    {
        const float OrthoWidth = MainCamera->GetOrthoWidth();
        const float OrthoHeight = OrthoWidth / MainCamera->GetAspectRatio();

        const FVector RayOrigin =
            CameraLocation +
            Right * (NdcX * (OrthoWidth * 0.5f)) +
            Up * (NdcY * (OrthoHeight * 0.5f));

        Ray.Origin = RayOrigin;
        Ray.Direction = Forward;
        Ray.Direction.Normalize();
        return Ray;
    }

    const float FovRadians = MainCamera->GetFieldOfView() * 3.14159265358979323846f / 180.0f;
    const float TanHalfFov = std::tan(FovRadians * 0.5f);

    const float ViewX = NdcX * MainCamera->GetAspectRatio() * TanHalfFov;
    const float ViewY = NdcY * TanHalfFov;

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
    auto MainCamera = Camera->GetCameraComponent();

    const TArray<AActor*>& Actors = World->GetActors();
    //auto GizmoActor = World->GizmoActor;

    for (AActor* Actor : Actors)
    {
        // @@@ Actor==GizmoActor라는데, 이거 XYZ로 나누면서 nullptr 아닌가?
        if (!Actor || Actor == Camera || Actor == GizmoActor || Actor == WorldAxisActor || Actor == ClickCircleActor || Actor == GridActor)
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
    //auto GizmoActor = World->GizmoActor;

    if (SelectedActor == NewSelected)
    {
        SelectedActor = nullptr;
        if (GizmoActor)
        {
            GizmoActor->SetTargetActor(nullptr);
        }
        return;
    }

    SelectedActor = NewSelected;

    if (!SelectedActor)
    {
        if (GizmoActor)
        {
            GizmoActor->SetTargetActor(nullptr);
        }
        return;
    }

    if (GizmoActor)
    {
        GizmoActor->SetTargetActor(SelectedActor);
        GizmoActor->UpdateColors(EGizmoAxis::None);
    }
}

bool FApplication::ProjectWorldToScreen(const FVector& WorldPos, float& OutX, float& OutY) const
{
    auto MainCamera = Camera->GetCameraComponent();

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
    //auto GizmoActor = World->GizmoActor;

    auto MainCamera = Camera->GetCameraComponent();

    if (!GizmoActor || !MainCamera || !WindowApp)
    {
        return EGizmoAxis::None;
    }

    return GizmoActor->PickAxis(
        MouseX,
        MouseY,
        MainCamera,
        WindowApp->GetClientWidth(),
        WindowApp->GetClientHeight());
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

    FVector LocalAxis = FVector::ZeroVector;
    switch (Axis)
    {
    case EGizmoAxis::X: LocalAxis = FVector(1.0f, 0.0f, 0.0f); break;
    case EGizmoAxis::Y: LocalAxis = FVector(0.0f, 1.0f, 0.0f); break;
    case EGizmoAxis::Z: LocalAxis = FVector(0.0f, 0.0f, 1.0f); break;
    default: return;
    }

    FVector AxisDir = LocalAxis;

    if (CurrentGizmoMode == EGizmoMode::Scale)
    {
        const FMatrix ActorWorld = Root->GetWorldTransformMatrix();
        const FVector4 Axis4 = ActorWorld * FVector4(LocalAxis, 0.0f);
        AxisDir = FVector(Axis4.X, Axis4.Y, Axis4.Z);
    }

    AxisDir.Normalize();

    const FMatrix ActorWorld = Root->GetWorldTransformMatrix();
    const FVector ActorWorldPivot = ActorWorld.GetTranslation();

    FRay Ray = BuildPickRay(MouseX, MouseY);

    DragStartGizmoMode = CurrentGizmoMode;
    ActiveGizmoAxis = Axis;
    DragAxisDirection = AxisDir;

    DragStartActorLocation = ActorWorldPivot;
    DragStartActorWorldPivot = ActorWorldPivot;
    DragStartActorWorldMatrix = ActorWorld;

    DragStartActorRotation = Root->GetRelativeRotation();
    DragStartActorQuat = Root->GetRelativeRotationQuat();
    DragStartActorScale = Root->GetRelativeScale();

    if (CurrentGizmoMode == EGizmoMode::Rotate)
    {
        FVector HitPoint;
        if (!IntersectRayPlane(Ray, ActorWorldPivot, AxisDir, HitPoint))
        {
            return;
        }

        FVector StartVec = HitPoint - ActorWorldPivot;
        StartVec = StartVec - AxisDir * StartVec.Dot(AxisDir);

        if (StartVec.LengthSquared() < 0.000001f)
        {
            return;
        }

        StartVec.Normalize();

        bDraggingGizmo = true;
        DragPlaneNormal = AxisDir;
        DragStartVectorOnPlane = StartVec;
        DragStartHitPoint = HitPoint;
        return;
    }

    auto MainCamera = Camera->GetCameraComponent();
    if (!MainCamera)
    {
        return;
    }

    const FMatrix CamRot = FMatrix::MakeRotationXYZ(MainCamera->GetRelativeRotation());
    const FVector4 Forward4 = CamRot * FVector4(0.0f, 0.0f, 1.0f, 0.0f);
    FVector CameraForward(Forward4.X, Forward4.Y, Forward4.Z);
    CameraForward.Normalize();

    FVector PlaneNormal = AxisDir.Cross(CameraForward.Cross(AxisDir));
    if (PlaneNormal.LengthSquared() < 0.000001f)
    {
        return;
    }
    PlaneNormal.Normalize();

    FVector HitPoint;
    if (!IntersectRayPlane(Ray, ActorWorldPivot, PlaneNormal, HitPoint))
    {
        return;
    }

    bDraggingGizmo = true;
    DragPlaneNormal = PlaneNormal;
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
    if (!IntersectRayPlane(Ray, DragStartActorWorldPivot, DragPlaneNormal, HitPoint))
    {
        return;
    }

    const FVector Pivot = DragStartActorWorldPivot;
    const FMatrix ToPivot = FMatrix::MakeTranslation(FVector(-Pivot.X, -Pivot.Y, -Pivot.Z));
    const FMatrix FromPivot = FMatrix::MakeTranslation(Pivot);
    // (*) HitPoint - DragStartActorWorldPivot; 다 중복임
    if (DragStartGizmoMode == EGizmoMode::Rotate)
    {
        FVector CurrentVec = HitPoint - DragStartActorWorldPivot;
        CurrentVec = CurrentVec - DragAxisDirection * CurrentVec.Dot(DragAxisDirection);

        if (CurrentVec.LengthSquared() < 0.000001f)
        {
            return;
        }

        CurrentVec.Normalize();

        const float DeltaAngle = SignedAngleAroundAxis(
            DragStartVectorOnPlane,
            CurrentVec,
            DragAxisDirection);

        const FQuat DeltaQuat = FQuat::FromAxisAngle(DragAxisDirection, DeltaAngle);
        const FMatrix DeltaRot = DeltaQuat.ToMatrix();

        const FMatrix NewWorld =
            DragStartActorWorldMatrix * ToPivot * DeltaRot * FromPivot;

        Root->SetWorldTransformMatrix(NewWorld);
    }
    else if (DragStartGizmoMode == EGizmoMode::Translate)
    {
        const FVector Delta = HitPoint - DragStartHitPoint;
        const float MoveAmount = Delta.Dot(DragAxisDirection);
        const FVector WorldDelta = DragAxisDirection * MoveAmount;

        const FMatrix DeltaTranslation = FMatrix::MakeTranslation(WorldDelta);
        const FMatrix NewWorld = DragStartActorWorldMatrix * DeltaTranslation;

        Root->SetWorldTransformMatrix(NewWorld);
    }
    else if (DragStartGizmoMode == EGizmoMode::Scale)
    {
        const FVector Delta = HitPoint - DragStartHitPoint;
        const float Amount = Delta.Dot(DragAxisDirection);

        //const float MinScale = 0.05f;
        FVector NewScale = DragStartActorScale;

        switch (ActiveGizmoAxis)
        {
        case EGizmoAxis::X:
            NewScale.X = DragStartActorScale.X + Amount * GizmoScaleSensitivity;
            break;
        case EGizmoAxis::Y:
            NewScale.Y = DragStartActorScale.Y + Amount * GizmoScaleSensitivity;
            break;
        case EGizmoAxis::Z:
            NewScale.Z = DragStartActorScale.Z + Amount * GizmoScaleSensitivity;
            break;
        }

        Root->SetRelativeScale(NewScale);
    }

    if (GizmoActor)
    {
        GizmoActor->UpdateTransformFromTarget();
        GizmoActor->UpdateColors(ActiveGizmoAxis);
    }
}

void FApplication::EndGizmoDrag()
{
    bDraggingGizmo = false;
    ActiveGizmoAxis = EGizmoAxis::None;
    DragAxisDirection = FVector::ZeroVector;
    DragPlaneNormal = FVector::ZeroVector;
    DragStartVectorOnPlane = FVector::ZeroVector;
    DragStartActorRotation = FVector::ZeroVector;
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

    const FMatrix BaseWorld = MeshComp->GetWorldTransformMatrix();
    const FMatrix OutlineScale = FMatrix::MakeScale(FVector(1.04f, 1.04f, 1.04f));

    FRenderItem OutlineItem;
    OutlineItem.Mesh = MeshComp->GetStaticMesh();
    OutlineItem.WorldMatrix = OutlineScale * BaseWorld;
    OutlineItem.Color = FVector4(1.0f, 0.333f, 0.0f, 1.0f);

    //OutlineItem.CullMode = ERenderCullMode::Front;
    const float Det3x3 =
        BaseWorld.M[0][0] * (BaseWorld.M[1][1] * BaseWorld.M[2][2] - BaseWorld.M[1][2] * BaseWorld.M[2][1]) -
        BaseWorld.M[0][1] * (BaseWorld.M[1][0] * BaseWorld.M[2][2] - BaseWorld.M[1][2] * BaseWorld.M[2][0]) +
        BaseWorld.M[0][2] * (BaseWorld.M[1][0] * BaseWorld.M[2][1] - BaseWorld.M[1][1] * BaseWorld.M[2][0]);

    OutlineItem.CullMode = (Det3x3 < 0.0f)
        ? ERenderCullMode::Back
        : ERenderCullMode::Front;

    OutlineItem.bDepthEnable = true;
    OutlineItem.bUseVertexColor = false;

    Scene->RenderItems.push_back(OutlineItem);
}

void FApplication::UpdateGizmoTransform()
{
    UCameraComponent* MainCamera = GetMainCamera();
    if (!GizmoActor || !MainCamera || !WindowApp)
    {
        return;
    }

    GizmoActor->UpdateTransformFromTarget();
    GizmoActor->UpdateConstantScreenScale(
        MainCamera,
        WindowApp->GetClientWidth(),
        WindowApp->GetClientHeight());
}

void FApplication::UpdateGizmoColors()
{
    //auto GizmoActor = World->GizmoActor;

    if (!GizmoActor)
    {
        return;
    }

    EGizmoAxis HighlightAxis = EGizmoAxis::None;

    if (bDraggingGizmo)
    {
        HighlightAxis = ActiveGizmoAxis;
    }
    //else
    //{
    //    int MouseX = 0;
    //    int MouseY = 0;
    //    InputManager->GetMousePosition(MouseX, MouseY);
    //    HighlightAxis = PickGizmoAxis(MouseX, MouseY);
    //}

    GizmoActor->UpdateColors(HighlightAxis);
}

void FApplication::SetGizmoVisibility(bool bVisible)
{
    //auto GizmoActor = World->GizmoActor;

    if (GizmoActor)
    {
        GizmoActor->SetGizmoVisible(bVisible);
    }
}

bool FApplication::ComputePointerPulseWorldPosition(int MouseX, int MouseY, float Distance, FVector& OutWorldPos) const
{
    auto MainCamera = Camera->GetCameraComponent();

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

void FApplication::BeginPointerPulse(int MouseX, int MouseY) // &&&
{
    PointerPulse.Phase = EPointerPulsePhase::Growing;
    PointerPulse.bDragDetected = false;
    PointerPulse.bMouseStillDown = true;
    PointerPulse.StartMouseX = MouseX;
    PointerPulse.StartMouseY = MouseY;
    PointerPulse.CurrentMouseX = MouseX;
    PointerPulse.CurrentMouseY = MouseY;

    PointerPulse.CurrentRadius = 0.0f;

    if (ClickCircleComp)
    {
        ClickCircleComp->SetVisibility(true);
        RefreshPointerPulseTransform(); // &&& 안 본 거 
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
    auto MainCamera = Camera->GetCameraComponent();

    if (!ClickCircleComp || !MainCamera)
    {
        return;
    }
    int MouseX = PointerPulse.StartMouseX;
    int MouseY = PointerPulse.StartMouseY;

    // 드래그 중이면 현재 마우스 위치를 따라가게 함
    if (PointerPulse.bDragDetected)
    {
        MouseX = PointerPulse.CurrentMouseX;
        MouseY = PointerPulse.CurrentMouseY;
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

void FApplication::CreatePointerPulseActor()
{
    // 원래 있던 마우스 펄스 생성 부분 복붙
    if (!World || !ClickCircleMesh)
    {
        return;
    }

    ClickCircleActor = NewObject<AActor>();
    ClickCircleComp = NewObject<UStaticMeshComponent>();

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

FString FApplication::BuildSceneFilePath() const
{
    namespace fs = std::filesystem;

    // 입력된 파일 이름이 없으면 그냥 "Default"로 판단
    fs::path ScenePath(SceneFileNameInput.empty() ? "Default" : SceneFileNameInput);
    if (ScenePath.extension().empty())
    {
        // 확장자가 있으면 그걸로 쓰고, 없으면 기본 ".Scene" 사용
        ScenePath += ".Scene";
    }

    return ScenePath.string();
}

void FApplication::UpdatePointerPulse(float DeltaTime)
{
    if (!ClickCircleComp || !InputManager)
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

    PointerPulse.CurrentMouseX = MouseX;
    PointerPulse.CurrentMouseY = MouseY;

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
            UObject* testObject = NewObject<AActor>("Test");
            TestObjects.push_back(testObject);
        }
        else
        {
            if (!TestObjects.empty())
            {
                UObject* Garbage = TestObjects.back();
                TestObjects.pop_back();

                DestroyObject(Garbage);
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
    ImGui::Begin("Jungle Property Window(Debug)");

    ImGui::Text("GTotalAllocationBytes: %d", FMemory::GetTotalAllocatedMemory());
    ImGui::Text("GTotalAllocationCount: %d", GUObjectArray.ElementalCount);
    ImGui::Text("ObjectCountInVector: %d", static_cast<int>(TestObjects.size()));

    auto test = NewObject<AActor>("Temp");
    auto msg = "Typeof :" + test->GetClass()->ClassName;
    ImGui::Text(msg.c_str());

    DestroyObject(test);
    if (!TestObjects.empty())
    {
        ImGui::Text("LastID: %d", TestObjects.back()->GetUUID());
    }

    ImGui::End();

}

void FApplication::SpawnSelectedMeshActor()
{
    if (!World)
    {
        return;
    }

    const int SpawnCount = (NumberOfSpawn < 1) ? 1 : NumberOfSpawn;

    AActor* LastSpawnedActor = nullptr;

    for (int i = 0; i < SpawnCount; ++i)
    {
        LastSpawnedActor = World->SpawnMeshActor(SelectedSpawnMeshType, FVector::ZeroVector);
    }

    if (LastSpawnedActor)
    {
        SetSelectedActor(LastSpawnedActor);
    }
}

void FApplication::RenderEditorUI()
{
    if (PropertyPanel)
    {
        PropertyPanel->Render(this);
    }

    if (ControlPanel)
    {
        ControlPanel->Render(this);
    }

    if (bShowBottomConsole && WindowApp)
    {
        const float ConsoleHeight = 220.0f;
        const float Width = static_cast<float>(WindowApp->GetClientWidth());
        const float Height = static_cast<float>(WindowApp->GetClientHeight());

        ImGui::SetNextWindowPos(ImVec2(0.0f, Height - ConsoleHeight), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(Width, ConsoleHeight), ImGuiCond_Once);

        ShowImGuiDemoConsole(&bShowBottomConsole);
    }
}

AActor* FApplication::GetSelectedActor() const
{
    return SelectedActor;
}

void FApplication::NotifySelectedActorTransformChanged()
{
    if (SelectedActor)
    {
        UpdateGizmoTransform();
        UpdateGizmoColors();
    }
}

void FApplication::ClearSelection()
{
    SetSelectedActor(nullptr);
}

UCameraComponent* FApplication::GetMainCamera() const
{
    auto MainCamera = Camera->GetCameraComponent();

    return MainCamera;

}

bool FApplication::IsVSyncEnabled() const
{
    return Renderer && Renderer->IsVSyncEnabled();
}

void FApplication::SetVSyncEnabled(bool bEnabled)
{
    if (Renderer)
    {
        Renderer->SetVSyncEnabled(bEnabled);
    }
}

void FApplication::ApplyCameraProjectionMode()
{
    auto MainCamera = Camera->GetCameraComponent();

    if (!MainCamera) { return; }
	MainCamera->SetProjectionMode(bUseOrthogonalProjection ? EProjectionMode::Orthogonal : EProjectionMode::Perspective);
	MainCamera->SetOrthoWidth(DebugOrthoWidth);
}

void FApplication::CycleGizmoMode()
{
    switch (CurrentGizmoMode)
    {
    case EGizmoMode::Translate:
        SetGizmoMode(EGizmoMode::Rotate);
        break;
    case EGizmoMode::Rotate:
        SetGizmoMode(EGizmoMode::Scale);
        break;
    case EGizmoMode::Scale:
    default:
        SetGizmoMode(EGizmoMode::Translate);
        break;
    }
}

float FApplication::SignedAngleAroundAxis(
    const FVector& From,
    const FVector& To,
    const FVector& Axis) const
{
    FVector NFrom = From.GetNormalized();
    FVector NTo = To.GetNormalized();
    FVector NAxis = Axis.GetNormalized();

    const float SinValue = NAxis.Dot(NFrom.Cross(NTo));
    const float CosValue = NFrom.Dot(NTo);

    return std::atan2(SinValue, CosValue);
}

EGizmoMode FApplication::GetCurrentGizmoMode() const
{
    return CurrentGizmoMode;
}

void FApplication::SetGizmoMode(EGizmoMode InMode)
{
    if (CurrentGizmoMode == InMode)
    {
        return;
    }

    CurrentGizmoMode = InMode;

    if (GizmoActor)
    {
        GizmoActor->SetMode(CurrentGizmoMode);
        GizmoActor->UpdateColors(EGizmoAxis::None);
        GizmoActor->UpdateTransformFromTarget();
    }
}