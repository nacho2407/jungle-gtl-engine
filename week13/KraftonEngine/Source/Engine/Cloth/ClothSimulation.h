#pragma once

#include "Cloth/ClothTypes.h"

class FNvClothContext;

/**
 * @brief 개별 Cloth simulation scaffold
 */
class FClothSimulation
{
public:
	FClothSimulation() = default;
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
	 * @return simulation 사용 가능 여부
	 */
	bool Initialize(FNvClothContext* InContext);

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
	 * @brief 연결된 Cloth backend 상태를 반환합니다
	 *
	 * @return 연결된 Cloth backend 상태
	 */
	const FClothBackendStatus& GetBackendStatus() const;

private:
	// 소유권 없이 참조하는 엔진 소유 NvCloth context
	FNvClothContext* Context = nullptr;
	bool bInitialized = false;
};
