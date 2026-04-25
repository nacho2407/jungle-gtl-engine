#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Collision/ConvexVolume.h"

class UStaticMeshComponent;

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

	// --- Material ---
	void SetMaterial(class UMaterial* InMaterial);
	class UMaterial* GetMaterial() const { return Material; }

	const FConvexVolume GetDecalVolume() { return ConvexVolume; }
	void UpdateDecalVolumeFromTransform();
	void OnTransformDirty() override;

	const TArray<UStaticMeshComponent*>& GetReceivers() const { return Receivers; }

	class UBillboardComponent* EnsureEditorBillboard();

private:
	void HandleFade(float DeltaTime);
	void UpdateReceivers();
	void DrawDebugBox();

private:
	FConvexVolume ConvexVolume;
	TArray<UStaticMeshComponent*> Receivers;
	FMaterialSlot MaterialSlot;
	UMaterial* Material = nullptr;
	FVector4 Color = {1,1,1,1};
	float FadeInDelay = 0;
	float FadeInDuration = 0;
	float FadeOutDelay = 0;
	float FadeOutDuration = 0;
	float FadeTimer = 0;
	float FadeOpacity = 1.0f;		// 페이드 효과 사용 시 Color.A에 곱함
};
