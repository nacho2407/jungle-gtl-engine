#include "TextComponent.h"
#include "Object/Class.h"
#include <algorithm>

#include "Serializer/Archive.h"
#include "Renderer/RenderType.h"


IMPLEMENT_RTTI(UTextRenderComponent, UPrimitiveComponent)

void UTextRenderComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	TextMesh = std::make_shared<FDynamicMesh>();
	TextMesh->Topology = EMeshTopology::EMT_TriangleList;

	bTextMeshDirty = true;
	if (TextMesh) TextMesh->bIsDirty = true;
}

void UTextRenderComponent::SetText(const FString& InText)
{
	if (Text != InText)
	{
		Text = InText;
		MarkTextMeshDirty();
	}
}


void UTextRenderComponent::SetHorizontalAlignment(EHorizTextAligment value)
{
	HorizontalAlignment = value;
	MarkTextMeshDirty();
}

void UTextRenderComponent::SetVerticalAlignment(EVerticalTextAligment value)
{
	VerticalAlignment = value;
	MarkTextMeshDirty();
}

void UTextRenderComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UPrimitiveComponent::DuplicateShallow(DuplicatedObject, Context);

	UTextRenderComponent* DuplicatedTextComponent = static_cast<UTextRenderComponent*>(DuplicatedObject);
	DuplicatedTextComponent->Text = Text;
	DuplicatedTextComponent->TextColor = TextColor;
	DuplicatedTextComponent->TextScale = TextScale;
	DuplicatedTextComponent->bBillboard = bBillboard;
	DuplicatedTextComponent->bHiddenInGame = bHiddenInGame;
	DuplicatedTextComponent->HorizontalAlignment = HorizontalAlignment;
	DuplicatedTextComponent->VerticalAlignment = VerticalAlignment;
	DuplicatedTextComponent->bTextMeshDirty = true;
	if (DuplicatedTextComponent->TextMesh)
	{
		DuplicatedTextComponent->TextMesh->bIsDirty = true;
	}
}

FRenderMesh* UTextRenderComponent::GetRenderMesh() const
{
	return TextMesh.get();
}


void UTextRenderComponent::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UPrimitiveComponent::PostDuplicate(DuplicatedObject, Context);

	UTextRenderComponent* DuplicatedTextComponent = static_cast<UTextRenderComponent*>(DuplicatedObject);
	DuplicatedTextComponent->MarkTextMeshDirty();
}

void UTextRenderComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	uint32 SavedHorizontalAlignment = static_cast<uint32>(HorizontalAlignment);
	uint32 SavedVerticalAlignment = static_cast<uint32>(VerticalAlignment);

	if (Ar.IsSaving())
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);
		Ar.Serialize("TextScale", TextScale);
		Ar.Serialize("Billboard", bBillboard);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("HorizontalAlignment", SavedHorizontalAlignment);
		Ar.Serialize("VerticalAlignment", SavedVerticalAlignment);
	}
	else
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);
		Ar.Serialize("TextScale", TextScale);
		Ar.Serialize("Billboard", bBillboard);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("HorizontalAlignment", SavedHorizontalAlignment);
		Ar.Serialize("VerticalAlignment", SavedVerticalAlignment);

		SetText(Text);
		SetTextColor(TextColor);
		SetTextScale(TextScale);
		SetBillboard(bBillboard);
		SetHiddenInGame(bHiddenInGame);
		SetHorizontalAlignment(static_cast<EHorizTextAligment>(static_cast<int32>(SavedHorizontalAlignment)));
		SetVerticalAlignment(static_cast<EVerticalTextAligment>(static_cast<int32>(SavedVerticalAlignment)));
	}
}

FBoxSphereBounds UTextRenderComponent::GetWorldBounds() const
{
	const FVector Center = GetRenderWorldPosition();
	const FString DisplayText = GetDisplayText();
	const size_t TextLength = std::max<size_t>(DisplayText.size(), 1);

	const FVector RenderScale = GetRenderWorldScale();
	const float BaseScale = std::max(
		std::max(RenderScale.X, RenderScale.Y),
		std::max(RenderScale.Z, 0.3f)
	);

	const float HalfWidth = static_cast<float>(TextLength) * BaseScale * 0.35f;
	const float HalfHeight = BaseScale * 0.5f;
	const float HalfDepth = BaseScale * 0.15f;

	const FVector BoxExtent(HalfDepth, HalfWidth, HalfHeight);
	return { Center, BoxExtent.Size(), BoxExtent };
}
