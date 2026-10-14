#include "AssetRegistry.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Animation/AnimationManager.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Skeleton/Skeleton.h"
#include "Animation/Skeleton/SkeletonManager.h"
#include "Platform/Paths.h"
#include "Particles/ParticleSystemManager.h"
#include "Physics/PhysicalMaterialManager.h"

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
        if (std::strcmp(AssetTypeName, "USkeleton") == 0)
        {
            return FSkeletonManager::Get().GetAvailableSkeletonFiles();
        }
		if (std::strcmp(AssetTypeName, "UAnimSequence") == 0)
		{
			// 콤보 열 때마다 재스캔 — 기존엔 MeshEditor 열 때만 Refresh 가 호출돼 PropertyWidget
			// 의 AnimSequence 콤보가 비어 있는 경우가 잦았음 (특히 새 PIE / 새 SkeletalMesh).
			FAnimationManager::Get().RefreshAvailableAnimations();
			return FAnimationManager::Get().GetAvailableAnimationFiles();
		}
		if (std::strcmp(AssetTypeName, "UAnimGraphAsset") == 0)
		{
			// 콤보 열 때마다 재스캔 — 방금 ContentBrowser 에서 만든 자산이 즉시 노출되도록.
			FAnimGraphManager::Get().RefreshAvailableGraphs();
			return FAnimGraphManager::Get().GetAvailableGraphFiles();
		}
		if (std::strcmp(AssetTypeName, "UParticleSystem") == 0)
		{
			FParticleSystemManager::Get().RefreshAvailableParticleSystems();
			return FParticleSystemManager::Get().GetAvailableParticleSystemFiles();
		}
		if (std::strcmp(AssetTypeName, "UPhysicalMaterial") == 0)
		{
			FPhysicalMaterialManager::Get().RefreshAvailablePhysicalMaterials();
			return FPhysicalMaterialManager::Get().GetAvailablePhysicalMaterialFiles();
		}
		if (std::strcmp(AssetTypeName, "LuaAnimScript") == 0)
		{
			// Content/Script/Anim/ 하위 .lua 파일 즉석 스캔. 콤보 열 때만 호출되므로 비용 무시.
			// startup 캐싱 필요해지면 별도 매니저로 이관.
			static TArray<FAssetListItem> Cache;
			Cache.clear();

			namespace fs = std::filesystem;
			const fs::path Dir = fs::path(FPaths::RootDir()) / L"Content" / L"Script" / L"Anim";
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

    TArray<FAssetListItem> ListSkeletons()
    {
        return FSkeletonManager::Get().GetAvailableSkeletonFiles();
    }

    bool ValidateAssetBinding(
        const FSkeletonBinding& A,
        const FSkeletonBinding& B,
        bool bAllowSameStructure,
        FSkeletonCompatibilityReport* OutReport)
    {
        const FSkeletonCompatibilityReport Report = FSkeletonManager::CheckCompatibility(A, B);
        if (OutReport)
        {
            *OutReport = Report;
        }

        if (Report.Result == ESkeletonCompatibilityResult::ExactSkeleton)
        {
            return true;
        }

        return bAllowSameStructure && Report.Result == ESkeletonCompatibilityResult::SameStructure;
    }

    TArray<FAssetListItem> ListAnimationsForSkeleton(const FSkeletonBinding& Skeleton, bool bAllowSameStructure)
    {
        TArray<FAssetListItem> Result;

        const TArray<FAssetListItem>& AnimFiles = FAnimationManager::Get().GetAvailableAnimationFiles();
        for (const FAssetListItem& Item : AnimFiles)
        {
            UAnimSequence* Sequence = FAnimationManager::Get().LoadAnimation(Item.FullPath);
            if (!Sequence)
            {
                continue;
            }

            if (ValidateAssetBinding(Sequence->GetSkeletonBinding(), Skeleton, bAllowSameStructure))
            {
                Result.push_back(Item);
            }
        }

        return Result;
    }

    TArray<FAssetListItem> ListMeshesForSkeleton(const FSkeletonBinding& Skeleton, bool bAllowSameStructure)
    {
        TArray<FAssetListItem> Result;

        const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableSkeletalMeshFiles();
        for (const FAssetListItem& Item : MeshFiles)
        {
            FSkeletonBinding MeshBinding;
            if (!FMeshManager::ReadSkeletalMeshBinding(Item.FullPath, MeshBinding))
            {
                continue;
            }

            if (ValidateAssetBinding(MeshBinding, Skeleton, bAllowSameStructure))
            {
                Result.push_back(Item);
            }
        }

        return Result;
    }

    USkeleton* FindSkeletonByGuid(const FString& SkeletonAssetGuid)
    {
        return FSkeletonManager::Get().FindSkeletonByAssetGuid(SkeletonAssetGuid);
    }

    bool CheckAnimationForMesh(const UAnimSequence* Sequence, const USkeletalMesh* Mesh, FSkeletonCompatibilityReport* OutReport)
    {
        if (!Sequence || !Mesh)
        {
            if (OutReport)
            {
                OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                OutReport->Reason = "null animation or mesh";
            }
            return false;
        }

        const FSkeletonCompatibilityReport Report = FSkeletonManager::CheckCompatibility(
            Sequence->GetSkeletonBinding(),
            Mesh->GetSkeletonBinding(),
            nullptr,
            Mesh->GetSkeleton());

        if (OutReport)
        {
            *OutReport = Report;
        }

        // Anim tracks carry bone names, and pose extraction remaps by name when indices differ.
        // Matching skeleton structure is therefore safe enough for runtime playback.
        return Report.IsCompatible(true);
    }
}
