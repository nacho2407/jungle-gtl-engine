#include "FbxMaterialLoader.h"

#include "Core/Logging/Log.h"
#include "Object/ObjectFactory.h"

#include <fbxsdk.h>

using namespace fbxsdk;

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
}

bool FFbxMaterialLoader::Load(const FString& FbxFilePath,
                              TMap<FString, UMaterial*>& OutMaterialAssets,
                              ID3D11Device* /*Device*/,
                              TArray<FString>* OutMaterialOrder)
{
    FbxManager* Manager = nullptr;
    FbxScene* Scene = nullptr;
    if (!OpenFbxScene(FbxFilePath, Manager, Scene))
    {
        return false;
    }

    const int32 MaterialCount = Scene->GetSrcObjectCount<FbxSurfaceMaterial>();
    UE_LOG("[FbxMaterialLoader] %s | FbxSurfaceMaterial count = %d", FbxFilePath.c_str(), MaterialCount);

    for (int32 i = 0; i < MaterialCount; ++i)
    {
        FbxSurfaceMaterial* SurfMat = Scene->GetSrcObject<FbxSurfaceMaterial>(i);
        if (!SurfMat) continue;

        const FString MatName = FString(SurfMat->GetName());
        if (MatName.empty()) continue;

        // 중복 방지 (동일 이름의 surface material이 있을 가능성).
        if (OutMaterialAssets.find(MatName) != OutMaterialAssets.end())
        {
            UE_LOG_WARNING("[FbxMaterialLoader] Duplicate material name skipped: %s", MatName.c_str());
            continue;
        }

        UMaterial* Mat = UObjectManager::Get().CreateObject<UMaterial>();
        Mat->ImportedName = MatName;
        ExtractMaterialProperties(SurfMat, Mat->MaterialData);

        OutMaterialAssets[MatName] = Mat;
        if (OutMaterialOrder)
        {
            OutMaterialOrder->push_back(MatName);
        }

        const FMaterial& M = Mat->MaterialData;
        UE_LOG("[FbxMaterialLoader]   [%d] %s (type=%s) | Diffuse=(%.2f,%.2f,%.2f) Shininess=%.2f Opacity=%.2f",
            i, MatName.c_str(), SurfMat->GetClassId().GetName(),
            M.DiffuseColor.X, M.DiffuseColor.Y, M.DiffuseColor.Z,
            M.Shininess, M.Opacity);
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
