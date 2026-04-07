#include "PrimitiveComponent.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"
#include "Debug/EngineLog.h"
#include "Actor/Actor.h"
#include "Level/Level.h"

#include "PrimitiveComponent.h"
IMPLEMENT_RTTI(UPrimitiveComponent, USceneComponent)

void UPrimitiveComponent::MarkTransformDirty()
{
	USceneComponent::MarkTransformDirty();

	if (AActor* Owner = GetOwner())
	{
		if (ULevel* Scene = Owner->GetScene())
		{
			Scene->MarkSpatialDirty();
		}
	}
}

FBoxSphereBounds UPrimitiveComponent::GetLocalBounds() const
{
	return { FVector(0, 0, 0), 0.f, FVector(0, 0, 0) };
}

void UPrimitiveComponent::UpdateBounds()
{
	Bounds = CalcBounds(GetWorldTransform());
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
