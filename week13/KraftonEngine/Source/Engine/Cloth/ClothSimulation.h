#pragma once

#include "Cloth/ClothTypes.h"

#include <memory>

class FNvClothContext;

/**
 * @brief 개별 Cloth simulation instance
 */
class FClothSimulation
{
public:
	FClothSimulation();
	~FClothSimulation();

	FClothSimulation(const FClothSimulation&) = delete;
	FClothSimulation& operator=(const FClothSimulation&) = delete;
	FClothSimulation(FClothSimulation&&) = delete;
	FClothSimulation& operator=(FClothSimulation&&) = delete;

	/**
	 * @brief 지정된 NvCloth context를 사용하도록 simulation을 초기화합니다
	 *
	 * @param InContext simulation이 참조할 NvCloth context
	 *
	 * @param BuildDesc simulation resource 생성 입력
	 *
	 * @return simulation 사용 가능 여부
	 */
	bool Initialize(FNvClothContext* InContext, const FClothSimulationBuildDesc& BuildDesc);

	/**
	 * @brief 기존 simulation resource를 해제하고 새 입력으로 다시 생성합니다
	 *
	 * @param InContext simulation이 참조할 NvCloth context
	 *
	 * @param BuildDesc simulation resource 생성 입력
	 *
	 * @return simulation resource 재생성 성공 여부
	 */
	bool Rebuild(FNvClothContext* InContext, const FClothSimulationBuildDesc& BuildDesc);

	/**
	 * @brief hard pin particle의 위치와 inverse mass를 갱신합니다
	 *
	 * @param PinnedIndices hard pin으로 고정할 particle index 배열
	 *
	 * @param PinTargetPositionsComponentLocal pinned particle의 component local 목표 위치 배열
	 *
	 * @return pinning 갱신 성공 여부
	 */
	bool ApplyPinning(
		const TArray<uint32>& PinnedIndices,
		const TArray<FVector>& PinTargetPositionsComponentLocal);

	/**
	 * @brief component local collision primitive를 NvCloth collision data로 갱신합니다
	 *
	 * @param CollisionPrimitives component local 기준 collision primitive 배열
	 *
	 * @return collision primitive 갱신 성공 여부
	 */
	bool UpdateCollisionPrimitives(const TArray<FClothCollisionPrimitive>& CollisionPrimitives);

	/**
	 * @brief simulation 상태를 종료합니다
	 */
	void Shutdown();

	/**
	 * @brief simulation을 한 프레임 진행합니다
	 *
	 * @param DeltaTime simulation에 사용할 프레임 시간
	 *
	 * @param RuntimeConfig simulation step에 적용할 runtime 설정
	 *
	 * @param OutPositionsComponentLocal simulation 결과 particle 위치 배열
	 *
	 * @return simulation 결과 위치 갱신 여부
	 */
	bool Tick(
		float DeltaTime,
		const FClothSimulationRuntimeConfig& RuntimeConfig,
		TArray<FVector>& OutPositionsComponentLocal);

	/**
	 * @brief simulation 사용 가능 여부를 반환합니다
	 *
	 * @return simulation 사용 가능 여부
	 */
	bool IsSimulationAvailable() const;

	/**
	 * @brief 현재 simulation particle 수를 반환합니다
	 *
	 * @return 현재 simulation particle 수
	 */
	uint32 GetParticleCount() const { return ParticleCount; }

	/**
	 * @brief 현재 simulation index 수를 반환합니다
	 *
	 * @return 현재 simulation index 수
	 */
	uint32 GetIndexCount() const { return IndexCount; }

	/**
	 * @brief 현재 hard pin particle 수를 반환합니다
	 *
	 * @return 현재 hard pin particle 수
	 */
	uint32 GetPinnedCount() const { return PinnedCount; }

	/**
	 * @brief 마지막으로 적용된 collision primitive 수를 반환합니다
	 *
	 * @return 마지막 collision primitive snapshot 크기
	 */
	uint32 GetCollisionPrimitiveCount() const { return CollisionPrimitiveCount; }

	/**
	 * @brief 마지막 simulation tick에서 소비한 fixed step 수를 반환합니다
	 *
	 * @return 마지막 simulation tick fixed step 수
	 */
	uint32 GetLastStepCount() const { return LastStepCount; }

	/**
	 * @brief fixed timestep accumulator를 초기화합니다
	 */
	void ResetAccumulator();

	/**
	 * @brief 마지막 simulation resource 생성 실패 사유를 반환합니다
	 *
	 * @return 마지막 simulation resource 생성 실패 사유
	 */
	const FString& GetLastFailureDetail() const { return LastFailureDetail; }

	/**
	 * @brief 연결된 Cloth backend 상태를 반환합니다
	 *
	 * @return 연결된 Cloth backend 상태
	 */
	const FClothBackendStatus& GetBackendStatus() const;

private:
	struct FImpl;

	/**
	 * @brief runtime 설정을 NvCloth cloth instance에 반영합니다
	 *
	 * @param RuntimeConfig simulation step에 적용할 runtime 설정
	 */
	void ApplyRuntimeConfig(const FClothSimulationRuntimeConfig& RuntimeConfig);

	/**
	 * @brief component world transform 변화 기반 local-space motion을 NvCloth에 반영합니다
	 *
	 * @param MotionConfig local-space motion 적용 설정
	 */
	void ApplyLocalSpaceMotion(const FClothLocalSpaceMotionConfig& MotionConfig);

	/**
	 * @brief turbulence particle acceleration을 갱신합니다
	 *
	 * @param RuntimeConfig simulation step에 적용할 runtime 설정
	 */
	void ApplyTurbulenceAcceleration(const FClothSimulationRuntimeConfig& RuntimeConfig);

	/**
	 * @brief NvCloth solver를 지정된 step만큼 진행합니다
	 *
	 * @param FixedStep solver에 전달할 fixed delta time
	 *
	 * @return solver step 성공 여부
	 */
	bool SimulateStep(float FixedStep);

	/**
	 * @brief 현재 particle 위치를 component local 배열로 읽어옵니다
	 *
	 * @param OutPositionsComponentLocal 읽어온 particle 위치 배열
	 *
	 * @return particle 위치 readback 성공 여부
	 */
	bool ReadCurrentPositions(TArray<FVector>& OutPositionsComponentLocal);

	/**
	 * @brief NvCloth collision primitive를 모두 제거합니다
	 *
	 * @return collision primitive 제거 성공 여부
	 */
	bool ClearCollisionPrimitives();

	/**
	 * @brief simulation resource 생성 실패 상태를 기록합니다
	 *
	 * @param FailureDetail simulation resource 생성 실패 사유
	 *
	 * @return 항상 false
	 */
	bool SetBuildFailure(const FString& FailureDetail);

	// 소유권 없이 참조하는 엔진 소유 NvCloth context
	FNvClothContext* Context = nullptr;

	// NvCloth 내부 resource 은닉과 생명주기 소유권
	std::unique_ptr<FImpl> Impl;

	FString LastFailureDetail;
	uint32 ParticleCount = 0;
	uint32 IndexCount = 0;
	uint32 PinnedCount = 0;
	uint32 CollisionPrimitiveCount = 0;
	float AccumulatedTime = 0.0f;
	float SimulationTime = 0.0f;
	uint32 LastStepCount = 0;
	bool bInitialized = false;
	bool bValid = false;
	bool bSelfCollisionCullScaleWarningLogged = false;
};
