#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/MeshManager.h"
#include "Mesh/StaticMesh.h"

#include "Source/Engine/Component/StaticMeshComponent.generated.h"
class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// UStaticMeshComponent — 월드 배치 컴포넌트
UCLASS()
class UStaticMeshComponent : public UMeshComponent
{
public:
	GENERATED_BODY()
	UStaticMeshComponent() = default;
	~UStaticMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	bool LineTraceStaticMeshFast(const FRay& Ray, const FMatrix& WorldMatrix, const FMatrix& WorldInverse, FHitResult& OutHitResult);
	void UpdateWorldAABB() const override;

	// 구체 프록시 생성 (FStaticMeshSceneProxy)
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetStaticMesh(UStaticMesh* InMesh);
	UStaticMesh* GetStaticMesh() const;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

	void PostDuplicate() override;

	// Property Editor 지원
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
	void CacheLocalBounds();

	UStaticMesh* StaticMesh = nullptr;
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Static Mesh", Type=StaticMeshRef, AssetType="StaticMesh", AllowedClass="UStaticMesh")
	FString StaticMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	UPROPERTY(Edit, Save, Category="Materials", DisplayName="Materials", Type=MaterialSlotArray, AssetType="Material", AllowedClass="UMaterial")
	TArray<FMaterialSlot> MaterialSlots; // 경로 + UVScroll 묶음

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
