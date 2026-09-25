#include "SubUVComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/SubUVAtlasAsset.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "SceneIO/SceneAssetPath.h"

namespace Engine::Component
{
    void USubUVComponent::SetSubUVAtlasPath(const FString& InPath)
    {
        SubUVAtlasPath = InPath;
        SubUVResource = nullptr;
        SetTextureResource(nullptr);
    }

    void USubUVComponent::SetSubUVAtlasResource(FSubUVAtlasResource* InResource)
    {
        SubUVResource = InResource;
        SetTextureResource(InResource != nullptr ? &InResource->Atlas : nullptr);
        SetFrameIndex(FrameIndex);
    }

    void USubUVComponent::SetFrameIndex(int32 InFrameIndex)
    {
        FrameIndex = (InFrameIndex >= 0) ? InFrameIndex : 0;

        const int32 FrameCount = GetFrameCount();
        if (FrameCount > 0 && FrameIndex >= FrameCount)
        {
            FrameIndex = FrameCount - 1;
        }
    }

    int32 USubUVComponent::GetFrameCount() const
    {
        if (SubUVResource != nullptr)
        {
            if (!SubUVResource->Frames.empty())
            {
                return static_cast<int32>(SubUVResource->Frames.size());
            }

            if (SubUVResource->Info.FrameCount > 0)
            {
                return static_cast<int32>(SubUVResource->Info.FrameCount);
            }
        }

        return AtlasRows * AtlasColumns;
    }

    void USubUVComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UAtlasComponent::DescribeProperties(Builder);

        FComponentPropertyOptions IntOptions;
        IntOptions.DragSpeed = 1.0f;

        FComponentPropertyOptions AtlasPathOptions;
        AtlasPathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::SpriteAtlasFile;

        Builder.AddAssetPath(
            "subuv_atlas_path", L"SubUV Atlas Path", [this]() { return GetSubUVAtlasPath(); },
            [this](const FString& InValue) { SetSubUVAtlasPath(InValue); }, AtlasPathOptions);

        Builder.AddInt(
            "frame_index", L"Frame Index", [this]() { return GetFrameIndex(); },
            [this](int32 InValue) { SetFrameIndex(InValue); }, IntOptions);
    }

    void USubUVComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        UAtlasComponent::ResolveAssetReferences(InAssetManager);
        SubUVResource = nullptr;

        if (InAssetManager == nullptr || SubUVAtlasPath.empty())
        {
            return;
        }

        const std::filesystem::path AbsolutePath =
            Engine::SceneIO::ResolveSceneAssetPathToAbsolute(SubUVAtlasPath);
        if (AbsolutePath.empty())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to resolve sprite atlas path for SubUVComponent: %s",
                   SubUVAtlasPath.c_str());
            return;
        }

        FAssetLoadParams LoadParams;
        LoadParams.ExplicitType = EAssetType::SpriteAtlas;

        UAsset*           LoadedAsset = InAssetManager->Load(AbsolutePath.native(), LoadParams);
        USubUVAtlasAsset* AtlasAsset = Cast<USubUVAtlasAsset>(LoadedAsset);
        if (AtlasAsset == nullptr)
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to load sprite atlas asset for SubUVComponent: %s",
                   SubUVAtlasPath.c_str());
            return;
        }

        SetSubUVAtlasResource(&AtlasAsset->GetResource());
    }

    REGISTER_CLASS(Engine::Component, USubUVComponent)
} // namespace Engine::Component