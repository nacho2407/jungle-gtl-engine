#include "FbxMaterialLoader.h"

#include "Asset/FileUtils.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

#include <fbxsdk.h>

#include <filesystem>

using namespace fbxsdk;
namespace fs = std::filesystem;

namespace
{
    // FBX scene bootstrap. mesh import에 쓰는 ImportScene과 동일하지만 axis/unit 변환은 불필요
    // (material은 color 데이터만 다루므로 공간 변환과 무관).
    bool OpenFbxScene(const FString& Path, FbxManager*& OutManager, FbxScene*& OutScene)
    {
        OutManager = FbxManager::Create();
        if (!OutManager)
        {
            UE_LOG_ERROR("[FbxMaterialLoader] FbxManager::Create failed");
            return false;
        }

        FbxIOSettings* IOSettings = FbxIOSettings::Create(OutManager, IOSROOT);
        OutManager->SetIOSettings(IOSettings);

        OutScene = FbxScene::Create(OutManager, "FbxMaterialScene");
        if (!OutScene)
        {
            UE_LOG_ERROR("[FbxMaterialLoader] FbxScene::Create failed");
            OutManager->Destroy();
            OutManager = nullptr;
            return false;
        }

        FbxImporter* Importer = FbxImporter::Create(OutManager, "");
        if (!Importer->Initialize(Path.c_str(), -1, OutManager->GetIOSettings()))
        {
            UE_LOG_ERROR("[FbxMaterialLoader] Initialize failed: %s (%s)",
                Path.c_str(), Importer->GetStatus().GetErrorString());
            Importer->Destroy();
            OutManager->Destroy();
            OutManager = nullptr;
            OutScene = nullptr;
            return false;
        }

        const bool bImported = Importer->Import(OutScene);
        Importer->Destroy();
        if (!bImported)
        {
            UE_LOG_ERROR("[FbxMaterialLoader] Import failed: %s", Path.c_str());
            OutManager->Destroy();
            OutManager = nullptr;
            OutScene = nullptr;
            return false;
        }

        return true;
    }

    // 절대 경로를 엔진 root 기준 상대 경로로 변환.
    FString ToEngineRelativePath(const fs::path& Path)
    {
        std::error_code Ec;
        const fs::path Root(FPaths::RootDir());  // RootDir는 이미 wstring
        fs::path RelativePath = fs::relative(Path, Root, Ec);
        if (Ec || RelativePath.empty())
        {
            return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
        }
        return FPaths::ToUtf8(RelativePath.lexically_normal().generic_wstring());
    }

    // FBX 텍스처 경로 후보를 검증/변환. 존재하지 않으면 빈 문자열.
    // 시도 순서: 후보 그대로 (절대 경로) → FBX dir 기준 상대 → 파일명만으로 FBX dir 재귀 검색.
    FString TryResolveCandidate(const fs::path& FbxDir, const FString& Candidate)
    {
        if (Candidate.empty()) return {};

        // 1) 후보를 그대로 시도 (절대 경로일 수 있음). 상대 경로면 FBX dir에 붙임.
        {
            fs::path P(FPaths::ToWide(Candidate));
            if (P.is_relative())
            {
                P = FbxDir / P;
            }
            P = P.lexically_normal();
            std::error_code Ec;
            if (fs::exists(P, Ec) && fs::is_regular_file(P, Ec))
            {
                return ToEngineRelativePath(P);
            }
        }

        // 2) 파일명만 떼서 FBX dir에서 재귀 검색 (ObjMtlLoader와 동일 패턴)
        const fs::path Filename = fs::path(FPaths::ToWide(Candidate)).filename();
        if (!Filename.empty())
        {
            FString FoundPath;
            if (FFileUtils::FindFileRecursively(
                FPaths::ToUtf8(FbxDir.generic_wstring()),
                FPaths::ToUtf8(Filename.generic_wstring()),
                FoundPath))
            {
                const fs::path FoundAbsPath = (FbxDir / fs::path(FPaths::ToWide(FoundPath))).lexically_normal();
                return ToEngineRelativePath(FoundAbsPath);
            }
        }

        return {};
    }

    // FbxFileTexture 객체에서 첫 유효한 경로를 엔진 상대 경로로 반환.
    FString ResolveFbxTexturePath(const fs::path& FbxDir, FbxFileTexture* Tex)
    {
        if (!Tex) return {};

        // 우선순위 1: RelativeFileName (다른 머신으로 옮겨다닌 자산에 안정적)
        if (const char* RelName = Tex->GetRelativeFileName())
        {
            if (*RelName)
            {
                FString Resolved = TryResolveCandidate(FbxDir, FString(RelName));
                if (!Resolved.empty()) return Resolved;
            }
        }

        // 우선순위 2: 절대 FileName (저작자 머신 기준 — 경로가 안 맞을 수 있음)
        if (const char* AbsName = Tex->GetFileName())
        {
            if (*AbsName)
            {
                FString Resolved = TryResolveCandidate(FbxDir, FString(AbsName));
                if (!Resolved.empty()) return Resolved;
            }
        }

        return {};
    }

    // surface material의 특정 property에 연결된 첫 번째 FbxFileTexture를 찾는다.
    FbxFileTexture* GetFirstFileTexture(FbxSurfaceMaterial* SurfMat, const char* PropName)
    {
        if (!SurfMat || !PropName) return nullptr;
        FbxProperty Prop = SurfMat->FindProperty(PropName);
        if (!Prop.IsValid()) return nullptr;

        const int32 Count = Prop.GetSrcObjectCount<FbxFileTexture>();
        if (Count <= 0) return nullptr;
        return Prop.GetSrcObject<FbxFileTexture>(0);
    }

    // 한 property에 대해 텍스처 추출 + 경로 변환 + FMaterial 필드 채움.
    // 추출 실패 시 OutHasFlag/OutPath은 변경하지 않음 (caller가 default false/empty 유지).
    void TryExtractTexture(FbxSurfaceMaterial* SurfMat, const char* PropName,
                           const fs::path& FbxDir,
                           FString& OutPath, bool& OutHasFlag,
                           const char* LogLabel)
    {
        FbxFileTexture* Tex = GetFirstFileTexture(SurfMat, PropName);
        if (!Tex) return;

        const FString TexPath = ResolveFbxTexturePath(FbxDir, Tex);
        if (!TexPath.empty())
        {
            OutPath = TexPath;
            OutHasFlag = true;
        }
        else
        {
            UE_LOG_WARNING("[FbxMaterialLoader] %s texture not found on disk: %s / %s",
                LogLabel,
                Tex->GetRelativeFileName(),
                Tex->GetFileName());
        }
    }
}

bool FFbxMaterialLoader::Load(const FString& FbxFilePath,
                              TMap<FString, UMaterial*>& OutMaterialAssets,
                              ID3D11Device* Device,
                              TArray<FString>* OutMaterialOrder)
{
    FbxManager* Manager = nullptr;
    FbxScene* Scene = nullptr;
    if (!OpenFbxScene(FbxFilePath, Manager, Scene))
    {
        return false;
    }

    const fs::path FbxDir = fs::path(FPaths::ToWide(FbxFilePath)).parent_path();

    const int32 MaterialCount = Scene->GetSrcObjectCount<FbxSurfaceMaterial>();
    UE_LOG("[FbxMaterialLoader] %s | FbxSurfaceMaterial count = %d", FbxFilePath.c_str(), MaterialCount);

    for (int32 i = 0; i < MaterialCount; ++i)
    {
        FbxSurfaceMaterial* SurfMat = Scene->GetSrcObject<FbxSurfaceMaterial>(i);
        if (!SurfMat) continue;

        const FString MatName = FString(SurfMat->GetName());
        if (MatName.empty()) continue;

        if (OutMaterialAssets.find(MatName) != OutMaterialAssets.end())
        {
            UE_LOG_WARNING("[FbxMaterialLoader] Duplicate material name skipped: %s", MatName.c_str());
            continue;
        }

        UMaterial* Mat = UObjectManager::Get().CreateObject<UMaterial>();
        Mat->ImportedName = MatName;
        ExtractMaterialProperties(SurfMat, Mat->MaterialData);
        FMaterial& MD = Mat->MaterialData;

        // 표준 5개 텍스처 슬롯 추출
        TryExtractTexture(SurfMat, FbxSurfaceMaterial::sDiffuse,  FbxDir,
            MD.DiffuseTexPath,  MD.bHasDiffuseTexture,  "Diffuse");
        TryExtractTexture(SurfMat, FbxSurfaceMaterial::sAmbient,  FbxDir,
            MD.AmbientTexPath,  MD.bHasAmbientTexture,  "Ambient");
        TryExtractTexture(SurfMat, FbxSurfaceMaterial::sSpecular, FbxDir,
            MD.SpecularTexPath, MD.bHasSpecularTexture, "Specular");
        TryExtractTexture(SurfMat, FbxSurfaceMaterial::sEmissive, FbxDir,
            MD.EmissiveTexPath, MD.bHasEmissiveTexture, "Emissive");
        // Normal map 우선, 없으면 Bump (FMaterial은 BumpTexPath 한 슬롯에 통합)
        TryExtractTexture(SurfMat, FbxSurfaceMaterial::sNormalMap, FbxDir,
            MD.BumpTexPath, MD.bHasBumpTexture, "Normal");
        if (!MD.bHasBumpTexture)
        {
            TryExtractTexture(SurfMat, FbxSurfaceMaterial::sBump, FbxDir,
                MD.BumpTexPath, MD.bHasBumpTexture, "Bump");
        }

        OutMaterialAssets[MatName] = Mat;
        if (OutMaterialOrder)
        {
            OutMaterialOrder->push_back(MatName);
        }

        auto TexOrNone = [](bool Has, const FString& Path) -> const char*
        {
            return Has ? Path.c_str() : "(none)";
        };

        UE_LOG("[FbxMaterialLoader]   [%d] %s (type=%s) | Diffuse=(%.2f,%.2f,%.2f) Shininess=%.2f Opacity=%.2f",
            i, MatName.c_str(), SurfMat->GetClassId().GetName(),
            MD.DiffuseColor.X, MD.DiffuseColor.Y, MD.DiffuseColor.Z,
            MD.Shininess, MD.Opacity);
        UE_LOG("[FbxMaterialLoader]       Tex: Diff=%s Amb=%s Spec=%s Emi=%s Bump=%s",
            TexOrNone(MD.bHasDiffuseTexture,  MD.DiffuseTexPath),
            TexOrNone(MD.bHasAmbientTexture,  MD.AmbientTexPath),
            TexOrNone(MD.bHasSpecularTexture, MD.SpecularTexPath),
            TexOrNone(MD.bHasEmissiveTexture, MD.EmissiveTexPath),
            TexOrNone(MD.bHasBumpTexture,     MD.BumpTexPath));
    }

    // ObjMtlLoader와 동일하게 MaterialParams에 셰이더 바인딩 정보 채움.
    // 이걸 안 하면 UMaterial이 cache에 등록돼도 셰이더는 default 값을 사용해 까맣게 나옴.
    UTexture* DefaultWhite = FResourceManager::Get().GetTexture("DefaultWhite");

    auto MapOrDefault = [&](bool HasTex, const FString& Path) -> FMaterialParamValue
    {
        return HasTex
            ? FMaterialParamValue(FResourceManager::Get().LoadTexture(Path, Device))
            : FMaterialParamValue(DefaultWhite);
    };

    for (auto& [Name, Mat] : OutMaterialAssets)
    {
        if (!Mat) continue;
        const FMaterial& MD = Mat->MaterialData;

        Mat->MaterialParams["AmbientColor"]  = FMaterialParamValue(MD.AmbientColor);
        Mat->MaterialParams["DiffuseColor"]  = FMaterialParamValue(MD.DiffuseColor);
        Mat->MaterialParams["SpecularColor"] = FMaterialParamValue(MD.SpecularColor);
        Mat->MaterialParams["EmissiveColor"] = FMaterialParamValue(MD.EmissiveColor);
        Mat->MaterialParams["Shininess"]     = FMaterialParamValue(MD.Shininess);
        Mat->MaterialParams["Opacity"]       = FMaterialParamValue(MD.Opacity);

        Mat->MaterialParams["DiffuseMap"]  = MapOrDefault(MD.bHasDiffuseTexture,  MD.DiffuseTexPath);
        Mat->MaterialParams["AmbientMap"]  = MapOrDefault(MD.bHasAmbientTexture,  MD.AmbientTexPath);
        Mat->MaterialParams["SpecularMap"] = MapOrDefault(MD.bHasSpecularTexture, MD.SpecularTexPath);
        Mat->MaterialParams["EmissiveMap"] = MapOrDefault(MD.bHasEmissiveTexture, MD.EmissiveTexPath);
        Mat->MaterialParams["BumpMap"]     = MapOrDefault(MD.bHasBumpTexture,     MD.BumpTexPath);

        Mat->MaterialParams["bHasDiffuseMap"]  = FMaterialParamValue(MD.bHasDiffuseTexture);
        Mat->MaterialParams["bHasAmbientMap"]  = FMaterialParamValue(MD.bHasAmbientTexture);
        Mat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(MD.bHasSpecularTexture);
        Mat->MaterialParams["bHasEmissiveMap"] = FMaterialParamValue(MD.bHasEmissiveTexture);
        Mat->MaterialParams["bHasBumpMap"]     = FMaterialParamValue(MD.bHasBumpTexture);

        Mat->MaterialParams["ScrollUV"] = FMaterialParamValue(FVector2(0.0f, 0.0f));
    }

    Manager->Destroy();
    return true;
}

void FFbxMaterialLoader::ExtractMaterialProperties(FbxSurfaceMaterial* SurfMat, FMaterial& OutData)
{
    if (!SurfMat) return;

    auto ToFVec = [](const FbxDouble3& V, double Factor) -> FVector
    {
        return FVector(
            static_cast<float>(V[0] * Factor),
            static_cast<float>(V[1] * Factor),
            static_cast<float>(V[2] * Factor));
    };

    // Phong → 모든 색 + Specular + Shininess
    if (FbxSurfacePhong* Phong = FbxCast<FbxSurfacePhong>(SurfMat))
    {
        OutData.AmbientColor  = ToFVec(Phong->Ambient.Get(),  Phong->AmbientFactor.Get());
        OutData.DiffuseColor  = ToFVec(Phong->Diffuse.Get(),  Phong->DiffuseFactor.Get());
        OutData.SpecularColor = ToFVec(Phong->Specular.Get(), Phong->SpecularFactor.Get());
        OutData.EmissiveColor = ToFVec(Phong->Emissive.Get(), Phong->EmissiveFactor.Get());
        OutData.Shininess     = static_cast<float>(Phong->Shininess.Get());
        OutData.Opacity       = static_cast<float>(1.0 - Phong->TransparencyFactor.Get());
    }
    // Lambert → Specular/Shininess는 FMaterial 기본값 유지
    else if (FbxSurfaceLambert* Lambert = FbxCast<FbxSurfaceLambert>(SurfMat))
    {
        OutData.AmbientColor  = ToFVec(Lambert->Ambient.Get(),  Lambert->AmbientFactor.Get());
        OutData.DiffuseColor  = ToFVec(Lambert->Diffuse.Get(),  Lambert->DiffuseFactor.Get());
        OutData.EmissiveColor = ToFVec(Lambert->Emissive.Get(), Lambert->EmissiveFactor.Get());
        OutData.Opacity       = static_cast<float>(1.0 - Lambert->TransparencyFactor.Get());
    }
    // 기타 surface material → property API로 Diffuse만이라도
    else
    {
        FbxProperty DiffuseProp = SurfMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (DiffuseProp.IsValid())
        {
            const FbxDouble3 D = DiffuseProp.Get<FbxDouble3>();
            OutData.DiffuseColor = FVector(
                static_cast<float>(D[0]),
                static_cast<float>(D[1]),
                static_cast<float>(D[2]));
        }
    }

    // Opacity 안전 clamp
    if (OutData.Opacity < 0.0f) OutData.Opacity = 0.0f;
    if (OutData.Opacity > 1.0f) OutData.Opacity = 1.0f;
}
