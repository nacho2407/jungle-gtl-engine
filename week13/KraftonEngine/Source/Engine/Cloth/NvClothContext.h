#pragma once

#include "Cloth/ClothTypes.h"

#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
class FClothSimulation;

/**
 * @brief NvCloth backend 공유 context
 */
class FNvClothContext
{
	friend class FClothSimulation;

public:
	FNvClothContext();
	~FNvClothContext();

	FNvClothContext(const FNvClothContext&) = delete;
	FNvClothContext& operator=(const FNvClothContext&) = delete;
	FNvClothContext(FNvClothContext&&) = delete;
	FNvClothContext& operator=(FNvClothContext&&) = delete;

	/**
	 * @brief NvCloth backend context를 초기화합니다
	 *
	 * @param InDevice DX11 backend 초기화에 사용할 D3D11 device
	 *
	 * @param InDeviceContext DX11 backend 초기화에 사용할 D3D11 device context
	 *
	 * @return NvCloth backend 사용 가능 여부
	 */
	bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext);

	/**
	 * @brief NvCloth backend context를 종료합니다
	 */
	void Shutdown();

	/**
	 * @brief NvCloth backend 초기화 시도 여부를 반환합니다
	 *
	 * @return 초기화 시도 여부
	 */
	bool IsInitializeAttempted() const { return bInitializeAttempted; }

	/**
	 * @brief 현재 Cloth backend 사용 가능 여부를 반환합니다
	 *
	 * @return 현재 Cloth backend 사용 가능 여부
	 */
	bool IsAvailable() const { return BackendStatus.bAvailable; }

	/**
	 * @brief 현재 선택된 Cloth backend 종류를 반환합니다
	 *
	 * @return 현재 선택된 Cloth backend 종류
	 */
	EClothBackendType GetBackendType() const { return BackendStatus.Backend; }

	/**
	 * @brief 현재 Cloth backend 초기화 상태를 반환합니다
	 *
	 * @return 현재 Cloth backend 초기화 상태
	 */
	const FClothBackendStatus& GetBackendStatus() const { return BackendStatus; }

private:
	struct FImpl;

	/**
	 * @brief NvCloth CUDA factory를 생성합니다
	 *
	 * @param OutFailureDetail factory 생성 실패 사유
	 *
	 * @return NvCloth CUDA factory 생성 성공 여부
	 */
	bool CreateCudaFactory(FString& OutFailureDetail);

	/**
	 * @brief NvCloth DX11 factory를 생성합니다
	 *
	 * @param OutFailureDetail factory 생성 실패 사유
	 *
	 * @return NvCloth DX11 factory 생성 성공 여부
	 */
	bool CreateDxFactory(FString& OutFailureDetail);

	/**
	 * @brief NvCloth CPU factory를 생성합니다
	 *
	 * @param OutFailureDetail factory 생성 실패 사유
	 *
	 * @return NvCloth CPU factory 생성 성공 여부
	 */
	bool CreateCpuFactory(FString& OutFailureDetail);

	/**
	 * @brief 현재 보유 중인 NvCloth factory를 해제합니다
	 */
	void ReleaseFactory();

	/**
	 * @brief simulation resource 생성에 사용할 내부 factory handle을 반환합니다
	 *
	 * @return 내부 NvCloth factory handle
	 */
	void* GetFactoryHandle() const;

private:
	// NvCloth 내부 타입 은닉과 factory 생명주기 소유권
	std::unique_ptr<FImpl> Impl;

	// stat cloth와 로그에서 공유할 backend 상태
	FClothBackendStatus BackendStatus;
	bool bInitializeAttempted = false;
};
