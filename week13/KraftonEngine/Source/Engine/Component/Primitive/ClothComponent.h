#pragma once

#include "Cloth/ClothTypes.h"
#include "Component/MeshComponent.h"
#include "Materials/Material.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Primitive/ClothComponent.generated.h"

class FPrimitiveSceneProxy;
class UMaterial;

/**
 * @brief 절차적 cloth grid component
 */
UCLASS()
class UClothComponent : public UMeshComponent
{
public:
	GENERATED_BODY()

	UClothComponent();
	~UClothComponent() override = default;

	/**
	 * @brief Cloth render proxy를 생성합니다
	 *
	 * @return 생성된 cloth render proxy 또는 nullptr
	 */
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	/**
	 * @brief property 변경 후 cloth 상태를 갱신합니다
	 *
	 * @param PropertyName 변경된 property 이름
	 */
	void PostEditProperty(const char* PropertyName) override;

	/**
	 * @brief duplication 이후 저장된 asset 참조와 render data를 복원합니다
	 */
	void PostDuplicate() override;

	/**
	 * @brief GC reference collector에 runtime material 참조를 추가합니다
	 *
	 * @param Collector object reference collector
	 */
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	/**
	 * @brief line trace와 picking에 사용할 cloth mesh view를 반환합니다
	 *
	 * @return cloth mesh data view
	 */
	FMeshDataView GetMeshDataView() const override;

	/**
	 * @brief cached local bounds를 world AABB로 변환합니다
	 */
	void UpdateWorldAABB() const override;

	/**
	 * @brief 지정된 material slot에 material을 설정합니다
	 *
	 * @param ElementIndex material slot index
	 *
	 * @param InMaterial 설정할 material
	 */
	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);

	/**
	 * @brief 지정된 material slot의 material을 반환합니다
	 *
	 * @param ElementIndex 조회할 material slot index
	 *
	 * @return 지정된 slot의 material
	 */
	UMaterial* GetMaterial(int32 ElementIndex) const;

	/**
	 * @brief cloth grid rebuild를 다음 tick으로 지연 표시합니다
	 */
	void MarkClothRebuildDirty();

	/**
	 * @brief dirty 상태인 cloth grid를 다시 생성합니다
	 *
	 * @param bNotifyProxyDirty render proxy에 mesh dirty를 전파할지 여부
	 */
	void RebuildClothIfNeeded(bool bNotifyProxyDirty = true);

	/**
	 * @brief cloth CPU render data를 반환합니다
	 *
	 * @return cloth CPU render data
	 */
	const FClothRenderData& GetClothRenderData() const { return RenderData; }

	/**
	 * @brief 현재 render revision을 반환합니다
	 *
	 * @return 현재 render revision
	 */
	uint64 GetRenderRevision() const { return RenderData.Revision; }

protected:
	/**
	 * @brief component tick에서 dirty rebuild와 backend status 로그를 처리합니다
	 *
	 * @param DeltaTime 프레임 delta time
	 *
	 * @param TickType level tick 종류
	 *
	 * @param ThisTickFunction 현재 component tick function
	 */
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	/**
	 * @brief property 값을 안전한 cloth config로 변환합니다
	 *
	 * @return 보정된 cloth config
	 */
	FClothConfig MakeClothConfig() const;

	/**
	 * @brief 현재 config로 procedural grid를 생성합니다
	 *
	 * @param Config grid 생성에 사용할 cloth config
	 *
	 * @param bNotifyProxyDirty render proxy에 mesh dirty를 전파할지 여부
	 */
	void BuildGrid(const FClothConfig& Config, bool bNotifyProxyDirty);

	/**
	 * @brief triangle과 UV 기준으로 normal과 tangent를 다시 계산합니다
	 */
	void RecalculateNormalsAndTangents();

	/**
	 * @brief single material section 정보를 갱신합니다
	 */
	void UpdateRenderSections();

	/**
	 * @brief render data 기준 local bounds cache를 갱신합니다
	 *
	 * @param Config bounds margin을 제공하는 cloth config
	 */
	void UpdateLocalBoundsFromRenderData(const FClothConfig& Config);

	/**
	 * @brief render data revision을 증가시킵니다
	 */
	void IncrementRenderRevision();

	/**
	 * @brief 저장된 material slot 경로에서 runtime material을 다시 불러옵니다
	 */
	void LoadMaterialFromSlot();

	/**
	 * @brief Cloth backend 상태를 한 번만 로그로 기록합니다
	 */
	void LogBackendStatusOnce();

private:
	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Num Particles X", Min=2.0f, Max=256.0f, Speed=1.0f)
	int32 NumParticlesX = 20;

	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Num Particles Y", Min=2.0f, Max=256.0f, Speed=1.0f)
	int32 NumParticlesY = 20;

	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Particle Spacing", Min=0.1f, Max=1000.0f, Speed=1.0f)
	float ParticleSpacing = 10.0f;

	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Material", AssetType="Material")
	FSoftObjectPtr MaterialSlot = "None";

	UPROPERTY(Transient, Category="Rendering")
	TObjectPtr<UMaterial> Material = nullptr;

	FClothRenderData RenderData;
	FVector CachedLocalCenter = FVector::ZeroVector;
	FVector CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
	bool bHasValidLocalBounds = false;
	bool bClothRebuildDirty = true;
	bool bBackendStatusLogged = false;
};
