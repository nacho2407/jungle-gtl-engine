#include "LuaComponentProxy.h"

#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/LightComponent.h"
#include "Component/LightComponentBase.h"
#include "Component/Shape/SphereComponent.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"
#include "Object/Object.h"
#include "Runtime/Engine.h"

FLuaComponentProxy::FLuaComponentProxy(UActorComponent* InComponent)
{
    SetComponent(InComponent);
}

void FLuaComponentProxy::SetComponent(UActorComponent* InComponent)
{
    ComponentUUID = InComponent ? InComponent->GetUUID() : 0;
}

UActorComponent* FLuaComponentProxy::ResolveComponent() const
{
    if (ComponentUUID == 0)
    {
        return nullptr;
    }

    return Cast<UActorComponent>(UObjectManager::Get().FindByUUID(ComponentUUID));
}

bool FLuaComponentProxy::IsValid() const
{
    return ResolveComponent() != nullptr;
}

FString FLuaComponentProxy::GetComponentClassName() const
{
    UActorComponent* Component = ResolveComponent();
    return Component && Component->GetClass() ? Component->GetClass()->GetName() : "";
}

USceneComponent* FLuaComponentProxy::ResolveSceneComponent() const
{
    return Cast<USceneComponent>(ResolveComponent());
}

UPrimitiveComponent* FLuaComponentProxy::ResolvePrimitiveComponent() const
{
    return Cast<UPrimitiveComponent>(ResolveComponent());
}

UStaticMeshComponent* FLuaComponentProxy::ResolveStaticMeshComponent() const
{
    return Cast<UStaticMeshComponent>(ResolveComponent());
}

// 작성중