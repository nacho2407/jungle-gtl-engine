#pragma once
#include "PrimitiveComponent.h"

#include "Renderer/MeshData.h"

class FPrimitiveSceneProxy;

class ENGINE_API UTextComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UTextComponent, UPrimitiveComponent)

	void PostConstruct() override;

	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetText(const FString& InText);
	const FString& GetText() const { return Text; }

	virtual FString GetDisplayText() const { return Text; }

	void SetTextColor(const FVector4& InColor) { TextColor = InColor; MarkRenderStateDirty(); }
	const FVector4& GetTextColor() const { return TextColor; }

	void SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; UpdateBounds(); }
	bool IsBillboard() const { return bBillboard; }

	void SetTextScale(float InScale) { TextScale = InScale; UpdateBounds(); }
	float GetTextScale() const { return TextScale; }

	void SetWorldScale(float InScale) { TextScale = InScale; UpdateBounds(); }
	float GetWorldScale() const { return TextScale; }

	virtual FVector GetRenderWorldPosition() const { return GetWorldLocation(); }
	virtual FVector GetRenderWorldScale() const { return GetWorldTransform().GetScaleVector() * TextScale; }

	virtual FRenderMesh* GetRenderMesh() const override;
	EPrimitiveRenderCategory GetRenderCategory() const override { return EPrimitiveRenderCategory::Text; }
	std::shared_ptr<FPrimitiveSceneProxy> CreateSceneProxy() const override;
	FDynamicMesh* GetTextMesh() const { return TextMesh.get(); }

	bool IsTextMeshDirty() const { return bTextMeshDirty; }
	void MarkTextMeshDirty() { bTextMeshDirty = true; if (TextMesh) TextMesh->bIsDirty = true; MarkRenderStateDirty(); }
	void ClearTextMeshDirty() { bTextMeshDirty = false; }

	void Serialize(FArchive& Ar) override;

protected:
	FString Text = "Text";
	FVector4 TextColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float TextScale = 1.0f;
	bool bBillboard = false;

	std::shared_ptr<struct FDynamicMesh> TextMesh;

	bool bTextMeshDirty = true;
};
