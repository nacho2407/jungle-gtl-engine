// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "GameFramework/World.h"
#include <cstring>

IMPLEMENT_HIDDEN_COMPONENT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::Serialize(FArchive& Ar)
{
    ULightComponentBase::Serialize(Ar);
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponentBase::GetEditableProperties(OutProps);
}

void ULightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponentBase::PostEditProperty(PropertyName);

    if (!LightProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    // Transform edits are already propagated through USceneComponent::PostEditProperty()
    // -> MarkTransformDirty() -> OnTransformDirty(), so we avoid duplicating that work here.
    if (strcmp(PropertyName, "Location") == 0 || strcmp(PropertyName, "Rotation") == 0 || strcmp(PropertyName, "Scale") == 0)
    {
        return;
    }

    FScene& Scene = Owner->GetWorld()->GetScene();
    Scene.MarkLightProxyDirty(LightProxy, ESceneProxyDirtyFlag::Lighting | ESceneProxyDirtyFlag::Shadow);
}

void ULightComponent::OnTransformDirty()
{
    MarkRenderTransformDirty();
}

void ULightComponent::CreateRenderState()
{
    if (LightProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    LightProxy = Scene.AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
    if (!LightProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    Scene.RemoveLight(LightProxy);
    LightProxy = nullptr;
}

void ULightComponent::MarkRenderStateDirty()
{
    if (!LightProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    Scene.RemoveLight(LightProxy);
    LightProxy = nullptr;
    LightProxy = Scene.AddLight(this);
}

void ULightComponent::MarkRenderTransformDirty()
{
    if (!LightProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    Owner->GetWorld()->GetScene().MarkLightProxyDirty(LightProxy, ESceneProxyDirtyFlag::Transform);
}

FLightProxy* ULightComponent::CreateLightProxy()
{
    return nullptr;
}


