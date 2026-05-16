#include "AssetRegistry.h"
#include "Mesh/MeshManager.h"

#include <cstring>

namespace FAssetRegistry
{
	const TArray<FAssetListItem>& ListByTypeName(const char* AssetTypeName)
	{
		static const TArray<FAssetListItem> Empty;
		if (!AssetTypeName) return Empty;

		if (std::strcmp(AssetTypeName, "UStaticMesh") == 0)
		{
			return FMeshManager::GetAvailableStaticMeshFiles();
		}
		if (std::strcmp(AssetTypeName, "USkeletalMesh") == 0)
		{
			return FMeshManager::GetAvailableSkeletalMeshFiles();
		}
		// UAnimSequence: A 의 AnimSequence asset 임포트/스캔 통합 후 매핑 추가.
		// 그 전까지는 빈 배열 (드롭다운엔 "None" 만 보임).
		return Empty;
	}
}
