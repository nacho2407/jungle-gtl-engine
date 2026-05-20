#include "BoneDebugComponent.h"

#include "Object/Reflection/ObjectFactory.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Render/Proxy/BoneDebugSceneProxy.h"

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
