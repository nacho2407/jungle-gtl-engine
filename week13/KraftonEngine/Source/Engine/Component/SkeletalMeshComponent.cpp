#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Mesh/SkeletalMesh.h"

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}
