#pragma once

#include "Resource/FBuiltInMeshes.h"

class UWorld;
class FWindowsApplication;
class AActor;
class UCameraComponent;
class UStaticMeshComponent;

class FEditorSceneBuilder
{
public:
    bool Build(
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
        UStaticMesh* CreateGridMesh(int HalfCount = 10, float Spacing = 1.0f);
    );
};