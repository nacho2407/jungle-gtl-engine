#include "AssetRegistry.h"
#include "Mesh/MeshManager.h"
#include "Animation/AnimationManager.h"

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
		if (std::strcmp(AssetTypeName, "UAnimSequence") == 0)
		{
			return FAnimationManager::Get().GetAvailableAnimationFiles();
		}

		return Empty;
	}
}
