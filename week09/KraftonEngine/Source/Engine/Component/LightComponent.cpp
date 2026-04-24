// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

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
    MarkRenderStateDirty(); // 속성 변경은 프록시 전체 재생성이 필요합니다.
}

void ULightComponent::OnTransformDirty()
{
    MarkRenderTransformDirty();
}

void ULightComponent::CreateRenderState()
{
    if (LightSceneProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    LightSceneProxy = Scene.AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
    if (!LightSceneProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    Scene.RemoveLight(LightSceneProxy);
    LightSceneProxy = nullptr;
}

void ULightComponent::MarkRenderStateDirty()
{
    if (!LightSceneProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    FScene& Scene = Owner->GetWorld()->GetScene();
    Scene.RemoveLight(LightSceneProxy);
    LightSceneProxy = nullptr;
    LightSceneProxy = Scene.AddLight(this);
}

void ULightComponent::MarkRenderTransformDirty()
{
    if (!LightSceneProxy)
        return;
    if (!Owner || !Owner->GetWorld())
        return;

    Owner->GetWorld()->GetScene().MarkLightProxyDirty(LightSceneProxy, ESceneProxyDirtyFlag::Transform);
}

FLightSceneProxy* ULightComponent::CreateLightSceneProxy()
{
    return nullptr;
}
