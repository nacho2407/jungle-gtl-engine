#pragma once

#include "Core/CoreMinimal.h"
#include "Render/Resource/Material.h"

/**
 * @brief Obj전용 .mtl 파일 파서
 */
class FObjMtlLoader
{
public:
	/**
	 * @brief MTL 파일을 파싱하여 머테리얼 맵을 채웁니다.
	 * @param FilePath
	 * @param OutMaterials
	 * @return 파일 열기 성공 여부
	 */
	static bool Load(const FString& FilePath, TMap<FString, UMaterial*>& OutMaterialAssets, ID3D11Device* Device);
};
