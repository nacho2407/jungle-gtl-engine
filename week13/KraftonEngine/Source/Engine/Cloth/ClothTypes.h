#pragma once

#include "Core/Types/CoreTypes.h"

/**
 * @brief Cloth simulation backend 종류
 */
enum class EClothBackendType
{
	Unavailable,
	CUDA,
	DX11,
	CPU,
	Disabled
};

/**
 * @brief Cloth backend 초기화 상태
 */
struct FClothBackendStatus
{
	EClothBackendType Backend = EClothBackendType::Unavailable;
	bool bAvailable = false;
	FString Detail;
};

/**
 * @brief 지정된 Cloth backend 종류의 표시 이름을 반환합니다
 *
 * @param Backend 표시 이름으로 변환할 Cloth backend 종류
 *
 * @return backend 표시 이름
 */
const char* GetClothBackendName(EClothBackendType Backend);
