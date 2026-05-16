#include "AssetRegistry.h"
#include "Mesh/MeshManager.h"
#include "Animation/AnimationManager.h"
#include "Platform/Paths.h"

#include <cstring>
#include <filesystem>

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
		if (std::strcmp(AssetTypeName, "LuaAnimScript") == 0)
		{
			// Asset/Script/Anim/ 하위 .lua 파일 즉석 스캔. 콤보 열 때만 호출되므로 비용 무시.
			// startup 캐싱 필요해지면 별도 매니저로 이관.
			static TArray<FAssetListItem> Cache;
			Cache.clear();

			namespace fs = std::filesystem;
			const fs::path Dir = fs::path(FPaths::RootDir()) / L"Asset" / L"Script" / L"Anim";
			if (fs::exists(Dir) && fs::is_directory(Dir))
			{
				for (const auto& Entry : fs::directory_iterator(Dir))
				{
					if (!Entry.is_regular_file()) continue;
					if (Entry.path().extension() != L".lua") continue;

					FAssetListItem Item;
					Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
					// FullPath 는 ULuaScriptComponent 와 동일 컨벤션 — ResolveScriptPath 가 받는 형태.
					Item.FullPath    = FPaths::ToUtf8(
						(fs::path(L"Anim") / Entry.path().filename()).generic_wstring());
					Cache.push_back(Item);
				}
			}
			return Cache;
		}

		return Empty;
	}
}
