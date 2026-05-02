#include "Runtime/Script/ScriptManager.h"

#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/BoxComponent.h"
#include "Component/CameraComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/DecalComponent.h"
#include "Component/FireballComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/PursuitMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/LightComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/SoundComponent.h"
#include "Component/SphereComponent.h"
#include "Component/SpringArmComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/Script/ScriptComponent.h"
#include "Runtime/Script/ScriptUtils.h"
#include "ThirdParty/sol/sol.hpp"

namespace
{
    bool MatchLuaTypeName(const FString& TypeName, const char* NativeName, const char* LuaName)
    {
        return TypeName == NativeName || TypeName == LuaName;
    }

    const FTypeInfo* FindComponentTypeInfo(const FString& TypeName)
    {
        if (MatchLuaTypeName(TypeName, "UActorComponent", "ActorComponent")) return &UActorComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USceneComponent", "SceneComponent")) return &USceneComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UPrimitiveComponent", "PrimitiveComponent")) return &UPrimitiveComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UShapeComponent", "ShapeComponent")) return &UShapeComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UMeshComponent", "MeshComponent")) return &UMeshComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UStaticMeshComponent", "StaticMeshComponent")) return &UStaticMeshComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UProceduralMeshComponent", "ProceduralMeshComponent")) return &UProceduralMeshComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UBillboardComponent", "BillboardComponent")) return &UBillboardComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USubUVComponent", "SubUVComponent")) return &USubUVComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UTextRenderComponent", "TextRenderComponent")) return &UTextRenderComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UCameraComponent", "CameraComponent")) return &UCameraComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USpringArmComponent", "SpringArmComponent")) return &USpringArmComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USoundComponent", "SoundComponent")) return &USoundComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UScriptComponent", "ScriptComponent")) return &UScriptComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UDecalComponent", "DecalComponent")) return &UDecalComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UFireballComponent", "FireballComponent")) return &UFireballComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UHeightFogComponent", "HeightFogComponent")) return &UHeightFogComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UMovementComponent", "MovementComponent")) return &UMovementComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UProjectileMovementComponent", "ProjectileMovementComponent")) return &UProjectileMovementComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UPursuitMovementComponent", "PursuitMovementComponent")) return &UPursuitMovementComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UInterpToMovementComponent", "InterpToMovementComponent")) return &UInterpToMovementComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "URotatingMovementComponent", "RotatingMovementComponent")) return &URotatingMovementComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "ULightComponentBase", "LightComponentBase")) return &ULightComponentBase::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "ULightComponent", "LightComponent")) return &ULightComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UAmbientLightComponent", "AmbientLightComponent")) return &UAmbientLightComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UDirectionalLightComponent", "DirectionalLightComponent")) return &UDirectionalLightComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UPointLightComponent", "PointLightComponent")) return &UPointLightComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USpotlightComponent", "SpotlightComponent")) return &USpotlightComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UBoxComponent", "BoxComponent")) return &UBoxComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "USphereComponent", "SphereComponent")) return &USphereComponent::s_TypeInfo;
        if (MatchLuaTypeName(TypeName, "UCapsuleComponent", "CapsuleComponent")) return &UCapsuleComponent::s_TypeInfo;
        return nullptr;
    }

    UActorComponent* GetComponentByType(AActor& Actor, const FString& TypeName)
    {
        const FTypeInfo* TargetType = FindComponentTypeInfo(TypeName);
        if (!TargetType)
        {
            return nullptr;
        }

        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (Component && Component->GetTypeInfo()->IsA(TargetType))
            {
                return Component;
            }
        }
        return nullptr;
    }

    UActorComponent* AddComponentByType(AActor& Actor, const FString& TypeName, sol::optional<bool> bAttachToRoot)
    {
        const FTypeInfo* TargetType = FindComponentTypeInfo(TypeName);
        if (!TargetType)
        {
            return nullptr;
        }

        UActorComponent* Component = Actor.AddComponentByClass(TargetType);
        USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
        if (!SceneComponent)
        {
            return Component;
        }

        if (!Actor.GetRootComponent())
        {
            Actor.SetRootComponent(SceneComponent);
            return Component;
        }

        if (bAttachToRoot.value_or(true))
        {
            SceneComponent->AttachToComponent(Actor.GetRootComponent());
        }
        return Component;
    }

    sol::table GetComponents(sol::this_state State, AActor& Actor)
    {
        sol::state_view Lua(State);
        sol::table Components = Lua.create_table();

        int32 Index = 1;
        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (Component)
            {
                Components[Index++] = Component;
            }
        }
        return Components;
    }

    sol::table GetComponentsByType(sol::this_state State, AActor& Actor, const FString& TypeName)
    {
        sol::state_view Lua(State);
        sol::table Components = Lua.create_table();

        const FTypeInfo* TargetType = FindComponentTypeInfo(TypeName);
        if (!TargetType)
        {
            return Components;
        }

        int32 Index = 1;
        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (Component && Component->GetTypeInfo()->IsA(TargetType))
            {
                Components[Index++] = Component;
            }
        }
        return Components;
    }

    sol::table ActorTagsToLuaTable(sol::this_state State, AActor& Actor)
    {
        sol::state_view Lua(State);
        sol::table Tags = Lua.create_table();

        int32 Index = 1;
        for (const FString& Tag : Actor.GetTags())
        {
            Tags[Index++] = Tag;
        }
        return Tags;
    }

    bool RemoveActorComponent(AActor& Actor, UActorComponent* Component)
    {
        if (!Component || Component->GetOwner() != &Actor)
        {
            return false;
        }

        Actor.RemoveComponent(Component);
        return true;
    }
}

void FScriptManager::BindActorTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, AActor, "Actor", UObject)
    LUA_PROPERTY(Name, [](AActor& Actor) -> FString
                 { return Actor.GetName(); });
    LUA_PROPERTY(TypeName, [](AActor& Actor) -> FString
                 { return Actor.GetTypeInfo() && Actor.GetTypeInfo()->name ? Actor.GetTypeInfo()->name : ""; });
    LUA_RW_PROPERTY(Location, GetActorLocation, SetActorLocation);
    LUA_RW_PROPERTY(Rotation, GetActorRotation, SetActorRotation);
    LUA_RW_PROPERTY(Scale, GetActorScale, SetActorScale);
    LUA_RO_PROPERTY(UUID, GetUUID);
    LUA_RO_PROPERTY(RootComponent, GetRootComponent);
    LUA_RW_PROPERTY(Active, IsActive, SetActive);
    LUA_RW_PROPERTY(Visible, IsVisible, SetVisible);
    LUA_RW_PROPERTY(TickInEditor, ShouldTickInEditor, SetTickInEditor);
    LUA_METHOD(Duplicate, Duplicate);
    LUA_METHOD(GetActorForwardVector, GetActorForward);
    LUA_METHOD(GetActorRightVector, GetActorRight);
    LUA_METHOD(GetActorUpVector, GetActorUp);
    LUA_METHOD(AddTag, AddTag);
    LUA_METHOD(RemoveTag, RemoveTag);
    LUA_METHOD(HasTag, HasTag);
    LUA_METHOD(ClearTags, ClearTags);
    LUA_METHOD(GetRootComponent, GetRootComponent);
    LUA_METHOD(Add_Actor_World_Offset, AddActorWorldOffset);
    LUA_METHOD(Get_Actor_Forward, GetActorForward);
    LUA_METHOD(Get_Actor_Right, GetActorRight);
    LUA_METHOD(Get_Actor_Up, GetActorUp);
    LUA_SET(GetComponents, &GetComponents);
    LUA_SET(Get_Components, &GetComponents);
    LUA_SET(GetComponent, &GetComponentByType);
    LUA_SET(GetComponentByType, &GetComponentByType);
    LUA_SET(Get_Component_By_Type, &GetComponentByType);
    LUA_SET(GetComponentsByType, &GetComponentsByType);
    LUA_SET(AddComponent, &AddComponentByType);
    LUA_SET(RemoveComponent, &RemoveActorComponent);
    LUA_SET(GetTags, &ActorTagsToLuaTable);
    LUA_SET(Get_Static_Mesh_Component, [](AActor& Actor)
            { return Cast<UStaticMeshComponent>(GetComponentByType(Actor, "StaticMeshComponent")); });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, APlayerController, "PlayerController", AActor, UObject)
    LUA_METHOD(Possess, Possess);
    LUA_METHOD(UnPossess, UnPossess);
    LUA_METHOD(SetViewTarget, SetViewTarget);
    LUA_METHOD(SetViewTargetCamera, SetViewTargetCamera);
    LUA_METHOD(GetPossessedActor, GetPossessedActor);
    LUA_METHOD(GetViewTargetActor, GetViewTargetActor);
    LUA_METHOD(GetViewTargetCamera, GetViewTargetCamera);
    LUA_RO_PROPERTY(PossessedActor, GetPossessedActor);
    LUA_RO_PROPERTY(ViewTargetActor, GetViewTargetActor);
    LUA_RO_PROPERTY(ViewTargetCamera, GetViewTargetCamera);
    LUA_END_TYPE();
}
