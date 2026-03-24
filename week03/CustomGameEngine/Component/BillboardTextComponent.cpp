#include "BillboardTextComponent.h"

#include <cmath>

#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "EngineCore/ResourceManager.h"
#include "Renderer/Renderer.h"

void UBillboardTextComponent::AppendDrawItems(TArray<FBillboardTextDrawItem>& OutItems) const
{
	if (AttachedComponent == nullptr)
	{
		return;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AttachedComponent);
	if (Primitive != nullptr && Primitive->bVisible == false)
	{
		return;
	}

	FFontAsset* FontAsset = &ResourceManager::GetInstance()->GetBillboardFont();
	const UCameraComponent* Camera = UCameraComponent::GetMainCamera();
	const FVector BillboardUp = (Camera != nullptr) ? Camera->UpDirection.Normalize() : FVector::Up();
	const FVector BaseAnchor = AttachedComponent->GetRelativeLocation() + FVector::Up() * ComputeAnchorHeightOffset();

	// 이름 라인은 UUID 라인보다 위쪽에 배치
	FBillboardTextDrawItem NameItem;
	NameItem.FontAsset = FontAsset;
	NameItem.Text = AttachedComponent->Name.GetDisplayName();
	NameItem.Anchor = BaseAnchor + BillboardUp * NameLineOffset;
	NameItem.Scale = TextScale;
	NameItem.LineGap = LineGap;
	NameItem.Color = NameColor;
	OutItems.push_back(NameItem);

	// UUID 라인은 기준 앵커 위치에 배치
	FBillboardTextDrawItem UUIDItem;
	UUIDItem.FontAsset = FontAsset;
	UUIDItem.Text = "UUID: " + std::to_string(AttachedComponent->UUID);
	UUIDItem.Anchor = BaseAnchor;
	UUIDItem.Scale = TextScale;
	UUIDItem.LineGap = LineGap;
	UUIDItem.Color = UUIDColor;
	OutItems.push_back(UUIDItem);
}

float UBillboardTextComponent::ComputeAnchorHeightOffset() const
{
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AttachedComponent);
	if (Primitive == nullptr || Primitive->GetGeometry() == nullptr || Primitive->GetGeometry()->Vertices.empty())
	{
		return 1.0f + VerticalOffset;
	}

	// 메시의 가장 높은 로컬 Z를 기준으로 텍스트 시작 높이 설정
	float MaxLocalZ = Primitive->GetGeometry()->Vertices[0].z;
	for (const FVector& Vertex : Primitive->GetGeometry()->Vertices)
	{
		if (Vertex.z > MaxLocalZ)
		{
			MaxLocalZ = Vertex.z;
		}
	}

	const float ScaleZ = std::abs(Primitive->GetRelativeScale3D().z);
	return std::abs(MaxLocalZ) * (ScaleZ > 0.0f ? ScaleZ : 0.001f) + VerticalOffset;
}

REGISTER_CLASS(UBillboardTextComponent);
