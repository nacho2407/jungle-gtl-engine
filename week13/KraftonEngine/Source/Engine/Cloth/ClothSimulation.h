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
	 * @brief simulation 상태를 종료합니다
	 */
	void Shutdown();

	/**
	 * @brief simulation을 한 프레임 진행합니다
	 *
	 * @param DeltaTime simulation에 사용할 프레임 시간
	 */
	void Tick(float DeltaTime);

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
	bool bInitialized = false;
	bool bValid = false;
};
