#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Render/Culling/ConvexVolume.h"  // Convex Volume을 다른 곳으로 옮겨도 될 것 같은데
#include "Render/Types/VertexTypes.h"

// class DecalProxy;

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent() = default;
	~UDecalComponent() override = default;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	// Property Editor 지원
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// Color (with Color)
	void SetColor(FVector4 InColor) { Color = InColor; }
	FVector4 GetColor() const { return Color; }
	
	// --- Texture ---
	void SetTexture(const FName& InTextureName);
	const FTextureResource* GetTexture() const { return CachedTexture; }
	const FName& GetTextureName() const { return TextureName; }

	const FConvexVolume GetOBB() { return OBB; }
	void SetOBB(FConvexVolume InOBB) { OBB = InOBB; }
	void UpdateOBBFromTransform();
	void OnTransformDirty() override;

	const TMeshData<FVertexPNCT>* GetDecalMeshData() const { return &DecalMeshData; }

private:
	void DrawDebugBox();
	void UpdateDecalMesh();

private:
	FConvexVolume OBB;
	FName TextureName;
	FVector4 Color = {1,1,1,1};
	TMeshData<FVertexPNCT> DecalMeshData;
	FTextureResource* CachedTexture = nullptr;	// ResourceManager 소유, 참조만
};
