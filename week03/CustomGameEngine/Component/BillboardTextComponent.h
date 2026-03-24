#pragma once

#include "SceneComponent.h"
#include "EngineCore/Color.h"

struct FBillboardTextDrawItem;

class UBillboardTextComponent : public USceneComponent
{
	DECLARE_RTTI(UBillboardTextComponent, USceneComponent)
public:
	UBillboardTextComponent() {}

	virtual ~UBillboardTextComponent() override {}

	void AttachTo(USceneComponent* InTarget) { AttachedComponent = InTarget; }
	USceneComponent* GetAttachedComponent() const { return AttachedComponent; }

	/**
	 * @brief 부착된 오브젝트의 이름 / UUID 텍스트 draw item을 생성합니다.
	 *
	 * @param OutItems 생성된 draw item이 추가될 배열
	 */
	void AppendDrawItems(TArray<FBillboardTextDrawItem>& OutItems) const;

	float VerticalOffset = 0.35f;
	float NameLineOffset = 0.75f;
	float TextScale = 0.02f;
	float LineGap = 0.0f;
	FColor NameColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);
	FColor UUIDColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);

private:
	/**
	 * @brief 빌보드 텍스트를 오브젝트 위로 띄우기 위한 높이 오프셋을 계산합니다.
	 *
	 * @return 오브젝트 기준 Z 오프셋 값
	 */
	float ComputeAnchorHeightOffset() const;

private:
	USceneComponent* AttachedComponent = nullptr;
};
