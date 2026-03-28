#include "Core/CoreMinimal.h"
#include "MaterialAsset.h"

REGISTER_CLASS(, UMaterialAsset)

void UMaterialAsset::Initialize(const FSourceRecord&               InSource,
                                std::shared_ptr<FMaterialResource> InResource)
{
    InitializeAssetMetadata(InSource);
    Resource = std::move(InResource);
}

//void UMaterialAsset::SetDiffuseTexture(UTexture2DAsset* NewTexture)
//{
//    // 1. 에셋이 동일하다면 무시
//    if (DiffuseTextureAsset == NewTexture)
//    {
//        return;
//    }
//
//    // 2. 기존 에셋 참조 해제 및 새 에셋 참조 저장 (이 시점에서 기존 에셋은 참조 카운트가 깎임)
//    DiffuseTextureAsset = NewTexture;
//
//    // 3. 실제 렌더러가 소비할 GPU 데이터(FMaterialData) 갱신
//    // (Materials 맵에서 특정 재질 데이터를 가져온다고 가정. 단일 재질이라면 구조에 맞게 수정)
//    if (Resource && !Resource->Materials.empty())
//    {
//        // 예시로 첫 번째 서브 재질의 텍스처를 교체
//        auto& MatData = Resource->Materials.begin()->second;
//
//        if (NewTexture)
//        {
//            MatData.DiffuseTexture = NewTexture->GetResource();
//        }
//        else
//        {
//            MatData.DiffuseTexture = nullptr; // 텍스처를 아예 제거하는 경우
//        }
//    }
//}

TArray<UTexture2DAsset*> UMaterialAsset::GetReferencedTextures() const
{
    TArray<UTexture2DAsset*> Refs;
    if (DiffuseTextureAsset)
        Refs.push_back(DiffuseTextureAsset);
    if (AmbientTextureAsset)
        Refs.push_back(AmbientTextureAsset);
    if (SpecularTextureAsset)
        Refs.push_back(SpecularTextureAsset);
    if (NormalTextureAsset)
        Refs.push_back(NormalTextureAsset);
    return Refs;
}
