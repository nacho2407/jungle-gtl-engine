#include "SpriteComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/Texture2DAsset.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "SceneIO/SceneAssetPath.h"

namespace Engine::Component
{
    void USpriteComponent::SetTextureResource(FTextureResource* InTextureResource)
    {
        TextureResource = InTextureResource;
    }

    void USpriteComponent::SetTexturePath(const FString& InPath)
    {
        TexturePath = InPath;
        TextureResource = nullptr;
    }

    void USpriteComponent::SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }

    void USpriteComponent::SetBillboardOffset(const FVector& InBillboardOffset)
    {
        BillboardOffset = InBillboardOffset;
    }

    bool USpriteComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        return UQuadComponent::GetLocalTriangles(OutTriangles);
    }

    Geometry::FAABB USpriteComponent::GetLocalAABB() const
    {
        return UQuadComponent::GetLocalAABB();
    }

    void USpriteComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UPrimitiveComponent::DescribeProperties(Builder); // 부모 호출 복구

        FComponentPropertyOptions TexturePathOptions;
        TexturePathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::TextureImage;

        Builder.AddAssetPath(
            "texture_path", L"Texture Path", [this]() { return GetTexturePath(); },
            [this](const FString& InValue) { SetTexturePath(InValue); }, TexturePathOptions);

        Builder.AddBool(
            "billboard", L"Billboard", [this]() { return GetBillboard(); },
            [this](bool bInValue) { SetBillboard(bInValue); });
            
        Builder.AddVector3(
            "billboard_offset", L"Billboard Offset", [this]() { return GetBillboardOffset(); },
            [this](const FVector& InValue) { SetBillboardOffset(InValue); });
    }

    void USpriteComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        TextureResource = nullptr;

        if (InAssetManager == nullptr || TexturePath.empty())
        {
            return;
        }

        const std::filesystem::path AbsolutePath =
            Engine::SceneIO::ResolveSceneAssetPathToAbsolute(TexturePath);
        if (AbsolutePath.empty())
        {
            return;
        }

        FAssetLoadParams LoadParams;
        LoadParams.ExplicitType = EAssetType::Texture;

        UAsset*          LoadedAsset = InAssetManager->Load(AbsolutePath.native(), LoadParams);
        UTexture2DAsset* TextureAsset = Cast<UTexture2DAsset>(LoadedAsset);
        if (TextureAsset != nullptr)
        {
            SetTextureResource(TextureAsset->GetResource());
        }
    }

    REGISTER_CLASS(Engine::Component, USpriteComponent)
} // namespace Engine::Component
