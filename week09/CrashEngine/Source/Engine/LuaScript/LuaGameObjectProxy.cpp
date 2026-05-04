#include "LuaGameObjectProxy.h"

#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Object/UClass.h"

namespace
{
FString NormalizeLuaComponentClassName(const FString& TypeName)
{
    if (TypeName == "SceneComponent")
        return "USceneComponent";
    if (TypeName == "StaticMeshComponent")
        return "UStaticMeshComponent";
    if (TypeName == "TextRenderComponent")
        return "UTextRenderComponent";
    if (TypeName == "BillboardComponent")
        return "UBillboardComponent";
    if (TypeName == "PointLightComponent")
        return "UPointLightComponent";
    if (TypeName == "SpotLightComponent")
        return "USpotLightComponent";
    if (TypeName == "DirectionalLightComponent")
        return "UDirectionalLightComponent";
    if (TypeName == "AmbientLightComponent")
        return "UAmbientLightComponent";
    if (TypeName == "SphereComponent")
        return "USphereComponent";
    if (TypeName == "BoxComponent")
        return "UBoxComponent";
    if (TypeName == "CapsuleComponent")
        return "UCapsuleComponent";
    if (TypeName == "LuaScriptComponent")
        return "ULuaScriptComponent";
    if (TypeName == "SubUVComponent")
        return "USubUVComponent";
    if (TypeName == "PatrolAgentComponent")
        return "UPatrolAgentComponent";
    if (TypeName == "PatrolPointComponent")
        return "UPatrolPointComponent";

    return TypeName;
}

bool IsLuaAddableComponentClassName(const FString& ClassName)
{
    return ClassName == "USceneComponent"
        || ClassName == "UStaticMeshComponent"
		|| ClassName == "UTextRenderComponent"
        || ClassName == "UBillboardComponent"
		|| ClassName == "UPointLightComponent"
		|| ClassName == "USpotLightComponent"
		|| ClassName == "UDirectionalLightComponent"
		|| ClassName == "UAmbientLightComponent"
		|| ClassName == "USphereComponent"
		|| ClassName == "UBoxComponent"
		|| ClassName == "UCapsuleComponent"
		|| ClassName == "ULuaScriptComponent"
        || ClassName == "USubUVComponent"
		|| ClassName == "UPatrolAgentComponent"
		|| ClassName == "UPatrolPointComponent";
}

UClass* FindClassByName(const FString& ClassName)
{
    for (UClass* Class : UClass::GetAllClasses())
    {
        if (Class && ClassName == Class->GetName())
        {
            return Class;
        }
    }

    return nullptr;
}
} // namespace

FLuaGameObjectProxy::FLuaGameObjectProxy(AActor* InActor)
{
    SetActor(InActor);
}

void FLuaGameObjectProxy::SetActor(AActor* InActor)
{
    ActorUUID = InActor ? InActor->GetUUID() : 0;

    UWorld* World = InActor ? InActor->GetWorld() : nullptr;
    WorldUUID = World ? World->GetUUID() : 0;
}

AActor* FLuaGameObjectProxy::ResolveActor() const
{
    if (ActorUUID == 0)
    {
        return nullptr;
    }

    AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
    if (!Actor)
    {
        return nullptr;
    }

    if (WorldUUID != 0)
    {
        UWorld* World = Actor->GetWorld();
        if (!World || World->GetUUID() != WorldUUID)
        {
            return nullptr;
        }
    }

    return Actor;
}

FVector FLuaGameObjectProxy::GetLocation() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaGameObjectProxy::SetLocation(const FVector& InLocation)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->SetActorLocation(InLocation);
}

FVector FLuaGameObjectProxy::GetRotation() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorRotation().ToVector() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaGameObjectProxy::SetRotation(const FVector& InRotation)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->SetActorRotation(InRotation);
}

FVector FLuaGameObjectProxy::GetScale() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f);
}

void FLuaGameObjectProxy::SetScale(const FVector& InScale)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->SetActorScale(InScale);
}

FVector FLuaGameObjectProxy::GetForwardVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorForward() : FVector(1.0f, 0.0f, 0.0f);
}

FVector FLuaGameObjectProxy::GetRightVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorRightVector() : FVector(0.0f, 1.0f, 0.0f);
}

FVector FLuaGameObjectProxy::GetUpVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorUpVector() : FVector(0.0f, 0.0f, 1.0f);
}

void FLuaGameObjectProxy::AddWorldOffset(const FVector& Delta)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->AddActorWorldOffset(Delta);
}

void FLuaGameObjectProxy::ApplyDamage(float Damage, const FLuaGameObjectProxy& Instigator)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    UWorld* World = Actor->GetWorld();
    if (World)
    {
        World->QueueDamage(Actor, Damage, Instigator.GetActor());
        return;
    }

    Actor->TakeDamage(Damage, Instigator.GetActor());
}

bool FLuaGameObjectProxy::HasTag(const FString& Tag) const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->HasTag(Tag) : false;
}

void FLuaGameObjectProxy::AddTag(const FString& Tag)
{
    AActor* Actor = ResolveActor();
    if (Actor)
    {
        Actor->AddTag(Tag);
    }
}

void FLuaGameObjectProxy::RemoveTag(const FString& Tag)
{
    AActor* Actor = ResolveActor();
    if (Actor)
    {
        Actor->RemoveTag(Tag);
    }
}

void FLuaGameObjectProxy::PrintLocation() const
{
    const FVector Location = GetLocation();
    UE_LOG(Lua, Info,
           "Actor %u Location = (%.3f, %.3f, %.3f)",
           GetUUID(),
           Location.X,
           Location.Y,
           Location.Z);
}

FLuaComponentProxy FLuaGameObjectProxy::AddComponent(const FString& ComponentType)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return FLuaComponentProxy();
    }

    const FString ClassName = NormalizeLuaComponentClassName(ComponentType);
    if (!IsLuaAddableComponentClassName(ClassName))
    {
        UE_LOG(Lua, Warning,
               "Lua tried to add disallowed component class: %s",
               ComponentType.c_str());
        return FLuaComponentProxy();
    }

    UClass* TargetClass = FindClassByName(ClassName);
    if (!TargetClass)
    {
        UE_LOG(Lua, Warning,
               "Lua component class was not found: %s",
               ClassName.c_str());
        return FLuaComponentProxy();
    }

    UActorComponent* NewComponent = Actor->AddComponentByClass(TargetClass);
    if (!NewComponent)
    {
        UE_LOG(Lua, Warning,
               "Lua failed to add component class: %s",
               ClassName.c_str());
        return FLuaComponentProxy();
    }

    if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent))
    {
        if (!Actor->GetRootComponent())
        {
            Actor->SetRootComponent(SceneComp);
        }
        else if (SceneComp != Actor->GetRootComponent())
        {
            SceneComp->AttachToComponent(Actor->GetRootComponent());
        }

        SceneComp->MarkTransformDirty();
    }

    if (Actor->HasActorBegunPlay())
    {
        NewComponent->BeginPlay();
    }

    return FLuaComponentProxy(NewComponent);
}

FLuaComponentProxy FLuaGameObjectProxy::GetComponent(const FString& ComponentType, int32 Index) const
{
    AActor* Actor = ResolveActor();
    if (!Actor || Index < 0)
    {
        return FLuaComponentProxy();
    }

    const FString ClassName = NormalizeLuaComponentClassName(ComponentType);
    int32 FoundIndex = 0;

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (!Component || !Component->GetClass())
        {
            continue;
        }

        if (ClassName == Component->GetClass()->GetName())
        {
            if (FoundIndex == Index)
            {
                return FLuaComponentProxy(Component);
            }

            ++FoundIndex;
        }
    }

    return FLuaComponentProxy();
}
