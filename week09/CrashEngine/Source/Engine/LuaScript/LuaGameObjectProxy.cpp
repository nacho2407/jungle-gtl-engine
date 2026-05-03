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
    if (TypeName == "StaticMeshComponent")
        return "UStaticMeshComponent";
    if (TypeName == "TextRenderComponent")
        return "UTextRenderComponent";
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

    return TypeName;
}

bool IsLuaAddableComponentClassName(const FString& ClassName)
{
    return ClassName == "UStaticMeshComponent"
		|| ClassName == "UTextRenderComponent" 
		|| ClassName == "UPointLightComponent" 
		|| ClassName == "USpotLightComponent" 
		|| ClassName == "UDirectionalLightComponent" 
		|| ClassName == "UAmbientLightComponent" 
		|| ClassName == "USphereComponent" 
		|| ClassName == "UBoxComponent" 
		|| ClassName == "UCapsuleComponent";
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

    Actor->TakeDamage(Damage, Instigator.GetActor());
}

bool FLuaGameObjectProxy::HasTag(const FString& Tag) const
{
    return Actor ? Actor->HasTag(Tag) : false;
}

void FLuaGameObjectProxy::AddTag(const FString& Tag)
{
    if (Actor)
    {
        Actor->AddTag(Tag);
    }
}

void FLuaGameObjectProxy::RemoveTag(const FString& Tag)
{
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
