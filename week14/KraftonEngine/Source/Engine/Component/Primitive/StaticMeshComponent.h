#pragma once

#include "Component/MeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

class UMaterialInterface;
class FPrimitiveSceneProxy;
class USkinnedMeshComponent;
class AActor;

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

	void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterialInterface*>& GetOverrideMaterials() const { return OverrideMaterials; }

	void PostDuplicate() override;

	// Property Editor 지원
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath.ToString(); }

	/**
	 * @brief Static Mesh의 물리 충돌을 bounds box collider로 대체할지 여부를 반환합니다
	 *
	 * @return bounds box collider 사용 여부
	 */
	bool ShouldUseBoundingBoxCollider() const { return bUseBoundingBoxCollider; }

	/**
	 * @brief 캐시된 Static Mesh 로컬 bounds를 반환합니다
	 *
	 * @param OutCenter Static Mesh 로컬 bounds 중심
	 *
	 * @param OutExtent Static Mesh 로컬 bounds 반치수
	 *
	 * @return 유효한 bounds 보유 여부
	 */
	bool GetCachedLocalBounds(FVector& OutCenter, FVector& OutExtent) const;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void CacheLocalBounds();
	void UpdateBoneAttachment();
	USkinnedMeshComponent* ResolveAttachMeshComponent();
	AActor* ResolveAttachTargetActor();
	bool TryGetSocketOffsetMatrix(
		FString& OutBoneName,
		FMatrix& OutSocketToObjectMatrix,
		FMatrix* OutObjectWorldMatrixAtExport = nullptr,
		bool* bOutHasObjectWorldMatrixAtExport = nullptr,
		bool* bOutIsSocketLocalMeshSpace = nullptr);
	FMatrix BuildAttachLocalMatrix(USkinnedMeshComponent* TargetMeshComponent, const FString& EffectiveBoneName);
	void InvalidateAttachSocketCache();
	void CaptureAttachOffsetFromCurrentTransform();

	TObjectPtr<UStaticMesh> StaticMesh;
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Static Mesh", AssetType="StaticMesh")
	FSoftObjectPtr StaticMeshPath = "None";

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach To Mesh Bone")
	bool bAttachToOwnerMeshBone = false;

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach Target Actor")
	FString AttachTargetActorName = "";

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Socket Offset JSON")
	FString AttachSocketOffsetPath = "";

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Socket Object Name")
	FString AttachSocketObjectName = "";

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Use Baked Bind Pose Correction")
	bool bUseBakedBindPoseCorrection = false;

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Edit Attach Offset")
	bool bEditAttachOffset = false;

	UPROPERTY(Edit, Category="StaticMesh|Attach", DisplayName="Capture Current Attach Offset")
	bool bCaptureCurrentAttachOffset = false;

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach Bone")
	FString AttachBoneName = "";

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach Offset Location", Type=Vec3, Speed=0.01f)
	FVector AttachOffsetLocation = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach Offset Rotation", Type=Rotator, Speed=0.1f)
	FRotator AttachOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(Edit, Save, Category="StaticMesh|Attach", DisplayName="Attach Offset Scale", Type=Vec3, Speed=0.01f)
	FVector AttachOffsetScale = FVector(1.0f, 1.0f, 1.0f);

	USkinnedMeshComponent* AttachMeshComponent = nullptr;
	AActor* AttachTargetActor = nullptr;
	bool bSocketOffsetCacheResolved = false;
	bool bCachedSocketOffsetValid = false;
	FString CachedAttachSocketOffsetPath = "";
	FString CachedAttachSocketObjectName = "";
	FString CachedSocketTargetBoneName = "";
	FMatrix CachedSocketToObjectMatrix = FMatrix::Identity;
	FMatrix CachedObjectWorldMatrixAtExport = FMatrix::Identity;
	bool bCachedHasObjectWorldMatrixAtExport = false;
	bool bCachedIsSocketLocalMeshSpace = false;

	TArray<UMaterialInterface*> OverrideMaterials;
	UPROPERTY(Edit, Save, EditFixedSize, Category="Materials", DisplayName="Materials", AssetType="Material")
	TArray<FSoftObjectPtr> MaterialSlots;

	// Static Mesh 물리 충돌을 렌더 bounds 기준 박스 collider로 대체하는 옵션
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Use Bounding Box Collider")
	bool bUseBoundingBoxCollider = false;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
