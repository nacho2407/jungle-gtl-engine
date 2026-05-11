#include "Core/FbxMaterialLoadService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/ImportedMaterialPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Resource/FbxMaterialLoader.h"
#include "Render/Resource/Shader.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    // 인덱스 i에 대한 .mat asset 경로 생성. 등록 / 디스크 저장 / 디스크 로드 모두 동일 키 사용.
    FString MakeFbxMaterialAssetPath(const FString& NormalizedFbxPath, int32 Index)
    {
        const fs::path AutoMaterialDir = fs::path(L"Asset") / L"Material" / L"Auto";
        const FString MatName = FImportedMaterialPolicy::MakeImportedMaterialAssetName(NormalizedFbxPath, Index);
        const fs::path RelativeMatPath = AutoMaterialDir / FPaths::ToWide(MatName + ".mat");
        return FPaths::Normalize(FPaths::ToUtf8(RelativeMatPath.generic_wstring()));
    }
}

FFbxMaterialLoadService::FFbxMaterialLoadService(FResourceManager& InResourceManager)
    : ResourceManager(InResourceManager)
{
}

bool FFbxMaterialLoadService::Load(const FString& FbxFilePath, const FString& ShaderName, ID3D11Device* Device)
{
    const FString NormalizedFbxPath = FPaths::Normalize(FbxFilePath);
    if (NormalizedFbxPath.empty())
    {
        return false;
    }

    // Cache hit early return (in-memory): 같은 FBX의 첫 material key가 이미 캐시에 있으면 즉시 반환.
    const FString FirstMaterialKey = MakeFbxMaterialAssetPath(NormalizedFbxPath, 0);
    if (ResourceManager.MaterialCache.ContainsMaterialKey(FirstMaterialKey))
    {
        UE_LOG("[FbxMaterialLoadService] Skipped (already cached): %s", NormalizedFbxPath.c_str());
        return true;
    }

    // Disk cache fallback: 이전 import에서 .mat을 디스크에 저장해두었으면 그것부터 로드.
    // FBX scene 파싱 비용(~4초)을 회피하고 엔진 재시작 후에도 material 상태 유지.
    if (FAssetPathPolicy::FileExists(FirstMaterialKey))
    {
        int32 LoadedCount = 0;
        for (int32 Index = 0; ; ++Index)
        {
            const FString MatAssetPath = MakeFbxMaterialAssetPath(NormalizedFbxPath, Index);
            if (!FAssetPathPolicy::FileExists(MatAssetPath))
            {
                break;
            }
            if (!ResourceManager.DeserializeMaterial(MatAssetPath))
            {
                UE_LOG_WARNING("[FbxMaterialLoadService] Failed to deserialize cached material: %s", MatAssetPath.c_str());
                continue;
            }
            // Slot alias 복원: ImportedName(=원본 FBX material name) → MaterialKey.
            // 디스크 .mat에 ImportedName 필드가 저장돼있어서 가능.
            if (UMaterial* Mat = ResourceManager.GetMaterial(MatAssetPath))
            {
                if (!Mat->ImportedName.empty())
                {
                    ResourceManager.MaterialCache.SetMaterialSlotAlias(
                        FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedFbxPath, Mat->ImportedName),
                        MatAssetPath);
                }
            }
            ++LoadedCount;
        }
        if (LoadedCount > 0)
        {
            UE_LOG("[FbxMaterialLoadService] Loaded %d materials from disk cache: %s", LoadedCount, NormalizedFbxPath.c_str());
            return true;
        }
    }

    UShader* Shader = ResourceManager.GetShader(ShaderName);
    if (!Shader)
    {
        UE_LOG_WARNING("[FbxMaterialLoadService] Shader not found: %s", ShaderName.c_str());
        return false;
    }

    TMap<FString, UMaterial*> Parsed;
    TArray<FString> MaterialOrder;
    if (!FFbxMaterialLoader::Load(NormalizedFbxPath, Parsed, Device, &MaterialOrder))
    {
        UE_LOG_WARNING("[FbxMaterialLoadService] FbxMaterialLoader failed: %s", NormalizedFbxPath.c_str());
        return false;
    }

    if (Parsed.empty())
    {
        // FBX에 surface material 0개여도 호출 자체는 성공 (resolve 단계가 DefaultWhite fallback).
        UE_LOG("[FbxMaterialLoadService] No materials in FBX: %s", NormalizedFbxPath.c_str());
        return true;
    }

    // .mat 디스크 저장을 위해 Asset/Material/Auto/ 디렉토리 보장.
    std::error_code Ec;
    fs::create_directories(fs::path(L"Asset") / L"Material" / L"Auto", Ec);

    for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialOrder.size()); ++MaterialIndex)
    {
        const FString& Name = MaterialOrder[MaterialIndex];
        auto ParsedIt = Parsed.find(Name);
        if (ParsedIt == Parsed.end()) continue;

        UMaterial* Mat = ParsedIt->second;
        if (!Mat) continue;

        const FString MaterialAssetPath = MakeFbxMaterialAssetPath(NormalizedFbxPath, MaterialIndex);
        const FString MaterialKey = MaterialAssetPath;
        const FString MaterialName = FImportedMaterialPolicy::MakeImportedMaterialAssetName(NormalizedFbxPath, MaterialIndex);

        Mat->Name = MaterialName;
        if (Mat->ImportedName.empty()) Mat->ImportedName = Name;
        Mat->FilePath = MaterialAssetPath;
        Mat->SetShader(Shader);

        // 중복 등록 가드: 이미 같은 key가 있다면 재사용하고 새 객체는 폐기.
        UMaterial* ExistingMaterial = ResourceManager.MaterialCache.FindMaterialByKey(MaterialKey);
        if (ExistingMaterial)
        {
            if (ExistingMaterial != Mat)
            {
                UObjectManager::Get().DestroyObject(Mat);
                Mat = ExistingMaterial;
            }
        }
        else
        {
            ResourceManager.MaterialCache.RegisterMaterial(MaterialKey, Mat);
        }

        // 보조 키 등록 (이름 기반 lookup 지원)
        if (!ResourceManager.MaterialCache.ContainsMaterialKey(Mat->Name))
        {
            ResourceManager.MaterialCache.RegisterMaterial(Mat->Name, Mat);
        }
        if (!ResourceManager.MaterialCache.ContainsMaterialKey(Name))
        {
            ResourceManager.MaterialCache.RegisterMaterial(Name, Mat);
        }

        // Slot alias: (fbxPath, FbxName) → MaterialKey
        // → ResolveStaticMeshMaterialSlots가 이 alias로 진짜 UMaterial을 찾음
        ResourceManager.MaterialCache.SetMaterialSlotAlias(
            FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedFbxPath, Name),
            MaterialKey);

        // 디스크 저장 — 다음 import 시 disk cache fallback이 FBX 재파싱 회피하도록.
        if (!ResourceManager.SerializeMaterial(MaterialAssetPath, Mat))
        {
            UE_LOG_WARNING("[FbxMaterialLoadService] Failed to serialize material to disk: %s", MaterialAssetPath.c_str());
        }

        UE_LOG("[FbxMaterialLoadService] Registered: %s → %s", Name.c_str(), MaterialKey.c_str());
    }

    UE_LOG("[FbxMaterialLoadService] Loaded %zu materials from %s",
        MaterialOrder.size(), NormalizedFbxPath.c_str());

    return true;
}
