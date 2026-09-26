#pragma once
#include "PrimitiveComponent.h"
#include "Renderer/MeshData.h"

enum EHorizTextAligment : int;
enum EVerticalTextAligment : int;


class ENGINE_API UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UTextRenderComponent, UPrimitiveComponent)

	void PostConstruct() override;

	virtual bool UseSpherePicking() const override { return true; }
	
	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetText(const FString& InText);
	const FString& GetText() const { return Text; }

	virtual FString GetDisplayText() const { return Text; }

	void SetTextColor(const FVector4& InColor) { TextColor = InColor; }
	const FVector4& GetTextColor() const { return TextColor; }

	void SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }
	bool IsBillboard() const { return bBillboard; }

	void SetTextScale(float InScale) { TextScale = InScale; }
	float GetTextScale() const { return TextScale; }

	void SetWorldScale(float InScale) { TextScale = InScale; }
	float GetWorldScale() const { return TextScale; }

	void SetHorizontalAlignment(EHorizTextAligment value);
	EHorizTextAligment GetHorizontalAlignment() const { return HorizontalAlignment; }

	void SetVerticalAlignment(EVerticalTextAligment value);
	EVerticalTextAligment GetVerticalAlignment() const { return VerticalAlignment; }

	virtual FVector GetRenderWorldPosition() const { return GetWorldLocation(); }
	virtual FVector GetRenderWorldScale() const { return GetWorldTransform().GetScaleVector() * TextScale; }

	virtual FRenderMesh* GetRenderMesh() const override;
	FDynamicMesh* GetTextMesh() const { return TextMesh.get(); }

	bool IsTextMeshDirty() const { return bTextMeshDirty; }
	void MarkTextMeshDirty() { bTextMeshDirty = true; if (TextMesh) TextMesh->bIsDirty = true; }
	void ClearTextMeshDirty() { bTextMeshDirty = false; }

	void Serialize(FArchive& Ar) override;
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

	void SetHiddenInGame(bool bInHidden) { bHiddenInGame = bInHidden; }
	bool IsHiddenInGame() const { return bHiddenInGame; }

protected:
	FString Text = "Text";
	FVector4 TextColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float TextScale = 1.0f;
	bool bBillboard = false;

	bool bHiddenInGame = true;

	EHorizTextAligment HorizontalAlignment = EHorizTextAligment::EHTA_Center;
	EVerticalTextAligment VerticalAlignment = EVerticalTextAligment::EVRTA_TextCenter;

	std::shared_ptr<struct FDynamicMesh> TextMesh;

	bool bTextMeshDirty = true;
};
