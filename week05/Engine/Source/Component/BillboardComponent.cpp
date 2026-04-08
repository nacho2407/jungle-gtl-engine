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
	if (DuplicatedBillboardComponent->BillboardMesh)
	{
		DuplicatedBillboardComponent->BillboardMesh->bIsDirty = true;
	}
}

void UBillboardComponent::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UPrimitiveComponent::PostDuplicate(DuplicatedObject, Context);

	UBillboardComponent* DuplicatedBillboardComponent = static_cast<UBillboardComponent*>(DuplicatedObject);
	if (DuplicatedBillboardComponent->BillboardMesh)
	{
		DuplicatedBillboardComponent->BillboardMesh->bIsDirty = true;
	}
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
	FVector SavedSize(Size.X, Size.Y, 0.0f);
	FVector SavedUVMin(UVMin.X, UVMin.Y, 0.0f);
	FVector SavedUVMax(UVMax.X, UVMax.Y, 0.0f);

	if (Ar.IsSaving())
	{
		Ar.Serialize("TexturePath", TexturePathString);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("Size", SavedSize);
		Ar.Serialize("UVMin", SavedUVMin);
		Ar.Serialize("UVMax", SavedUVMax);
	}
	else
	{
		Ar.Serialize("TexturePath", TexturePathString);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("Size", SavedSize);
		Ar.Serialize("UVMin", SavedUVMin);
		Ar.Serialize("UVMax", SavedUVMax);

		TexturePath = TexturePathString.empty()
			? std::wstring()
			: FPaths::ToWide(FPaths::ToAbsolutePath(TexturePathString));
		Size = FVector2(SavedSize.X, SavedSize.Y);
		UVMin = FVector2(SavedUVMin.X, SavedUVMin.Y);
		UVMax = FVector2(SavedUVMax.X, SavedUVMax.Y);
		if (BillboardMesh)
		{
			BillboardMesh->bIsDirty = true;
		}
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
