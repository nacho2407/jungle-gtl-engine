#pragma once

#include "Core/CoreMinimal.h"
#include "Render/Resource/Material.h"

namespace fbxsdk { class FbxSurfaceMaterial; }

/**
 * @brief FBX 파일의 내장 surface material을 파싱하여 UMaterial로 변환.
 * ObjMtlLoader의 FBX 버전. 색 속성만 추출 (텍스처는 후속 phase).
 */
class FFbxMaterialLoader
{
public:
    /**
     * @brief FBX scene을 열어 FbxSurfaceMaterial들을 UMaterial로 변환.
     * @param FbxFilePath     FBX 파일 경로
     * @param OutMaterialAssets  파싱된 (이름 → UMaterial*) 맵
     * @param Device          텍스처 생성용 (B1엔 미사용)
     * @param OutMaterialOrder 등록 순서 보존 (선택)
     * @return scene 열기 성공 여부
     */
    static bool Load(const FString& FbxFilePath,
                     TMap<FString, UMaterial*>& OutMaterialAssets,
                     ID3D11Device* Device,
                     TArray<FString>* OutMaterialOrder = nullptr);

private:
    static void ExtractMaterialProperties(fbxsdk::FbxSurfaceMaterial* SurfMat, FMaterial& OutData);
};
