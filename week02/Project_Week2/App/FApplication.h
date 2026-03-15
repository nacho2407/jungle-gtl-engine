#pragma once

#include <windows.h>
#include "FRay.h"
#include "Structs.h"

class UWorld;
class URenderer;
class UCameraComponent;
class AActor;
class FScene;
class FWindowsApplication;
class UStaticMesh;

class UStaticMeshComponent;

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

private:
    FWindowsApplication* WindowApp;
    URenderer* Renderer;
    UWorld* World;
    FScene* Scene;

    AActor* CameraActor = nullptr;
    UCameraComponent* MainCamera;
    // @@@ 액터의 mesh 별로 이렇게 하나씩 다 선언하는 게 맞음?? 하나로 편하게 관리 못하나?
    // 지금 Spawn이 아니라 미리 만들어놓아야 해서 이렇게 한건가?
    UStaticMesh* CubeMesh; 
    UStaticMesh* SphereMesh;
    //UStaticMesh* TriangleMesh;
    UStaticMesh* AxesMesh;
    UStaticMesh* GizmoArrowMesh = nullptr;

    AActor* GizmoActor = nullptr;
    UStaticMeshComponent* GizmoXComp = nullptr;
    UStaticMeshComponent* GizmoYComp = nullptr;
    UStaticMeshComponent* GizmoZComp = nullptr;
    //UStaticMeshComponent* GizmoMeshComp = nullptr;
    AActor* SelectedActor = nullptr;

    AActor* WorldAxesActor = nullptr;

    bool bIsRunning;

    bool bDraggingGizmo = false;
    EGizmoAxis ActiveGizmoAxis = EGizmoAxis::None;

    FVector DragStartActorLocation = FVector::ZeroVector;
    FVector DragStartHitPoint = FVector::ZeroVector;
    FVector DragAxisDirection = FVector::ZeroVector;
    FVector DragPlaneNormal = FVector::ZeroVector;
};