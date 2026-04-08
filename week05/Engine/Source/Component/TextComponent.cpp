#include "TextComponent.h"
#include "Object/Class.h"
#include <algorithm>

#include "Serializer/Archive.h"
#include "Renderer/RenderType.h"


IMPLEMENT_RTTI(UTextRenderComponent, UPrimitiveComponent)

void UTextRenderComponent::PostConstruct()
{
	// 폰트 렌더링용 메시 데이터 객체 생성
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
		// NOTE: 실제 정점 데이터 갱신은 RenderCollector에서 TextRenderer를 통해 수행함
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

FRenderMesh* UTextRenderComponent::GetRenderMesh() const
{
	return TextMesh.get();
}

void UTextRenderComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);
		Ar.Serialize("Billboard", bBillboard);
	}
	else
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);
		Ar.Serialize("Billboard", bBillboard);

		SetText(Text);
		SetTextColor(TextColor);
		SetBillboard(bBillboard);
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
