#include "BillboardComponent.h"
#include "Core/Paths.h"
#include "Object/Class.h"
#include "Renderer/MeshData.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(UBillboardComponent, UPrimitiveComponent)

void UBillboardComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	BillboardMesh = std::make_shared<FDynamicMesh>();
	MarkBillboardMeshDirty();
}

void UBillboardComponent::MarkBillboardMeshDirty()
{
	bBillboardMeshDirty = true;
	if (BillboardMesh)
	{
		BillboardMesh->bIsDirty = true;
	}
}

void UBillboardComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UPrimitiveComponent::DuplicateShallow(DuplicatedObject, Context);

	UBillboardComponent* DuplicatedBillboardComponent = static_cast<UBillboardComponent*>(DuplicatedObject);
	DuplicatedBillboardComponent->bHiddenInGame = bHiddenInGame;
	DuplicatedBillboardComponent->TexturePath = TexturePath;
	DuplicatedBillboardComponent->Size = Size;
	DuplicatedBillboardComponent->UVMin = UVMin;
	DuplicatedBillboardComponent->UVMax = UVMax;
	DuplicatedBillboardComponent->MarkBillboardMeshDirty();
}

void UBillboardComponent::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UPrimitiveComponent::PostDuplicate(DuplicatedObject, Context);

	UBillboardComponent* DuplicatedBillboardComponent = static_cast<UBillboardComponent*>(DuplicatedObject);
	DuplicatedBillboardComponent->MarkBillboardMeshDirty();
	DuplicatedBillboardComponent->UpdateBounds();
}

void UBillboardComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	FString TexturePathString;
	if (!TexturePath.empty())
	{
		TexturePathString = FPaths::ToRelativePath(FPaths::FromWide(TexturePath));
	}

	if (Ar.IsSaving())
	{
		Ar.Serialize("TexturePath", TexturePathString);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("Size", Size);
		Ar.Serialize("UVMin", UVMin);
		Ar.Serialize("UVMax", UVMax);
	}
	else
	{
		Ar.Serialize("TexturePath", TexturePathString);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("Size", Size);
		Ar.Serialize("UVMin", UVMin);
		Ar.Serialize("UVMax", UVMax);

		TexturePath = TexturePathString.empty()
			? std::wstring()
			: FPaths::ToWide(FPaths::ToAbsolutePath(TexturePathString));

		MarkBillboardMeshDirty();
		UpdateBounds();
	}
}

FBoxSphereBounds UBillboardComponent::GetWorldBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfW = Size.X * 0.5f * WorldScale.X;
	const float HalfH = Size.Y * 0.5f * WorldScale.Y;
	const float HalfZ = ((HalfW > HalfH) ? HalfW : HalfH);

	const FVector BoxExtent(HalfW, HalfH, HalfZ);
	return { Center, BoxExtent.Size(), BoxExtent };
}

FRenderMesh* UBillboardComponent::GetRenderMesh() const
{
	return BillboardMesh.get();
}
