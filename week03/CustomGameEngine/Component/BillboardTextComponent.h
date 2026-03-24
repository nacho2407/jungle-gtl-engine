#pragma once

#include "SceneComponent.h"
#include "EngineCore/Color.h"

struct FBillboardTextDrawItem;

class UBillboardTextComponent : public USceneComponent
{
	DECLARE_RTTI(UBillboardTextComponent, USceneComponent)
public:
	UBillboardTextComponent();
	virtual ~UBillboardTextComponent() override;

	void AttachTo(USceneComponent* InTarget) { AttachedComponent = InTarget; }
	USceneComponent* GetAttachedComponent() const { return AttachedComponent; }

	void AppendDrawItems(TArray<FBillboardTextDrawItem>& OutItems) const;

	float VerticalOffset = 0.35f;
	float NameLineOffset = 0.75f;
	float TextScale = 0.02f;
	float LineGap = 0.0f;
	FColor NameColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);
	FColor UUIDColor = FColor(0.65f, 0.85f, 1.0f, 1.0f);

private:
	float ComputeAnchorHeightOffset() const;

private:
	USceneComponent* AttachedComponent = nullptr;
};
