#include "Core/FbxMaterialLoadService.h"

#include "Core/ImportedMaterialPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Resource/FbxMaterialLoader.h"
#include "Render/Resource/Shader.h"

#include <filesystem>

namespace fs = std::filesystem;

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

    const fs::path AutoMaterialDir = fs::path(L"Asset") / L"Material" / L"Auto";

    for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialOrder.size()); ++MaterialIndex)
    {
        const FString& Name = MaterialOrder[MaterialIndex];
        auto ParsedIt = Parsed.find(Name);
        if (ParsedIt == Parsed.end()) continue;

        UMaterial* Mat = ParsedIt->second;
        if (!Mat) continue;

        // .mat asset 자산 키 (디스크 파일 생성은 안 함 — in-memory key only)
        const FString MaterialName = FImportedMaterialPolicy::MakeImportedMaterialAssetName(NormalizedFbxPath, MaterialIndex);
        const fs::path RelativeMatPath = AutoMaterialDir / FPaths::ToWide(MaterialName + ".mat");
        const FString MaterialAssetPath = FPaths::Normalize(FPaths::ToUtf8(RelativeMatPath.generic_wstring()));
        const FString MaterialKey = MaterialAssetPath;

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

        UE_LOG("[FbxMaterialLoadService] Registered: %s → %s", Name.c_str(), MaterialKey.c_str());

        // 텍스처 로드는 B2 이후에 추가. B1엔 bHasXxxTexture가 모두 false라 skip.
    }

    UE_LOG("[FbxMaterialLoadService] Loaded %zu materials from %s",
        MaterialOrder.size(), NormalizedFbxPath.c_str());

    return true;
}
