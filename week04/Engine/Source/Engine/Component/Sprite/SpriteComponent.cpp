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

    void USpriteComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UQuadComponent::DescribeProperties(Builder);

        FComponentPropertyOptions TexturePathOptions;
        TexturePathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::TextureImage;

        Builder.AddAssetPath(
            "texture_path", L"Texture Path", [this]() { return GetTexturePath(); },
            [this](const FString& InValue) { SetTexturePath(InValue); }, TexturePathOptions);

        Builder.AddBool(
            "billboard", L"Billboard", [this]() { return GetBillboard(); },
            [this](bool bInValue) { SetBillboard(bInValue); });
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
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to resolve texture path for SpriteComponent: %s", TexturePath.c_str());
            return;
        }

        FAssetLoadParams LoadParams;
        LoadParams.ExplicitType = EAssetType::Texture;

        UAsset*          LoadedAsset = InAssetManager->Load(AbsolutePath.native(), LoadParams);
        UTexture2DAsset* TextureAsset = Cast<UTexture2DAsset>(LoadedAsset);
        if (TextureAsset == nullptr)
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to load texture asset for SpriteComponent: %s", TexturePath.c_str());
            return;
        }

        SetTextureResource(TextureAsset->GetResource());
    }

    REGISTER_CLASS(Engine::Component, USpriteComponent)
} // namespace Engine::Component