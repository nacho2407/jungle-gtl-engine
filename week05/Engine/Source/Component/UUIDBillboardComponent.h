#pragma once

#include "TextComponent.h"


class ENGINE_API UUUIDBillboardComponent : public UTextComponent
{
public:
	DECLARE_RTTI(UUUIDBillboardComponent, UTextComponent)

	virtual void Initialize() override;

	virtual FString GetDisplayText() const override;
	// SetWorldOffset 반영해서 오브젝트 머리 위에 뜨도록 함
	virtual FVector GetRenderWorldPosition() const override;
	virtual FVector GetRenderWorldScale() const override;

	const FVector& GetWorldOffset() const { return WorldOffset; }
	void SetWorldOffset(const FVector& InOffset) { WorldOffset = InOffset; }

	virtual FBoxSphereBounds GetWorldBounds() const override;

private:
	FVector WorldOffset = FVector(0.0f, 0.0f, 0.3f);
};