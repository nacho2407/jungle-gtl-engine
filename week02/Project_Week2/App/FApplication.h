#pragma once

#include <windows.h>
#include "FRay.h"
#include "Structs.h"
#include "Actor/ACamera.h"
#include "Actor/AGridActor.h"
#include "Actor/AAxisActor.h"
#include "Component/EGizmoMode.h"
#include "World/ESpawnMeshType.h"

#include "Math/FQuat.h"

class UWorld;
class URenderer;
class UCameraComponent;
class AActor;
class FScene;
class FWindowsApplication;
class UStaticMesh;

class UStaticMeshComponent;
class AGizmoActor;

class FGUIManager;
class FInputManager;

class FPropertyPanel;
class FControlPanel;

enum class EPointerPulsePhase
{
    Hidden,
    Growing,
    Holding,
    Shrinking
};

struct FPointerPulse
{
    EPointerPulsePhase Phase = EPointerPulsePhase::Hidden;

    bool bDragDetected = false;
    bool bMouseStillDown = false;

    int StartMouseX = 0;
    int StartMouseY = 0;

    int CurrentMouseX = 0;
    int CurrentMouseY = 0;

    float CurrentRadius = 0.0f;
    float MaxRadius = 0.02f;      // 월드 단위
    float GrowSpeed = 0.5f;       // radius/sec
    float ShrinkSpeed = 2.4f;     // radius/sec

    float OverlayDistance = 2.0f; // 카메라 앞 몇 유닛에 붙일지
};

class FApplication
{
public:
    FApplication();
    ~FApplication();

public:
    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();

    // 패널 렌더링
    AActor* GetSelectedActor() const;
    void NotifySelectedActorTransformChanged();
    void ClearSelection();

    UCameraComponent* GetMainCamera() const;

    bool bUseOrthogonalProjection = false;
    float DebugOrthoWidth = 10.0f;

    void ApplyCameraProjectionMode();

    ESpawnMeshType SelectedSpawnMeshType = ESpawnMeshType::Sphere;
    void SpawnSelectedMeshActor();

    // Scene save / load 관련 함수들
    const FString& GetSceneFileNameInput() const;
    void SetSceneFileNameInput(const FString& InSceneFileName);
    
    void NewScene();
    bool SaveScene();
    bool LoadScene();

private:
    bool InitializeEngine();
    bool InitializeGUI();
    bool InitializeInput();
    bool InitializeResources();
    bool InitializeScene();
    void MainLoop();
    void Tick(float DeltaTime);
    void RenderFrame();

    void HandleMousePicking();
    FRay BuildPickRay(int MouseX, int MouseY) const;
    AActor* PickActor(const FRay& Ray) const;
    bool IntersectRaySphere(const FRay& Ray, const FVector& Center, float Radius, float& OutT) const;
    UStaticMeshComponent* FindStaticMeshComponent(AActor* Actor) const;
    void SetSelectedActor(AActor* NewSelected);

    bool ProjectWorldToScreen(const FVector& WorldPos, float& OutX, float& OutY) const;
    EGizmoAxis PickGizmoAxis(int MouseX, int MouseY) const;
    float DistancePointToSegment2D(float Px, float Py, float Ax, float Ay, float Bx, float By) const;

    bool IntersectRayPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, FVector& OutHitPoint) const;
    void BeginGizmoDrag(EGizmoAxis Axis, int MouseX, int MouseY);
    void UpdateGizmoDrag(int MouseX, int MouseY);
    void EndGizmoDrag();

    void AddSelectionOutlineRenderItem();

    void UpdateGizmoTransform();
    void UpdateGizmoColors();
    void SetGizmoVisibility(bool bVisible);

    void BeginPointerPulse(int MouseX, int MouseY);
    void UpdatePointerPulse(float DeltaTime);
    void EndPointerPulse();
    bool ComputePointerPulseWorldPosition(int MouseX, int MouseY, float Distance, FVector& OutWorldPos) const;
    void RefreshPointerPulseTransform();

    // ClickCircle 생성해서 월드에 추가하는 함수인데, 이 기능 자체를 빼는 걸 고려중
    void CreatePointerPulseActor();

    // 상혁 테스트
    void UpdateObjectAllocationTest();
    void RenderDebugUI();

	// 패널 렌더링
    void RenderEditorUI();

    void TempFunc(AActor* actor);

    void CycleGizmoMode();

    float SignedAngleAroundAxis(
        const FVector& From,
        const FVector& To,
        const FVector& Axis) const;
    


    // Scene save / load 파일 경로 생성 관련 함수
    FString BuildSceneFilePath() const;
    

private:
    FWindowsApplication* WindowApp;
    URenderer* Renderer;
    UWorld* World;
    FScene* Scene;

    UStaticMesh* CubeMesh;
    UStaticMesh* TorusMesh;
    UStaticMesh* AxesMesh;
    UStaticMesh* GizmoArrowMesh = nullptr;
    UStaticMesh* GizmoScaleMesh = nullptr;

    //AActor* GizmoActor = nullptr;
    //UStaticMeshComponent* GizmoXComp = nullptr;
    //UStaticMeshComponent* GizmoYComp = nullptr;
    //UStaticMeshComponent* GizmoZComp = nullptr;
    //UStaticMeshComponent* GizmoMeshComp = nullptr;
    AGizmoActor* GizmoActor = nullptr;
    AActor* SelectedActor = nullptr;

    AActor* WorldAxesActor;

    bool bIsRunning;

    bool bDraggingGizmo = false;
    EGizmoAxis ActiveGizmoAxis = EGizmoAxis::None;

    FVector DragStartActorLocation = FVector::ZeroVector;
    FVector DragStartHitPoint = FVector::ZeroVector;
    FVector DragAxisDirection = FVector::ZeroVector;
    FVector DragPlaneNormal = FVector::ZeroVector;

    UStaticMesh* ClickCircleMesh = nullptr;
    AActor* ClickCircleActor = nullptr;
    UStaticMeshComponent* ClickCircleComp = nullptr;

    UStaticMesh* GridMesh = nullptr;

    FPointerPulse PointerPulse;

    // 상혁 테스트
    int TestDelta = 1;
    int TestInterval = 5;
    int TestIntervalCounter = 0;
    TArray<UObject*> TestObjects;


    FGUIManager* GUIManager = nullptr;

	// 패널 렌더링
    FPropertyPanel* PropertyPanel = nullptr;
    FControlPanel* ControlPanel = nullptr;

    // 하단 콘솔
    bool bShowBottomConsole = true;

    FInputManager* InputManager = nullptr;

    int PrevMouseX = 0;
    int PrevMouseY = 0;
    
    // Scene save / load에 사용할 파일 이름(확장자 제외)
    FString SceneFileNameInput = "Default";

    ACamera* Camera;
    AAxisActor* WorldAxisActor;
    AGridActor* GridActor;

    EGizmoMode CurrentGizmoMode = EGizmoMode::Translate;

    UStaticMesh* GizmoRotateRingMesh = nullptr;

    EGizmoMode DragStartGizmoMode = EGizmoMode::Translate;
    FVector DragStartActorRotation = FVector::ZeroVector;
    FVector DragStartVectorOnPlane = FVector::ZeroVector;

    FQuat DragStartActorQuat = FQuat::Identity();
    FVector DragStartActorScale = FVector::OneVector;
    float GizmoScaleSensitivity = 0.35f; // 모드별 감도

    FMatrix DragStartActorWorldMatrix = FMatrix::Identity;
    FVector DragStartActorWorldPivot = FVector::ZeroVector;
};