#pragma once

#include "Component/MeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// UStaticMeshComponent — 월드 배치 컴포넌트

#include "Source/Engine/Component/Primitive/StaticMeshComponent.generated.h"

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

	const FString& GetStaticMeshPath() const { return StaticMeshPath.ToString(); }

private:
	void CacheLocalBounds();

	TObjectPtr<UStaticMesh> StaticMesh;
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Static Mesh", AssetType="StaticMesh")
	FSoftObjectPtr StaticMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	UPROPERTY(Edit, Save, EditFixedSize, Category="Materials", DisplayName="Materials", AssetType="Material")
	TArray<FSoftObjectPtr> MaterialSlots;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
