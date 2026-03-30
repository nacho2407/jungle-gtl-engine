#include "Core/CoreMinimal.h"
#include "Engine/Game/QuadActor.h"
#include "Engine/Component/Mesh/QuadComponent.h"

AQuadActor::AQuadActor()
{
    auto* SceneRoot = new Engine::Component::USceneComponent();
    SceneRoot->Name = "DefaultSceneRoot";
    AddOwnedComponent(SceneRoot, true);

    QuadComponent = new Engine::Component::UQuadComponent();
    QuadComponent->Name = "QuadComponent";

    AddOwnedComponent(QuadComponent, false);
    QuadComponent->AttachToComponent(SceneRoot);

    Name = "QuadActor";
}

AQuadActor::~AQuadActor() {}

REGISTER_CLASS(, AQuadActor)
