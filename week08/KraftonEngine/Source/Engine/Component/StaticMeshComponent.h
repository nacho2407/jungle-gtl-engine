#pragma once

#include "Component/MeshComponent.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

class UMaterial;

namespace json { class JSON; }

// UStaticMeshComp — 월드 배치 컴포넌트
class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

	UStaticMeshComponent() = default;
	~UStaticMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	void CollectRender(FRenderBus& Bus) const override;
	void CollectSelection(FRenderBus& Bus) const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	void UpdateWorldAABB() const override;

	void SetStaticMesh(UStaticMesh* InMesh);
	UStaticMesh* GetStaticMesh() const;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

	void Serialize(bool bIsLoading, json::JSON& Handle);

	// Property Editor 지원
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
	void CacheLocalBounds();

	UStaticMesh* StaticMesh = nullptr;
	FString StaticMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	TArray<FString> OverrideMaterialPaths;
	TArray<uint8> OverrideUVScrolls;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
