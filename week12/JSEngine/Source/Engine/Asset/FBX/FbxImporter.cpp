#include "FbxImporter.h"
#include "FbxImporterInternal.h"

#include "Core/Logging/Log.h"
#include "Core/PlatformTime.h"

#include <fbxsdk.h>

using namespace fbxsdk;
using namespace FFbxImporterInternal;

FStaticMesh* FFbxImporter::Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
	const double StartTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Start loading FBX: %s", Path.c_str());

	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
		return nullptr;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
	if (!Scene)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene");
		Manager->Destroy();
		return nullptr;
	}

	if (!ImportScene(Path, Manager, Scene))
	{
		Manager->Destroy();
		return nullptr;
	}

	// Triangulate 후 메시 처리
	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, /*pReplace=*/true);

	FStaticMesh* StaticMesh = new FStaticMesh();
	StaticMesh->PathFileName = Path;

	if (FbxNode* RootNode = Scene->GetRootNode())
	{
		for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
		{
			CollectMeshes(RootNode->GetChild(i), StaticMesh);
		}
	}

	Manager->Destroy();

	if (StaticMesh->Vertices.empty() || StaticMesh->Indices.empty())
	{
		UE_LOG_ERROR("[FbxImporter] No geometry found in FBX: %s", Path.c_str());
		delete StaticMesh;
		return nullptr;
	}

    DeduplicateStaticMeshVertices(StaticMesh);

	if (LoadOptions.bNormalizeToUnitCube)
	{
		UE_LOG("[FbxImporter] NormalizeToUnitCube enabled: %s", Path.c_str());
		NormalizePositionsToUnitCube(StaticMesh);
	}

	StaticMesh->LocalBounds = BuildLocalBounds(StaticMesh);

	ComputeTangents(StaticMesh);

	UE_LOG("[FbxImporter] FBX Loaded: %s (Vertices: %zu, Indices: %zu, Sections: %zu, Slots: %zu)",
		Path.c_str(),
		StaticMesh->Vertices.size(),
		StaticMesh->Indices.size(),
		StaticMesh->Sections.size(),
		StaticMesh->Slots.size());

	const double EndTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Loaded %s in %.3f sec", Path.c_str(), EndTime - StartTime);

	return StaticMesh;
}

bool FFbxImporter::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("fbx") || Extension == FString(".fbx") ||
		   Extension == FString("FBX") || Extension == FString(".FBX");
}

FString FFbxImporter::GetLoaderName() const
{
	return FString{ "FFbxImporter" };
}

FSkeletalMesh* FFbxImporter::LoadSkeletalMesh(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
    (void)LoadOptions;

    const double StartTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Start loading Skeletal FBX: %s", Path.c_str());

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
        return nullptr;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ImportSkeletalScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene");
        Manager->Destroy();
        return nullptr;
    }

    if (!ImportScene(Path, Manager, Scene))
    {
        Manager->Destroy();
        return nullptr;
    }

    FbxGeometryConverter Converter(Manager);
    Converter.Triangulate(Scene, true);

    FSkeletalMesh* SkeletalMesh = new FSkeletalMesh();
    SkeletalMesh->PathFileName = Path;

    TMap<FbxNode*, int32> BoneNodeToIndex;

    bool bHasImportedSkinnedMesh = false;

    if (FbxNode* RootNode = Scene->GetRootNode())
    {
        // 1-pass: skin deformer가 있는 mesh만 먼저 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                ESkeletalMeshImportPass::SkinnedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh);
        }

        // 2-pass: skin deformer가 없는 mesh 중 bone 아래에 붙은 mesh를 rigid mesh로 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                ESkeletalMeshImportPass::RigidAttachedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh);
        }
    }

    Manager->Destroy();

    if (SkeletalMesh->Vertices.empty() || SkeletalMesh->Indices.empty() || SkeletalMesh->Bones.empty())
    {
        UE_LOG_ERROR("[FbxImporter] No skeletal geometry or bones found: %s", Path.c_str());
        delete SkeletalMesh;
        return nullptr;
    }

    DeduplicateSkeletalMeshVertices(SkeletalMesh);

    SkeletalMesh->LocalBounds = BuildLocalBounds(SkeletalMesh);
    ComputeTangents(SkeletalMesh);

    const double EndTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Skeletal FBX Loaded: %s (Vertices=%zu, Indices=%zu, Bones=%zu, Sections=%zu, Slots=%zu, %.3f sec)",
           Path.c_str(),
           SkeletalMesh->Vertices.size(),
           SkeletalMesh->Indices.size(),
           SkeletalMesh->Bones.size(),
           SkeletalMesh->Sections.size(),
           SkeletalMesh->MaterialSlots.size(),
           EndTime - StartTime);

    return SkeletalMesh;
}

FFbxMeshContentInfo FFbxImporter::InspectMeshContent(const FString& Path)
{
    FFbxMeshContentInfo Result;

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager for inspection");
        return Result;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "InspectFbxScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene for inspection");
        Manager->Destroy();
        return Result;
    }

    if (ImportScene(Path, Manager, Scene))
    {
        if (FbxNode* RootNode = Scene->GetRootNode())
        {
            InspectMeshContentRecursive(RootNode, Result);
        }
    }

    Manager->Destroy();
    return Result;
}

bool FFbxImporter::ImportScene(const FString& Path, FbxManager* Manager, FbxScene* Scene)
{
	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(Path.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_ERROR("[FbxImporter] Initialize failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
		Importer->Destroy();
		return false;
	}

	const bool bResult = Importer->Import(Scene);
	if (!bResult)
	{
		UE_LOG_ERROR("[FbxImporter] Import failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
	}

	Importer->Destroy();

	if (bResult)
	{
		// Engine import policy: left-handed, Z-up, X-forward, meter.
		// FBX SDK가 mesh/transform/anim까지 일관되게 변환해주므로 정점 단계에서 축 swap 금지.
		const FbxAxisSystem TargetAxis(
			FbxAxisSystem::eZAxis,
			FbxAxisSystem::eParityOdd,
			FbxAxisSystem::eLeftHanded);
		TargetAxis.DeepConvertScene(Scene);

		FbxSystemUnit::m.ConvertScene(Scene);
	}

	return bResult;
}
