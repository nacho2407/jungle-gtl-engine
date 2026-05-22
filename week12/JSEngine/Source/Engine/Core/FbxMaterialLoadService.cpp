#include "Core/FbxMaterialLoadService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/ImportedMaterialPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/FbxMaterialLoader.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    // ?몃뜳??i?????.mat asset 寃쎈줈 ?앹꽦. ?깅줉 / ?붿뒪?????/ ?붿뒪??濡쒕뱶 紐⑤몢 ?숈씪 ???ъ슜.
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

bool FFbxMaterialLoadService::Load(const FString& FbxFilePath, EMaterialShaderType ShaderType, ID3D11Device* Device)
{
    (void)ShaderType;
    (void)Device;

    const FString NormalizedFbxPath = FPaths::Normalize(FbxFilePath);
    if (NormalizedFbxPath.empty())
    {
        return false;
    }

    // Cache hit early return (in-memory): 媛숈? FBX??泥?material key媛 ?대? 罹먯떆???덉쑝硫?利됱떆 諛섑솚.
    const FString FirstMaterialKey = MakeFbxMaterialAssetPath(NormalizedFbxPath, 0);
    if (ResourceManager.MaterialCache.ContainsMaterialKey(FirstMaterialKey))
    {
        UE_LOG("[FbxMaterialLoadService] MaterialSource=MemoryCache | Path=%s", NormalizedFbxPath.c_str());
        return true;
    }

    // Disk cache fallback: ?댁쟾 import?먯꽌 .mat???붿뒪?ъ뿉 ??ν빐?먯뿀?쇰㈃ 洹멸쾬遺??濡쒕뱶.
    // FBX scene ?뚯떛 鍮꾩슜(~4珥????뚰뵾?섍퀬 ?붿쭊 ?ъ떆???꾩뿉??material ?곹깭 ?좎?.
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
            // Slot alias 蹂듭썝: ImportedName(=?먮낯 FBX material name) ??MaterialKey.
            // ?붿뒪??.mat??ImportedName ?꾨뱶媛 ??λ뤌?덉뼱??媛??
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
            UE_LOG("[FbxMaterialLoadService] MaterialSource=DiskAsset | Count=%d | Path=%s", LoadedCount, NormalizedFbxPath.c_str());
            return true;
        }

        UE_LOG_WARNING("[FbxMaterialLoadService] MaterialSource=CorruptAsset | Path=%s", NormalizedFbxPath.c_str());
        return false;
    }

    UE_LOG_WARNING("[FbxMaterialLoadService] MaterialSource=MissingAsset | Path=%s", NormalizedFbxPath.c_str());
    return false;
}

bool FFbxMaterialLoadService::ImportFromFbx(const FString& FbxFilePath, EMaterialShaderType ShaderType, ID3D11Device* Device)
{
    const FString NormalizedFbxPath = FPaths::Normalize(FbxFilePath);
    if (NormalizedFbxPath.empty())
    {
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
        // FBX??surface material 0媛쒖뿬???몄텧 ?먯껜???깃났 (resolve ?④퀎媛 DefaultWhite fallback).
        UE_LOG("[FbxMaterialLoadService] No materials in FBX: %s", NormalizedFbxPath.c_str());
        UE_LOG("[FbxMaterialLoadService] MaterialSource=ExplicitFbxImport | Count=0 | Path=%s",
            NormalizedFbxPath.c_str());
        return true;
    }

    // .mat ?붿뒪????μ쓣 ?꾪빐 Asset/Material/Auto/ ?붾젆?좊━ 蹂댁옣.
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
        Mat->SetShaderType(ShaderType);

        // 以묐났 ?깅줉 媛?? ?대? 媛숈? key媛 ?덈떎硫??ъ궗?⑺븯怨???媛앹껜???먭린.
        UMaterial* ExistingMaterial = ResourceManager.MaterialCache.FindMaterialByKey(MaterialKey);
        if (ExistingMaterial)
        {
            if (ExistingMaterial != Mat)
            {
                ExistingMaterial->Name = Mat->Name;
                ExistingMaterial->ImportedName = Mat->ImportedName;
                ExistingMaterial->FilePath = Mat->FilePath;
                ExistingMaterial->MaterialData = Mat->MaterialData;
                ExistingMaterial->SetShaderType(ShaderType);
                UObjectManager::Get().DestroyObject(Mat);
                Mat = ExistingMaterial;
            }
        }
        else
        {
            ResourceManager.MaterialCache.RegisterMaterial(MaterialKey, Mat);
        }

        // 蹂댁“ ???깅줉 (?대쫫 湲곕컲 lookup 吏??
        if (!ResourceManager.MaterialCache.ContainsMaterialKey(Mat->Name))
        {
            ResourceManager.MaterialCache.RegisterMaterial(Mat->Name, Mat);
        }
        if (!ResourceManager.MaterialCache.ContainsMaterialKey(Name))
        {
            ResourceManager.MaterialCache.RegisterMaterial(Name, Mat);
        }

        // Slot alias: (fbxPath, FbxName) ??MaterialKey
        // ??ResolveStaticMeshMaterialSlots媛 ??alias濡?吏꾩쭨 UMaterial??李얠쓬
        ResourceManager.MaterialCache.SetMaterialSlotAlias(
            FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedFbxPath, Name),
            MaterialKey);

        // ?붿뒪????????ㅼ쓬 import ??disk cache fallback??FBX ?ы뙆???뚰뵾?섎룄濡?
        if (!ResourceManager.SerializeMaterial(MaterialAssetPath, Mat))
        {
            UE_LOG_WARNING("[FbxMaterialLoadService] Failed to serialize material to disk: %s", MaterialAssetPath.c_str());
        }

        UE_LOG("[FbxMaterialLoadService] Registered: %s ??%s", Name.c_str(), MaterialKey.c_str());
    }

    UE_LOG("[FbxMaterialLoadService] Loaded %zu materials from %s",
        MaterialOrder.size(), NormalizedFbxPath.c_str());
    UE_LOG("[FbxMaterialLoadService] MaterialSource=ExplicitFbxImport | Count=%zu | Path=%s",
        MaterialOrder.size(), NormalizedFbxPath.c_str());

    return true;
}
