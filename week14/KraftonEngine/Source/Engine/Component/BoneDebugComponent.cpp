#include "BoneDebugComponent.h"

#include "Object/ObjectFactory.h"
#include "SkeletalMeshComponent.h"
#include "Render/Proxy/BoneDebugSceneProxy.h"

IMPLEMENT_CLASS(UBoneDebugComponent, UPrimitiveComponent)

UBoneDebugComponent::UBoneDebugComponent()
{
}

UBoneDebugComponent::~UBoneDebugComponent()
{
}

FPrimitiveSceneProxy* UBoneDebugComponent::CreateSceneProxy()
{
	return new FBoneDebugSceneProxy(this);
}
