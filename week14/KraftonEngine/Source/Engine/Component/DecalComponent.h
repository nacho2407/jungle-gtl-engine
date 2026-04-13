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
	
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	// Color (with Color)
	void SetColor(FVector4 InColor) { Color = InColor; }
	FVector4 GetColor() const;

	// --- Texture ---
	void SetTexture(const FName& InTextureName);
	const FTextureResource* GetTexture() const { return CachedTexture; }
	const FName& GetTextureName() const { return TextureName; }

	const FConvexVolume GetOBB() { return ConvexVolume; }
	void SetOBB(FConvexVolume InOBB) { ConvexVolume = InOBB; }
	void UpdateOBBFromTransform();
	void OnTransformDirty() override;

	const TMeshData<FVertexPNCT>* GetDecalMeshData() const { return &DecalMeshData; }

private:
	void HandleFade(float DeltaTime);
	void UpdateDecalMesh();
	void DrawDebugBox();

private:
	FConvexVolume ConvexVolume;
	FName TextureName;
	FVector4 Color = {1,1,1,1};
	float FadeInDelay = 0;
	float FadeInDuration = 0;
	float FadeOutDelay = 0;
	float FadeOutDuration = 0;
	float FadeTimer = 0;
	float FadeOpacity = 1.0f;		// 페이드 효과 사용 시 Color.A에 곱함
	TMeshData<FVertexPNCT> DecalMeshData;
	FTextureResource* CachedTexture = nullptr;	// ResourceManager 소유, 참조만
};
