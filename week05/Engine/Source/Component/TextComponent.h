#pragma once
#include "PrimitiveComponent.h"

class ENGINE_API UTextComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UTextComponent, UPrimitiveComponent)

	virtual void Initialize();

	virtual FBoxSphereBounds GetWorldBounds() const override;

	/** 표시할 텍스트 설정 - 메시 데이터가 갱신될 수 있도록 유도함 */
	void SetText(const FString& InText);
	const FString& GetText() const { return Text; }

	virtual FString GetDisplayText() const { return Text; }

	void SetTextColor(const FVector4& InColor) { TextColor = InColor; }
	const FVector4& GetTextColor() const { return TextColor; }

	void SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }
	bool IsBillboard() const { return bBillboard; }

	void SetTextScale(float InScale) { TextScale = InScale; }
	float GetTextScale() const { return TextScale; }

	/** 기존 UUIDBillboardComponent와의 호환성 및 편의를 위해 추가 */
	void SetWorldScale(float InScale) { TextScale = InScale; }
	float GetWorldScale() const { return TextScale; }

	virtual FVector GetRenderWorldPosition() const { return GetWorldLocation(); }
	virtual FVector GetRenderWorldScale() const { return GetWorldTransform().GetScaleVector() * TextScale; }

	/** 폰트 렌더링용 메시 데이터 반환 */
	struct FMeshData* GetTextMesh() const { return TextMesh.get(); }

protected:
	FString Text = "Text";
	FVector4 TextColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float TextScale = 1.0f;
	bool bBillboard = false;

	/** 텍스트 렌더링을 위해 생성된 동적 메시 데이터 */
	std::shared_ptr<struct FMeshData> TextMesh;
};