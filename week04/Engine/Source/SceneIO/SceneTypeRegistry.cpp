#include "SceneTypeRegistry.h"

#include "Engine/Component/Mesh/ConeComponent.h"
#include "Engine/Component/Mesh/CubeComponent.h"
#include "Engine/Component/Mesh/CylinderComponent.h"
#include "Engine/Component/Mesh/QuadComponent.h"
#include "Engine/Component/Mesh/RingComponent.h"
#include "Engine/Component/Mesh/SphereComponent.h"
#include "Engine/Component/Mesh/TriangleComponent.h"
#include "Engine/Component/Sprite/AtlasComponent.h"
#include "Engine/Component/Sprite/SpriteComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"
#include "Engine/Component/Sprite/SubUVAnimatedComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Core/UnknownComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/ConeActor.h"
#include "Engine/Game/CubeActor.h"
#include "Engine/Game/CylinderActor.h"
#include "Engine/Game/EffectActor.h"
#include "Engine/Game/RingActor.h"
#include "Engine/Game/SphereActor.h"
#include "Engine/Game/SpriteActor.h"
#include "Engine/Game/TextActor.h"
#include "Engine/Game/TriangleActor.h"
#include "Engine/Game/AtlasSpriteActor.h"
#include "Engine/Game/FlipbookActor.h"
#include "Engine/Game/UnknownActor.h"

#include <typeindex>

namespace
{
    struct FActorTypeInfo
    {
        FString                  TypeName;
        std::function<AActor*()> Factory;
    };

    struct FComponentTypeInfo
    {
        FString                                              TypeName;
        std::function<Engine::Component::USceneComponent*()> Factory;
    };

    struct FRegistryStorage
    {
        TMap<FString, FActorTypeInfo>  ActorTypesByName;
        TMap<std::type_index, FString> ActorTypeNamesByIndex;

        TMap<FString, FComponentTypeInfo> ComponentTypesByName;
        TMap<std::type_index, FString>    ComponentTypeNamesByIndex;
    };

    FRegistryStorage& GetRegistryStorage()
    {
        static FRegistryStorage Storage;
        return Storage;
    }

    template <typename TActor> void RegisterActorType(const FString& TypeName)
    {
        FRegistryStorage& Storage = GetRegistryStorage();
        Storage.ActorTypesByName[TypeName] =
            FActorTypeInfo{.TypeName = TypeName, .Factory = []() { return new TActor(); }};
        Storage.ActorTypeNamesByIndex[std::type_index(typeid(TActor))] = TypeName;
    }

    template <typename TComponent> void RegisterComponentType(const FString& TypeName)
    {
        FRegistryStorage& Storage = GetRegistryStorage();
        Storage.ComponentTypesByName[TypeName] =
            FComponentTypeInfo{.TypeName = TypeName, .Factory = []() { return new TComponent(); }};
        Storage.ComponentTypeNamesByIndex[std::type_index(typeid(TComponent))] = TypeName;
    }

    void EnsureSceneTypesRegistered()
    {
        static bool bRegistered = false;
        if (bRegistered)
        {
            return;
        }

        bRegistered = true;

        RegisterActorType<AConeActor>("AConeActor");
        RegisterActorType<ACubeActor>("ACubeActor");
        RegisterActorType<ACylinderActor>("ACylinderActor");
        RegisterActorType<AEffectActor>("AEffectActor");
        RegisterActorType<ARingActor>("ARingActor");
        RegisterActorType<ASphereActor>("ASphereActor");
        RegisterActorType<ASpriteActor>("ASpriteActor");
        RegisterActorType<ATextActor>("ATextActor");
        RegisterActorType<ATriangleActor>("ATriangleActor");
        RegisterActorType<AFlipbookActor>("AFlipbookActor");
        RegisterActorType<AAtlasSpriteActor>("AAtlasSpriteActor");
        RegisterActorType<AUnknownActor>("AUnknownActor");

        RegisterComponentType<Engine::Component::UConeComponent>("UConeComponent");
        RegisterComponentType<Engine::Component::UCubeComponent>("UCubeComponent");
        RegisterComponentType<Engine::Component::UCylinderComponent>("UCylinderComponent");
        RegisterComponentType<Engine::Component::UQuadComponent>("UQuadComponent");
        RegisterComponentType<Engine::Component::URingComponent>("URingComponent");
        RegisterComponentType<Engine::Component::USphereComponent>("USphereComponent");
        RegisterComponentType<Engine::Component::UTriangleComponent>("UTriangleComponent");
        RegisterComponentType<Engine::Component::USpriteComponent>("USpriteComponent");
        RegisterComponentType<Engine::Component::UAtlasComponent>("UAtlasComponent");
        RegisterComponentType<Engine::Component::USubUVComponent>("USubUVComponent");
        RegisterComponentType<Engine::Component::USubUVComponent>("USubUVAnimatedComponent");
        RegisterComponentType<Engine::Component::UAtlasTextComponent>("UTextComponent");
        RegisterComponentType<Engine::Component::UAtlasTextComponent>("UAtlasTextComponent");
        RegisterComponentType<Engine::Component::UUnknownComponent>("UUnknownComponent");
    }
} // namespace

FString FSceneTypeRegistry::ResolveActorTypeName(const AActor& Actor)
{
    EnsureSceneTypesRegistered();

    if (Actor.IsA(AUnknownActor::GetClass()))
    {
        const auto& UnknownActor = static_cast<const AUnknownActor&>(Actor);
        if (!UnknownActor.GetOriginalTypeName().empty())
        {
            return UnknownActor.GetOriginalTypeName();
        }
    }

    const FRegistryStorage& Storage = GetRegistryStorage();
    const auto Iterator = Storage.ActorTypeNamesByIndex.find(std::type_index(typeid(Actor)));
    if (Iterator != Storage.ActorTypeNamesByIndex.end())
    {
        return Iterator->second;
    }

    return typeid(Actor).name();
}

FString
FSceneTypeRegistry::ResolveComponentTypeName(const Engine::Component::USceneComponent& Component)
{
    EnsureSceneTypesRegistered();

    if (Component.IsA(Engine::Component::UUnknownComponent::GetClass()))
    {
        const auto& UnknownComponent =
            static_cast<const Engine::Component::UUnknownComponent&>(Component);
        if (!UnknownComponent.GetOriginalTypeName().empty())
        {
            return UnknownComponent.GetOriginalTypeName();
        }
    }

    const FRegistryStorage& Storage = GetRegistryStorage();
    const auto              Iterator =
        Storage.ComponentTypeNamesByIndex.find(std::type_index(typeid(Component)));
    if (Iterator != Storage.ComponentTypeNamesByIndex.end())
    {
        return Iterator->second;
    }

    return typeid(Component).name();
}

AActor* FSceneTypeRegistry::ConstructActor(const FString& TypeName, bool* OutKnownType)
{
    EnsureSceneTypesRegistered();

    const FRegistryStorage& Storage = GetRegistryStorage();
    const auto              Iterator = Storage.ActorTypesByName.find(TypeName);
    if (Iterator != Storage.ActorTypesByName.end())
    {
        if (OutKnownType != nullptr)
        {
            *OutKnownType = true;
        }
        return Iterator->second.Factory();
    }

    if (OutKnownType != nullptr)
    {
        *OutKnownType = false;
    }

    auto* UnknownActor = new AUnknownActor();
    UnknownActor->SetOriginalTypeName(TypeName);
    return UnknownActor;
}

Engine::Component::USceneComponent* FSceneTypeRegistry::ConstructComponent(const FString& TypeName,
                                                                           bool* OutKnownType)
{
    EnsureSceneTypesRegistered();

    const FRegistryStorage& Storage = GetRegistryStorage();
    const auto              Iterator = Storage.ComponentTypesByName.find(TypeName);
    if (Iterator != Storage.ComponentTypesByName.end())
    {
        if (OutKnownType != nullptr)
        {
            *OutKnownType = true;
        }
        return Iterator->second.Factory();
    }

    if (OutKnownType != nullptr)
    {
        *OutKnownType = false;
    }

    auto* UnknownComponent = new Engine::Component::UUnknownComponent();
    UnknownComponent->SetOriginalTypeName(TypeName);
    return UnknownComponent;
}
