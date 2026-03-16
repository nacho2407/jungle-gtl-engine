#include "Editor/FEditorSceneBuilder.h"

#include "WindowsApplication.h"

#include "World/UWorld.h"
#include "Actor/AActor.h"
#include "Component/UCameraComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"
#include "Structs.h"

bool FEditorSceneBuilder::Build(
    UWorld* World,
    FWindowsApplication* WindowApp,
    const FBuiltInMeshes& Meshes,
    AActor*& OutCameraActor,
    UCameraComponent*& OutMainCamera,
    AActor*& OutWorldAxesActor,
    AActor*& OutGridActor,
    AActor*& OutGizmoActor,
    UStaticMeshComponent*& OutGizmoXComp,
    UStaticMeshComponent*& OutGizmoYComp,
    UStaticMeshComponent*& OutGizmoZComp,
    AActor*& OutClickCircleActor,
    UStaticMeshComponent*& OutClickCircleComp
)
{
    if (!World || !WindowApp)
    {
        return false;
    }

    // Camera
    OutCameraActor = new AActor();
    OutMainCamera = new UCameraComponent();

    OutMainCamera->SetRelativeLocation(FVector(2.0f, 4.0f, -7.0f));
    OutMainCamera->SetRelativeRotation(FVector(0.3f, 0.0f, 0.0f));
    OutMainCamera->SetFieldOfView(39.6f);
    OutMainCamera->SetAspectRatio(
        static_cast<float>(WindowApp->GetClientWidth()) /
        static_cast<float>(WindowApp->GetClientHeight()));
    OutMainCamera->SetNearClip(0.1f);
    OutMainCamera->SetFarClip(1000.0f);

    OutCameraActor->AddComponent(OutMainCamera);
    OutCameraActor->SetRootComponent(OutMainCamera);
    World->AddActor(OutCameraActor);

    // Sphere 1
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.SphereMesh);
        MeshComp->SetRelativeLocation(FVector(-2.0f, -2.0f, 5.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Sphere 2
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.SphereMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 2.0f, 10.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Cube
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.CubeMesh);
        MeshComp->SetRelativeLocation(FVector(2.0f, 0.0f, 5.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // Torus
    {
        AActor* Actor = new AActor();
        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.TorusMesh);
        MeshComp->SetRelativeLocation(FVector(3.0f, 0.0f, 8.0f));
        MeshComp->SetRelativeScale(FVector(1.5f, 1.5f, 1.5f));
        MeshComp->SetRelativeRotation(FVector(0.6f, 0.0f, 0.0f));
        Actor->AddComponent(MeshComp);
        Actor->SetRootComponent(MeshComp);
        World->AddActor(Actor);
    }

    // World Axes
    {
        OutWorldAxesActor = new AActor();

        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.AxesMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

        OutWorldAxesActor->AddComponent(MeshComp);
        OutWorldAxesActor->SetRootComponent(MeshComp);
        World->AddActor(OutWorldAxesActor);
    }

    // Grid
    OutGridActor = nullptr;
    if (Meshes.GridMesh)
    {
        OutGridActor = new AActor();

        UStaticMeshComponent* MeshComp = new UStaticMeshComponent();
        MeshComp->SetStaticMesh(Meshes.GridMesh);
        MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

        OutGridActor->AddComponent(MeshComp);
        OutGridActor->SetRootComponent(MeshComp);
        World->AddActor(OutGridActor);
    }

    // Gizmo
    {
        OutGizmoActor = new AActor();

        OutGizmoXComp = new UStaticMeshComponent();
        OutGizmoYComp = new UStaticMeshComponent();
        OutGizmoZComp = new UStaticMeshComponent();

        OutGizmoXComp->SetStaticMesh(Meshes.GizmoArrowMesh);
        OutGizmoYComp->SetStaticMesh(Meshes.GizmoArrowMesh);
        OutGizmoZComp->SetStaticMesh(Meshes.GizmoArrowMesh);

        OutGizmoXComp->SetRelativeLocation(FVector::ZeroVector);
        OutGizmoYComp->SetRelativeLocation(FVector::ZeroVector);
        OutGizmoZComp->SetRelativeLocation(FVector::ZeroVector);

        OutGizmoXComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
        OutGizmoYComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
        OutGizmoZComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

        OutGizmoXComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
        OutGizmoYComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
        OutGizmoZComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));

        OutGizmoXComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
        OutGizmoYComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
        OutGizmoZComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

        OutGizmoXComp->SetVisibility(false);
        OutGizmoYComp->SetVisibility(false);
        OutGizmoZComp->SetVisibility(false);

        OutGizmoActor->AddComponent(OutGizmoXComp);
        OutGizmoActor->AddComponent(OutGizmoYComp);
        OutGizmoActor->AddComponent(OutGizmoZComp);
        OutGizmoActor->SetRootComponent(OutGizmoXComp);

        World->AddActor(OutGizmoActor);
    }

    // Click Pulse Circle
    {
        OutClickCircleActor = new AActor();
        OutClickCircleComp = new UStaticMeshComponent();

        OutClickCircleComp->SetStaticMesh(Meshes.ClickCircleMesh);
        OutClickCircleComp->SetRelativeLocation(FVector::ZeroVector);
        OutClickCircleComp->SetRelativeRotation(FVector::ZeroVector);
        OutClickCircleComp->SetRelativeScale(FVector(0.0f, 0.0f, 0.0f));

        OutClickCircleComp->SetRenderColor(FVector4(0.98f, 0.84f, 0.10f, 1.0f));
        OutClickCircleComp->SetUseVertexColor(false);
        OutClickCircleComp->SetDepthEnable(false);
        OutClickCircleComp->SetCullMode(ERenderCullMode::None);
        OutClickCircleComp->SetVisibility(false);

        OutClickCircleActor->AddComponent(OutClickCircleComp);
        OutClickCircleActor->SetRootComponent(OutClickCircleComp);

        World->AddActor(OutClickCircleActor);
    }

    UStaticMesh* CreateGridMesh(int HalfCount, float Spacing)
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGridVertices(HalfCount, Spacing));
        Mesh->SetTopology(EMeshTopology::LineList);
        return Mesh;
    }

    return true;
}