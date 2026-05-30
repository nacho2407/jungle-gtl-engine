#include "PhysicsAssetDebugComponent.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Render/Proxy/PhysicsAssetSceneProxy.h"

HIDE_FROM_COMPONENT_LIST(UPhysicsAssetDebugComponent)

UPhysicsAssetDebugComponent::UPhysicsAssetDebugComponent()
{
}

UPhysicsAssetDebugComponent::~UPhysicsAssetDebugComponent()
{
}

FPrimitiveSceneProxy* UPhysicsAssetDebugComponent::CreateSceneProxy()
{
	return new FPhysicsAssetSceneProxy(this);
}

void UPhysicsAssetDebugComponent::SetTarget(
	USkeletalMeshComponent* InTargetSkeletalMeshComponent,
	UPhysicsAsset* InPhysicsAsset)
{
	if (TargetSkeletalMeshComponent.Get() == InTargetSkeletalMeshComponent &&
		PhysicsAsset.Get() == InPhysicsAsset)
	{
		return;
	}

	TargetSkeletalMeshComponent = InTargetSkeletalMeshComponent;
	PhysicsAsset = InPhysicsAsset;
	MarkRenderStateDirty();
}

void UPhysicsAssetDebugComponent::SetSelectedBodyIndex(int32 InSelectedBodyIndex)
{
	if (SelectedBodyIndex == InSelectedBodyIndex)
	{
		return;
	}

	SelectedBodyIndex = InSelectedBodyIndex;
	MarkRenderStateDirty();
}

void UPhysicsAssetDebugComponent::SetSelectedConstraintIndex(int32 InSelectedConstraintIndex)
{
	if (SelectedConstraintIndex == InSelectedConstraintIndex)
	{
		return;
	}

	SelectedConstraintIndex = InSelectedConstraintIndex;
	MarkRenderStateDirty();
}

void UPhysicsAssetDebugComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UPrimitiveComponent::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(TargetSkeletalMeshComponent.Get(), "UPhysicsAssetDebugComponent.TargetSkeletalMeshComponent");
	Collector.AddReferencedObject(PhysicsAsset.Get(), "UPhysicsAssetDebugComponent.PhysicsAsset");
}
