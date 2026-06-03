#pragma once

#include "Cloth/ClothSimulation.h"
#include "Cloth/ClothTypes.h"
#include "Component/MeshComponent.h"
#include "Math/Rotator.h"
#include "Materials/Material.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Primitive/ClothComponent.generated.h"

class FPrimitiveSceneProxy;
class UMaterial;
class UPrimitiveComponent;
class USkeletalMeshComponent;

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
	 * @brief component 제거 경로에서 cloth simulation resource를 해제합니다
	 */
	void RouteComponentDestroyed() override;

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
	 * @brief cloth simulation 전체 재생성을 지연 표시합니다
	 */
	void MarkClothSimulationRebuildDirty();

	/**
	 * @brief cloth pin selection 갱신을 지연 표시합니다
	 */
	void MarkClothPinningDirty();

	/**
	 * @brief cloth pin target 갱신을 지연 표시합니다
	 */
	void MarkClothPinTargetDirty();

	/**
	 * @brief cloth force parameter 갱신을 지연 표시합니다
	 */
	void MarkClothForceDirty();

	/**
	 * @brief cloth collision primitive 갱신을 지연 표시합니다
	 */
	void MarkClothCollisionDirty();

	/**
	 * @brief cloth editor preview 정책 갱신을 지연 표시합니다
	 */
	void MarkClothEditorPreviewDirty();

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

	/**
	 * @brief 현재 hard pin으로 선택된 particle 수를 반환합니다
	 *
	 * @return 현재 hard pin particle 수
	 */
	uint32 GetPinnedCount() const { return static_cast<uint32>(CachedPinnedIndices.size()); }

	/**
	 * @brief 현재 hard pin으로 선택된 particle index 배열을 반환합니다
	 *
	 * @return 현재 hard pin particle index 배열
	 */
	const TArray<uint32>& GetCachedPinnedIndices() const { return CachedPinnedIndices; }

	/**
	 * @brief 현재 actor local hard pin 선택 영역 debug shape를 생성합니다
	 *
	 * @param OutShape 생성된 component local 기준 debug shape
	 *
	 * @return debug shape 생성 여부
	 */
	bool BuildPinSelectionDebugShape(FClothPinSelectionDebugShape& OutShape) const;

	/**
	 * @brief 현재 simulation에 적용된 collision primitive 수를 반환합니다
	 *
	 * @return 현재 collision primitive 수
	 */
	uint32 GetCollisionPrimitiveCount() const { return Simulation.GetCollisionPrimitiveCount(); }

	/**
	 * @brief 현재 debug 표시용 collision primitive snapshot을 반환합니다
	 *
	 * @return component local 기준 collision primitive snapshot
	 */
	const TArray<FClothCollisionPrimitive>& GetCachedCollisionPrimitives() const { return CachedCollisionPrimitives; }

	/**
	 * @brief 현재 independent collision primitive 수를 반환합니다
	 *
	 * @return 현재 independent collision primitive 수
	 */
	uint32 GetCachedIndependentCollisionPrimitiveCount() const { return CachedIndependentCollisionPrimitiveCount; }

	/**
	 * @brief 현재 body collision primitive 수를 반환합니다
	 *
	 * @return 현재 body collision primitive 수
	 */
	uint32 GetCachedBodyCollisionPrimitiveCount() const { return CachedBodyCollisionPrimitiveCount; }

	/**
	 * @brief 최근 tick에서 계산된 최종 wind vector를 반환합니다
	 *
	 * @return world 기준 최종 wind vector
	 */
	FVector GetCachedFinalWindVelocityWorld() const { return CachedFinalWindVelocityWorld; }

	/**
	 * @brief 최근 tick에서 계산된 owner motion delta를 반환합니다
	 *
	 * @return world 기준 owner motion delta
	 */
	FVector GetCachedOwnerMotionDeltaWorld() const { return CachedOwnerMotionDeltaWorld; }

	/**
	 * @brief 최근 tick에서 global wind 적용 여부를 반환합니다
	 *
	 * @return global wind 적용 여부
	 */
	bool WasGlobalWindAppliedLastTick() const { return bGlobalWindAppliedLastTick; }

	/**
	 * @brief 최근 tick에서 owner motion inertia 적용 여부를 반환합니다
	 *
	 * @return owner motion inertia 적용 여부
	 */
	bool WasOwnerMotionInertiaAppliedLastTick() const { return bOwnerMotionInertiaAppliedLastTick; }

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
	 * @brief 현재 property와 transform 기준 runtime simulation 설정을 생성합니다
	 *
	 * @param Config timestep 보정값을 제공하는 cloth config
	 *
	 * @return runtime simulation 설정
	 */
	FClothSimulationRuntimeConfig MakeSimulationRuntimeConfig(const FClothConfig& Config, float DeltaTime) const;

	/**
	 * @brief owner/root forward 기준 이동 속도를 world 기준으로 계산합니다
	 *
	 * @param CurrentClothWorldTransform 현재 cloth world transform
	 *
	 * @param DeltaTime 프레임 delta time
	 *
	 * @return world 기준 owner/root forward 방향 이동 속도
	 */
	FVector ComputeOwnerMotionVelocityWorld(const FTransform& CurrentClothWorldTransform, float DeltaTime) const;

	/**
	 * @brief owner motion inertia cache를 초기화합니다
	 */
	void ResetOwnerMotionCache();

	/**
	 * @brief 현재 runtime config 기준 owner motion inertia cache를 저장합니다
	 *
	 * @param RuntimeConfig 이번 tick에 사용한 runtime 설정
	 */
	void StoreOwnerMotionCache(const FClothSimulationRuntimeConfig& RuntimeConfig);

	/**
	 * @brief 현재 tick에서 simulation을 진행해야 하는지 반환합니다
	 *
	 * @param TickType level tick 종류
	 *
	 * @return simulation tick 허용 여부
	 */
	bool ShouldTickSimulation(ELevelTick TickType) const;

	/**
	 * @brief editor preview simulation에 필요한 owner actor tick 정책을 적용합니다
	 */
	void ApplyEditorPreviewPolicy();

	/**
	 * @brief editor preview simulation 결과를 rest pose로 되돌립니다
	 *
	 * @param Config bounds 갱신에 사용할 cloth config
	 */
	void ResetEditorPreviewSimulationState(const FClothConfig& Config);

	/**
	 * @brief render data 위치를 rest position cache 기준으로 복원합니다
	 *
	 * @param Config bounds 갱신에 사용할 cloth config
	 *
	 * @return rest position 복원 성공 여부
	 */
	bool RestoreRenderDataToRestPositions(const FClothConfig& Config);

	/**
	 * @brief 현재 config로 procedural grid를 생성합니다
	 *
	 * @param Config grid 생성에 사용할 cloth config
	 *
	 * @param bNotifyProxyDirty render proxy에 mesh dirty를 전파할지 여부
	 */
	void BuildGrid(const FClothConfig& Config, bool bNotifyProxyDirty);

	/**
	 * @brief 현재 render data를 simulation build 입력으로 변환합니다
	 *
	 * @param Config simulation build 입력에 포함할 cloth config
	 *
	 * @return simulation resource 생성 입력
	 */
	FClothSimulationBuildDesc BuildSimulationDesc(const FClothConfig& Config) const;

	/**
	 * @brief 현재 property 기준 hard pin particle과 target 위치를 계산합니다
	 *
	 * @param Config pin index 계산에 사용할 cloth config
	 *
	 * @param OutPinnedIndices 계산된 hard pin particle index 배열
	 *
	 * @param OutPinTargetPositionsComponentLocal 계산된 component local target 위치 배열
	 */
	void BuildPinnedParticles(
		const FClothConfig& Config,
		TArray<uint32>& OutPinnedIndices,
		TArray<FVector>& OutPinTargetPositionsComponentLocal) const;

	/**
	 * @brief dirty 상태인 cloth simulation resource를 다시 생성합니다
	 *
	 * @param Config simulation build 입력에 포함할 cloth config
	 */
	void RebuildSimulationIfNeeded(const FClothConfig& Config);

	/**
	 * @brief dirty 상태인 hard pin 정보를 기존 simulation resource에 반영합니다
	 *
	 * @param Config pin index 계산에 사용할 cloth config
	 */
	void ApplySimulationPinningIfNeeded(const FClothConfig& Config);

	/**
	 * @brief cloth simulation을 진행하고 render data에 결과를 반영합니다
	 *
	 * @param DeltaTime 프레임 delta time
	 *
	 * @param TickType level tick 종류
	 *
	 * @param Config bounds와 timestep 보정값을 제공하는 cloth config
	 */
	void TickSimulationIfNeeded(float DeltaTime, ELevelTick TickType, const FClothConfig& Config);

	/**
	 * @brief 현재 actor local collision property를 component local primitive 배열로 변환합니다
	 *
	 * @param OutPrimitives component local 기준 collision primitive 배열
	 */
	void BuildCollisionPrimitivesComponentLocal(TArray<FClothCollisionPrimitive>& OutPrimitives) const;

	/**
	 * @brief body collision source로 사용할 primitive component를 반환합니다
	 *
	 * @return 선택된 primitive component 또는 nullptr
	 */
	UPrimitiveComponent* ResolveBodyCollisionSource() const;

	/**
	 * @brief body collision source component의 world 기준 primitive snapshot을 생성합니다
	 *
	 * @param SourceComponent body collision source primitive component
	 *
	 * @param OutPrimitives world 기준 body collision primitive 배열
	 */
	void BuildBodyCollisionPrimitivesWorld(
		const UPrimitiveComponent* SourceComponent,
		TArray<FClothCollisionPrimitive>& OutPrimitives) const;

	/**
	 * @brief world 기준 body collision primitive 배열을 component local 배열에 추가합니다
	 *
	 * @param BodyWorldPrimitives world 기준 body collision primitive 배열
	 *
	 * @param OutPrimitives component local 기준 collision primitive 배열
	 *
	 * @return 실제 추가된 body collision primitive 수
	 */
	uint32 AppendBodyCollisionPrimitivesComponentLocal(
		const TArray<FClothCollisionPrimitive>& BodyWorldPrimitives,
		TArray<FClothCollisionPrimitive>& OutPrimitives) const;

	/**
	 * @brief world 기준 collision primitive를 component local primitive로 변환합니다
	 *
	 * @param WorldPrimitive world 기준 collision primitive
	 *
	 * @param OutComponentLocalPrimitive component local 기준 collision primitive
	 */
	void ConvertWorldCollisionPrimitiveToComponentLocal(
		const FClothCollisionPrimitive& WorldPrimitive,
		FClothCollisionPrimitive& OutComponentLocalPrimitive) const;

	/**
	 * @brief 현재 collision primitive snapshot을 simulation에 반영합니다
	 */
	void UpdateSimulationCollisionPrimitives();

	/**
	 * @brief simulation readback 위치를 render data에 반영합니다
	 *
	 * @param Config bounds margin을 제공하는 cloth config
	 *
	 * @param PositionsComponentLocal component local 기준 simulation 위치 배열
	 *
	 * @return render data 갱신 성공 여부
	 */
	bool ApplySimulationPositionsToRenderData(
		const FClothConfig& Config,
		const TArray<FVector>& PositionsComponentLocal);

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

	/**
	 * @brief actor local 위치를 component local 위치로 변환합니다
	 *
	 * @param ActorLocalPoint actor local 기준 위치
	 *
	 * @return component local 기준 위치
	 */
	FVector TransformActorLocalPointToComponentLocal(const FVector& ActorLocalPoint) const;

	/**
	 * @brief actor local vector를 component local vector로 변환합니다
	 *
	 * @param ActorLocalVector actor local 기준 vector
	 *
	 * @return component local 기준 vector
	 */
	FVector TransformActorLocalVectorToComponentLocal(const FVector& ActorLocalVector) const;

	/**
	 * @brief actor local 방향을 component local 단위 방향으로 변환합니다
	 *
	 * @param ActorLocalDirection actor local 기준 방향
	 *
	 * @return component local 기준 단위 방향
	 */
	FVector TransformActorLocalDirectionToComponentLocal(const FVector& ActorLocalDirection) const;

	/**
	 * @brief world vector를 component local vector로 변환합니다
	 *
	 * @param WorldVector world 기준 vector
	 *
	 * @return component local 기준 vector
	 */
	FVector TransformWorldVectorToComponentLocal(const FVector& WorldVector) const;

	/**
	 * @brief world 위치를 component local 위치로 변환합니다
	 *
	 * @param WorldPoint world 기준 위치
	 *
	 * @return component local 기준 위치
	 */
	FVector TransformWorldPointToComponentLocal(const FVector& WorldPoint) const;

	/**
	 * @brief world 방향을 component local 단위 방향으로 변환합니다
	 *
	 * @param WorldDirection world 기준 방향
	 *
	 * @return component local 기준 단위 방향
	 */
	FVector TransformWorldDirectionToComponentLocal(const FVector& WorldDirection) const;

	/**
	 * @brief world 길이를 component local 보수적 길이로 변환합니다
	 *
	 * @param WorldLength world 기준 길이
	 *
	 * @return component local 기준 보수적 길이
	 */
	float TransformWorldLengthToComponentLocal(float WorldLength) const;

	/**
	 * @brief actor local plane을 component local plane으로 변환합니다
	 *
	 * @param ActorLocalPoint actor local 기준 plane 위 위치
	 *
	 * @param ActorLocalNormal actor local 기준 plane normal
	 *
	 * @param OutComponentLocalPoint component local 기준 plane 위 위치
	 *
	 * @param OutComponentLocalNormal component local 기준 plane normal
	 */
	void TransformActorLocalPlaneToComponentLocal(
		const FVector& ActorLocalPoint,
		const FVector& ActorLocalNormal,
		FVector& OutComponentLocalPoint,
		FVector& OutComponentLocalNormal) const;

	/**
	 * @brief actor local capsule을 component local capsule endpoint로 변환합니다
	 *
	 * @param ActorLocalCenter actor local 기준 capsule 중심
	 *
	 * @param ActorLocalAxis actor local 기준 capsule 축
	 *
	 * @param HalfHeight capsule 절반 높이
	 *
	 * @param OutComponentLocalStart component local 기준 capsule 시작점
	 *
	 * @param OutComponentLocalEnd component local 기준 capsule 끝점
	 */
	void TransformActorLocalCapsuleToComponentLocal(
		const FVector& ActorLocalCenter,
		const FVector& ActorLocalAxis,
		float HalfHeight,
		FVector& OutComponentLocalStart,
		FVector& OutComponentLocalEnd) const;

	/**
	 * @brief actor local 길이를 component local 보수적 길이로 변환합니다
	 *
	 * @param ActorLocalLength actor local 기준 길이
	 *
	 * @return component local 기준 보수적 길이
	 */
	float TransformActorLocalLengthToComponentLocal(float ActorLocalLength) const;

private:
	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Num Particles X", Min=2.0f, Max=2048.0f, Speed=1.0f)
	int32 NumParticlesX = 20;

	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Num Particles Y", Min=2.0f, Max=2048.0f, Speed=1.0f)
	int32 NumParticlesY = 20;

	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Particle Spacing", Min=0.001f, Max=1000.0f, Speed=0.1f)
	float ParticleSpacing = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Enable Simulation")
	bool bEnableSimulation = true;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Fixed Time Step", Min=0.001f, Max=0.1f, Speed=0.001f)
	float FixedTimeStep = 1.0f / 60.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Max Substeps", Min=1.0f, Max=4.0f, Speed=1.0f)
	int32 MaxSubsteps = 2;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Max Accumulated Time", Min=0.016f, Max=1.0f, Speed=0.01f)
	float MaxAccumulatedTime = 0.25f;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Gravity Scale", Min=0.0f, Max=10.0f, Speed=0.05f)
	float GravityScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Damping", Min=0.0f, Max=1.0f, Speed=0.01f)
	float Damping = 0.1f;

	UPROPERTY(Edit, Save, Category="Cloth|Simulation", DisplayName="Stiffness", Min=0.0f, Max=1.0f, Speed=0.01f)
	float Stiffness = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pinning Mode", Enum=EClothPinSelectionType)
	EClothPinSelectionType PinningMode = EClothPinSelectionType::TopEdge;

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Center Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PinCenterActorLocal = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Offset Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PinOffsetActorLocal = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Radius", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float PinRadius = 50.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Box Extent Actor Local", Type=Vec3, Min=0.0f, Max=10000.0f, Speed=1.0f)
	FVector PinBoxExtentActorLocal = FVector(50.0f, 50.0f, 50.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Rect Min Actor Local XZ", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PinRectMinActorLocalXZ = FVector(-50.0f, 0.0f, -50.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Pinning", DisplayName="Pin Rect Max Actor Local XZ", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PinRectMaxActorLocalXZ = FVector(50.0f, 0.0f, 50.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Enable Wind")
	bool bEnableWind = false;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Use Global Wind")
	bool bUseGlobalWind = true;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Global Wind Response", Min=0.0f, Max=10.0f, Speed=0.05f)
	float GlobalWindResponse = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Local Wind Scale", Min=0.0f, Max=10.0f, Speed=0.05f)
	float LocalWindScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Turbulence Response", Min=0.0f, Max=10.0f, Speed=0.05f)
	float TurbulenceResponse = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Direction", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector WindDirection = FVector::ForwardVector;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Strength", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float WindStrength = 0.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Turbulence Strength", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float WindTurbulenceStrength = 0.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Turbulence Spatial Scale", Min=0.001f, Max=10000.0f, Speed=1.0f)
	float WindTurbulenceSpatialScale = 100.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Turbulence Temporal Scale", Min=0.0f, Max=100.0f, Speed=0.01f)
	float WindTurbulenceTemporalScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Turbulence Seed", Min=0.0f, Max=1000000.0f, Speed=1.0f)
	int32 WindTurbulenceSeed = 1337;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Drag Coefficient", Min=0.0f, Max=10.0f, Speed=0.01f)
	float WindDragCoefficient = 0.5f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Lift Coefficient", Min=0.0f, Max=10.0f, Speed=0.01f)
	float WindLiftCoefficient = 0.05f;

	UPROPERTY(Edit, Save, Category="Cloth|Wind", DisplayName="Wind Fluid Density", Min=0.0f, Max=10.0f, Speed=0.01f)
	float WindFluidDensity = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Enable Owner Motion Inertia")
	bool bEnableOwnerMotionInertia = true;

	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Enable Owner Motion Wind")
	bool bEnableOwnerMotionWind = true;

	/**
	 * @brief owner 이동 속도에 비례한 역방향 wind 반응 계수
	 *
	 * @details 1.0은 owner 이동 속도와 같은 크기의 역방향 wind이며, 1.0 초과 값은 게임용 과장 반응입니다
	 */
	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Owner Motion Wind Response", Min=0.0f, Max=10.0f, Speed=0.05f)
	float OwnerMotionWindResponse = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Owner Motion Wind Max Speed", Min=0.0f, Max=100000.0f, Speed=10.0f)
	float OwnerMotionWindMaxSpeed = 3000.0f;

	/**
	 * @brief owner 이동에 대한 선형 관성 반응 계수
	 *
	 * @details 1.0은 NvCloth 기준 물리값이며, 1.0 초과 값은 게임용 과장 반응입니다
	 */
	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Linear Inertia Response", Min=0.0f, Max=3.0f, Speed=0.01f)
	float OwnerLinearInertiaResponse = 0.35f;

	/**
	 * @brief owner 이동에 대한 각 관성 반응 계수
	 *
	 * @details 1.0은 NvCloth 기준 물리값이며, 1.0 초과 값은 게임용 과장 반응입니다
	 */
	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Angular Inertia Response", Min=0.0f, Max=3.0f, Speed=0.01f)
	float OwnerAngularInertiaResponse = 0.15f;

	/**
	 * @brief owner 이동에 대한 원심 관성 반응 계수
	 *
	 * @details 1.0은 NvCloth 기준 물리값이며, 1.0 초과 값은 게임용 과장 반응입니다
	 */
	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Centrifugal Inertia Response", Min=0.0f, Max=3.0f, Speed=0.01f)
	float OwnerCentrifugalInertiaResponse = 0.15f;

	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Owner Motion Teleport Distance", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float OwnerMotionTeleportDistance = 300.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Owner Motion", DisplayName="Owner Motion Teleport Angle", Min=0.0f, Max=180.0f, Speed=1.0f)
	float OwnerMotionTeleportAngleDegrees = 45.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Self Collision", DisplayName="Enable Self Collision")
	bool bEnableSelfCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Self Collision", DisplayName="Self Collision Distance", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float SelfCollisionDistance = 2.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Self Collision", DisplayName="Self Collision Stiffness", Min=0.0f, Max=1.0f, Speed=0.01f)
	float SelfCollisionStiffness = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Self Collision", DisplayName="Self Collision Cull Scale", Min=0.0f, Max=10.0f, Speed=0.01f)
	float SelfCollisionCullScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Enable Sphere Collision")
	bool bEnableSphereCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Sphere Center Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector SphereCenterActorLocal = FVector(0.0f, 0.0f, -50.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Sphere Radius", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float SphereRadius = 25.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Enable Plane Collision")
	bool bEnablePlaneCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Plane Point Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PlanePointActorLocal = FVector(0.0f, 0.0f, -100.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Plane Normal Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector PlaneNormalActorLocal = FVector::UpVector;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Enable Capsule Collision")
	bool bEnableCapsuleCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Capsule Center Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector CapsuleCenterActorLocal = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Capsule Axis Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector CapsuleAxisActorLocal = FVector::UpVector;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Capsule Radius", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float CapsuleRadius = 15.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Capsule Half Height", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float CapsuleHalfHeight = 50.0f;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Enable Box Collision")
	bool bEnableBoxCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Box Center Actor Local", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector BoxCenterActorLocal = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Box Extent Actor Local", Type=Vec3, Min=0.0f, Max=10000.0f, Speed=1.0f)
	FVector BoxExtentActorLocal = FVector(25.0f, 25.0f, 25.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Collision", DisplayName="Box Rotation Actor Local", Type=Rotator, Min=0.0f, Max=0.0f, Speed=0.1f)
	FRotator BoxRotationActorLocal = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(Edit, Save, Category="Cloth|Body Collision", DisplayName="Enable Body Collision")
	bool bEnableBodyCollision = false;

	UPROPERTY(Edit, Save, Category="Cloth|Body Collision", DisplayName="Auto Find Owner Body Collision")
	bool bAutoFindOwnerBodyCollision = true;

	UPROPERTY(Edit, Save, Category="Cloth|Body Collision", DisplayName="Collision Source Component Name")
	FName CollisionSourceComponentName = FName::None;

	UPROPERTY(Edit, Save, Category="Cloth|Body Collision", DisplayName="Max Body Collision Primitives", Min=0.0f, Max=128.0f, Speed=1.0f)
	int32 MaxBodyCollisionPrimitives = 64;

	UPROPERTY(Edit, Save, Category="Cloth|Editor", DisplayName="Simulate In Editor")
	bool bSimulateInEditor = false;

	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Material", AssetType="Material")
	FSoftObjectPtr MaterialSlot = "None";

	UPROPERTY(Transient, Category="Rendering")
	TObjectPtr<UMaterial> Material = nullptr;

	FClothRenderData RenderData;
	FClothSimulation Simulation;
	TArray<FVector> RestPositionsComponentLocal;
	TArray<FVector> SimulationReadbackPositions;
	TArray<uint32> CachedPinnedIndices;
	TArray<FVector> CachedPinTargetPositionsComponentLocal;
	TArray<FClothCollisionPrimitive> CachedCollisionPrimitives;
	uint32 CachedIndependentCollisionPrimitiveCount = 0;
	uint32 CachedBodyCollisionPrimitiveCount = 0;
	mutable FVector CachedFinalWindVelocityWorld = FVector::ZeroVector;
	mutable FVector CachedOwnerMotionDeltaWorld = FVector::ZeroVector;
	FTransform PreviousClothWorldTransform;
	FVector CachedLocalCenter = FVector::ZeroVector;
	FVector CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
	bool bHasValidLocalBounds = false;
	mutable bool bGlobalWindAppliedLastTick = false;
	bool bHasPreviousClothWorldTransform = false;
	mutable bool bOwnerMotionInertiaAppliedLastTick = false;
	bool bTopologyRebuildDirty = true;
	bool bSimulationRebuildDirty = true;
	bool bPinningDirty = true;
	bool bPinTargetDirty = true;
	bool bForceConfigDirty = true;
	bool bCollisionDirty = true;
	bool bEditorPreviewDirty = true;
	bool bBackendStatusLogged = false;
	bool bSimulationTickWarningLogged = false;
	bool bCollisionUpdateWarningLogged = false;
	bool bLastSimulationBuildSkippedByAllPinned = false;
};
