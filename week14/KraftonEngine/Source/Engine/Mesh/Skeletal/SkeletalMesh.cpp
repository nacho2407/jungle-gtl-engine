#include "SkeletalMesh.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Animation/Skeleton/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetManager.h"

#include <filesystem>

namespace
{
	bool IsValidPhysicsAssetPath(const FString& Path)
	{
		return !Path.empty() && Path != "None";
	}

	FString BuildDefaultPhysicsAssetPath(const FString& SkeletalMeshPath)
	{
		if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
		{
			return "None";
		}

		const FString RelativeMeshPath = FPaths::MakeProjectRelative(SkeletalMeshPath);
		const std::filesystem::path MeshPath(FPaths::ToWide(RelativeMeshPath));
		if (MeshPath.extension().empty())
		{
			return "None";
		}

		const std::wstring CandidateFileName =
			MeshPath.stem().wstring() + L"_PhysicsAsset" + MeshPath.extension().wstring();
		const std::filesystem::path CandidatePath = MeshPath.parent_path() / CandidateFileName;
		return FPaths::ToUtf8(CandidatePath.generic_wstring());
	}
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() && !SkeletalMeshAsset)
	{
		SkeletalMeshAsset = new FSkeletalMesh();
	}

    if (Ar.IsSaving())
    {
        SyncSkeletonBindingToAsset();
    }

	Ar << SkeletalMeshAsset->PathFileName;
	Ar << SkeletalMeshAsset->SkeletonPath;
    Ar << SkeletalMeshAsset->SkeletonAssetGuid;
    Ar << SkeletalMeshAsset->SkeletonCompatibilitySignature;
	Ar << SkeletalMeshAsset->Vertices;
	Ar << SkeletalMeshAsset->Indices;
	Ar << SkeletalMeshAsset->Sections;
	Ar << SkeletalMeshAsset->MeshRanges;
	Ar << SkeletalMeshAsset->Bones;
	Ar << SkeletalMaterials;
	Ar << SkeletalMeshAsset->MorphTargets;

	// ── 트레일링 버전 섹션 (구버전 .uasset 호환) ──
	// 구버전 파일은 여기서 끝(EOF)이라 AtEnd()가 true → 읽지 않고 기본값 유지.
	// 앞으로 포맷 확장은 ExtVersion 하나로 관리한다.
	if (Ar.IsSaving())
	{
		uint32 ExtVersion = 1;
		Ar << ExtVersion;
		Ar << PhysicsAssetPath;
	}
	else
	{
		if (!Ar.AtEnd())
		{
			uint32 ExtVersion = 0;
			Ar << ExtVersion;
			if (ExtVersion >= 1)
			{
				Ar << PhysicsAssetPath;
			}
		}
	}

	if (Ar.IsLoading())
	{
		SkeletalMeshAsset->NormalizeBonePoseData();
        SyncSkeletonBindingFromAsset();
		CacheSectionMaterialIndices();
		SkeletalMeshAsset->bBoundsValid = false;
	}
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	SkeletalMeshAsset = InMesh;
	if (SkeletalMeshAsset)
	{
		SkeletalMeshAsset->NormalizeBonePoseData();
	}
    SyncSkeletonBindingFromAsset();
	CacheSectionMaterialIndices();
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
	return SkeletalMeshAsset;
}

void USkeletalMesh::SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials)
{
	SkeletalMaterials = InMaterials;
	CacheSectionMaterialIndices();
}

const TArray<FSkeletalMaterial>& USkeletalMesh::GetSkeletalMaterials() const
{
	return SkeletalMaterials;
}

void USkeletalMesh::InitResources(ID3D11Device* InDevice)
{
	if (!InDevice || !SkeletalMeshAsset) return;

	const uint32 CPUSize =
		static_cast<uint32>(SkeletalMeshAsset->Vertices.size() * sizeof(FVertexPNCTBW)) +
		static_cast<uint32>(SkeletalMeshAsset->Indices.size() * sizeof(uint32));
	MemoryStats::AddSkeletalMeshCPUMemory(CPUSize);

	TMeshData<FVertexPNCTBW> RenderMeshData;
	RenderMeshData.Vertices.reserve(SkeletalMeshAsset->Vertices.size());

	for (const FVertexPNCTBW& RawVert : SkeletalMeshAsset->Vertices)
	{
		FVertexPNCTBW RenderVert;
		RenderVert.Position = RawVert.Position;
		RenderVert.Normal = RawVert.Normal;
		RenderVert.Color = RawVert.Color;
		RenderVert.UV = RawVert.UV;
		RenderVert.Tangent = RawVert.Tangent;
		std::copy(std::begin(RawVert.BoneIndices), std::end(RawVert.BoneIndices), std::begin(RenderVert.BoneIndices));
		std::copy(std::begin(RawVert.BoneWeights), std::end(RawVert.BoneWeights), std::begin(RenderVert.BoneWeights));
		RenderMeshData.Vertices.push_back(RenderVert);
	}
	RenderMeshData.Indices = SkeletalMeshAsset->Indices;

	SkeletalMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
	SkeletalMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
}

void USkeletalMesh::SetSkeleton(USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;

	if (Skeleton)
	{
        SetSkeletonBinding(Skeleton->GetSkeletonBinding());
	}
	else
	{
        FSkeletonBinding EmptyBinding;
        SetSkeletonBinding(EmptyBinding);
	}
}

USkeleton* USkeletalMesh::GetSkeleton() const
{
	return Skeleton;
}

UPhysicsAsset* USkeletalMesh::GetPhysicsAsset()
{
	if (CachedPhysicsAsset)
	{
		return CachedPhysicsAsset;
	}
	if (IsValidPhysicsAssetPath(PhysicsAssetPath))
	{
		CachedPhysicsAsset = FPhysicsAssetManager::Get().Load(PhysicsAssetPath);
		if (CachedPhysicsAsset)
		{
			return CachedPhysicsAsset;
		}
	}

	// 매니저가 캐시/소유하므로 GC는 매니저 쪽에서 보호된다.
	const FString DefaultPhysicsAssetPath = BuildDefaultPhysicsAssetPath(AssetPathFileName);
	if (!IsValidPhysicsAssetPath(DefaultPhysicsAssetPath) || DefaultPhysicsAssetPath == PhysicsAssetPath)
	{
		return nullptr;
	}

	CachedPhysicsAsset = FPhysicsAssetManager::Get().Load(DefaultPhysicsAssetPath);
	if (CachedPhysicsAsset)
	{
		PhysicsAssetPath = DefaultPhysicsAssetPath;
		UE_LOG(
			"SkeletalMesh resolved default PhysicsAsset. Mesh=%s PhysicsAsset=%s",
			AssetPathFileName.c_str(),
			PhysicsAssetPath.c_str()
		);
	}

	return CachedPhysicsAsset;
}

void USkeletalMesh::SetSkeletonBinding(const FSkeletonBinding& InBinding)
{
    SkeletonBinding = InBinding;
    if (SkeletonBinding.SkeletonPath.empty())
    {
        SkeletonBinding.SkeletonPath = "None";
    }
    SyncSkeletonBindingToAsset();
}

void USkeletalMesh::SyncSkeletonBindingToAsset()
{
    if (!SkeletalMeshAsset)
    {
        return;
    }

    SkeletalMeshAsset->SkeletonPath = SkeletonBinding.SkeletonPath.empty() ? FString("None") : SkeletonBinding.SkeletonPath;
    SkeletalMeshAsset->SkeletonAssetGuid = SkeletonBinding.SkeletonAssetGuid;
    SkeletalMeshAsset->SkeletonCompatibilitySignature = SkeletonBinding.CompatibilitySignature;
}

void USkeletalMesh::SyncSkeletonBindingFromAsset()
{
    if (!SkeletalMeshAsset)
    {
        return;
    }

    SkeletonBinding.SkeletonPath = SkeletalMeshAsset->SkeletonPath.empty() ? FString("None") : SkeletalMeshAsset->SkeletonPath;
    SkeletonBinding.SkeletonAssetGuid = SkeletalMeshAsset->SkeletonAssetGuid;
    SkeletonBinding.CompatibilitySignature = SkeletalMeshAsset->SkeletonCompatibilitySignature;
}

void USkeletalMesh::CacheSectionMaterialIndices()
{
	if (!SkeletalMeshAsset)
	{
		return;
	}

	for (FSkeletalMeshSection& Section : SkeletalMeshAsset->Sections)
	{
		Section.MaterialIndex = -1;
		for (int32 i = 0; i < static_cast<int32>(SkeletalMaterials.size()); ++i)
		{
			if (SkeletalMaterials[i].MaterialSlotName == Section.MaterialSlotName)
			{
				Section.MaterialIndex = i;
				break;
			}
		}
	}
}
