#include "Core/CoreMinimal.h"
#include "Engine/Game/StaticMeshActor.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"

AStaticMeshActor::AStaticMeshActor()
{
    // 1. Create a persistent root component (Unreal's DefaultSceneRoot style)
    auto* SceneRoot = new Engine::Component::USceneComponent();
    SceneRoot->Name = "DefaultSceneRoot";
    AddOwnedComponent(SceneRoot, true); // 'true' makes it the RootComponent

    // 2. Create the StaticMeshComponent as a child, allowing it to be removed by the user
    StaticMeshComponent = new Engine::Component::UStaticMeshComponent();
    StaticMeshComponent->Name = "StaticMeshComponent";

    // Logic: No slots initially. They appear only when a Static Mesh is assigned.

    AddOwnedComponent(StaticMeshComponent, false); // Not a root, so it's removable

    // Establishes parent-child relationship in the scene hierarchy
    StaticMeshComponent->AttachToComponent(SceneRoot);

    Name = "StaticMeshActor";
}

AStaticMeshActor::~AStaticMeshActor() {}

REGISTER_CLASS(, AStaticMeshActor)
