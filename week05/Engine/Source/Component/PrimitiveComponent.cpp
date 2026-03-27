#include "PrimitiveComponent.h"
#include "Object/Class.h"
#include "Debug/EngineLog.h"

IMPLEMENT_RTTI(UPrimitiveComponent, USceneComponent)

FBoxSphereBounds UPrimitiveComponent::GetWorldBounds() const
{
	FVector Center = GetWorldLocation();

	if (!Primitive)
	{
		return { Center, 1, FVector(1, 1, 1) };
	}

	FMeshData* MeshData = Primitive->GetMeshData();

	if (MeshData)
	{
		FVector LocalBoxExtent = (MeshData->GetMaxCoord() - MeshData->GetMinCoord()) * 0.5;
		Center = GetWorldTransform().TransformPosition(MeshData->GetCenterCoord());
		FVector S = GetWorldTransform().GetScaleVector();
		FVector ScaledExtent = FVector::Multiply(LocalBoxExtent, S);
		FMatrix AbsR = FMatrix::Abs(GetWorldTransform().GetRotationMatrix());

		FVector WorldBoxExtent;
		// S * R (row-major)
		WorldBoxExtent.X = AbsR.M[0][0] * ScaledExtent.X
			+ AbsR.M[1][0] * ScaledExtent.Y 
			+ AbsR.M[2][0] * ScaledExtent.Z;

		WorldBoxExtent.Y = AbsR.M[0][1] * ScaledExtent.X
			+ AbsR.M[1][1] * ScaledExtent.Y
			+ AbsR.M[2][1] * ScaledExtent.Z;

		WorldBoxExtent.Z = AbsR.M[0][2] * ScaledExtent.X
			+ AbsR.M[1][2] * ScaledExtent.Y
			+ AbsR.M[2][2] * ScaledExtent.Z;

		return { Center, WorldBoxExtent.Size(), WorldBoxExtent };
	}

	/** MeshData 가 없을 때 어떻게 처리할 지는 생각이 필요할듯 */
	return { Center, 1, FVector(1, 1, 1) };
}

void UPrimitiveComponent::UpdateLocalBound()
{
	FMeshData* MeshData = Primitive->GetMeshData();

	if (MeshData)
	{
		MeshData->UpdateLocalBound();
	}
}
