#include "PrimitiveComponent.h"
#include "Object/Class.h"
#include "Renderer/SceneProxy.h"
#include "Serializer/Archive.h"
#include "Debug/EngineLog.h"
#include "Actor/Actor.h"
#include "Scene/Scene.h"
IMPLEMENT_RTTI(UPrimitiveComponent, USceneComponent)

TArray<UPrimitiveComponent*> UPrimitiveComponent::PendingRenderStateUpdates;

FBoxSphereBounds UPrimitiveComponent::GetLocalBounds() const
{
	return { FVector(0, 0, 0), 0.f, FVector(0, 0, 0) };
}

void UPrimitiveComponent::OnRegister()
{
	USceneComponent::OnRegister();
	MarkRenderStateDirty();
}

void UPrimitiveComponent::OnUnregister()
{
	auto PendingIt = std::remove(PendingRenderStateUpdates.begin(), PendingRenderStateUpdates.end(), this);
	PendingRenderStateUpdates.erase(PendingIt, PendingRenderStateUpdates.end());
	SceneProxy.reset();
	bRenderStateDirty = true;
	bRenderStateUpdateQueued = false;
	USceneComponent::OnUnregister();
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	bRenderStateDirty = true;
	EnqueueRenderStateUpdate(this);
}

void UPrimitiveComponent::FlushPendingRenderStateUpdates()
{
	if (PendingRenderStateUpdates.empty())
	{
		return;
	}

	TArray<UPrimitiveComponent*> PendingUpdates = std::move(PendingRenderStateUpdates);
	PendingRenderStateUpdates.clear();

	for (UPrimitiveComponent* PrimitiveComponent : PendingUpdates)
	{
		if (!PrimitiveComponent || !PrimitiveComponent->bRenderStateUpdateQueued)
		{
			continue;
		}

		PrimitiveComponent->bRenderStateUpdateQueued = false;
		if (PrimitiveComponent->IsPendingKill() || !PrimitiveComponent->IsRegistered())
		{
			continue;
		}

		if (PrimitiveComponent->bRenderStateDirty)
		{
			PrimitiveComponent->RecreateSceneProxy();
		}
	}
}

std::shared_ptr<FPrimitiveSceneProxy> UPrimitiveComponent::CreateSceneProxy() const
{
	return nullptr;
}

FPrimitiveSceneProxy* UPrimitiveComponent::GetSceneProxy() const
{
	// Render state updates are expected to be processed in FlushPendingRenderStateUpdates
	// on the render command collection boundary.

	return SceneProxy.get();
}

void UPrimitiveComponent::EnqueueRenderStateUpdate(UPrimitiveComponent* InPrimitiveComponent)
{
	if (!InPrimitiveComponent || InPrimitiveComponent->bRenderStateUpdateQueued)
	{
		return;
	}

	InPrimitiveComponent->bRenderStateUpdateQueued = true;
	PendingRenderStateUpdates.push_back(InPrimitiveComponent);
}

void UPrimitiveComponent::RecreateSceneProxy()
{
	SceneProxy = CreateSceneProxy();
	bRenderStateDirty = false;
	bRenderStateUpdateQueued = false;
}

void UPrimitiveComponent::UpdateBounds()
{
	bBoundsDirty = true;
	MarkRenderStateDirty();

	AActor* OwnerActor = GetOwner();
	if (OwnerActor)
	{
		if (UScene* Scene = OwnerActor->GetScene())
		{
			Scene->MarkSpatialDirty();
		}
	}
}

FBoxSphereBounds UPrimitiveComponent::GetWorldBounds() const
{
	if (bBoundsDirty)
	{
		Bounds = CalcBounds(GetWorldTransform());
		bBoundsDirty = false;
	}
	return Bounds;
}

FBoxSphereBounds UPrimitiveComponent::CalcBounds(const FMatrix& LocalToWorld) const
{
	FBoxSphereBounds LocalBound = GetLocalBounds();

	if (LocalBound.Radius <= 0.f && LocalBound.BoxExtent.X == 0.f)
	{
		FVector Translation(LocalToWorld.M[3][0], LocalToWorld.M[3][1], LocalToWorld.M[3][2]);
		return { Translation, 1.0f, FVector(1, 1, 1) };
	}

	FVector Center = LocalToWorld.TransformPosition(LocalBound.Center);

	FMatrix AbsM = FMatrix::Abs(LocalToWorld);

	FVector WorldBoxExtent;
	WorldBoxExtent.X = AbsM.M[0][0] * LocalBound.BoxExtent.X
		+ AbsM.M[1][0] * LocalBound.BoxExtent.Y
		+ AbsM.M[2][0] * LocalBound.BoxExtent.Z;

	WorldBoxExtent.Y = AbsM.M[0][1] * LocalBound.BoxExtent.X
		+ AbsM.M[1][1] * LocalBound.BoxExtent.Y
		+ AbsM.M[2][1] * LocalBound.BoxExtent.Z;

	WorldBoxExtent.Z = AbsM.M[0][2] * LocalBound.BoxExtent.X
		+ AbsM.M[1][2] * LocalBound.BoxExtent.Y
		+ AbsM.M[2][2] * LocalBound.BoxExtent.Z;

	return { Center, WorldBoxExtent.Size(), WorldBoxExtent };
}

/*
void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar.Serialize("bDrawDebugBounds", bDrawDebugBounds);
}
*/
