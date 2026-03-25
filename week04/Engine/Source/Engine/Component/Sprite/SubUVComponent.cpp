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

    void USubUVComponent::Update(float DeltaTime)
    {
        UAtlasComponent::Update(DeltaTime);

        const int32 FrameCount = GetFrameCount();
        if (FrameCount <= 1 || AnimationFPS <= 0.0f || DeltaTime <= 0.0f)
        {
            return;
        }

        const float SecondsPerFrame = 1.0f / AnimationFPS;
        AnimationTimeAccumulator += DeltaTime;
        if (AnimationTimeAccumulator < SecondsPerFrame)
        {
            return;
        }

        const int32 FramesToAdvance =
            static_cast<int32>(AnimationTimeAccumulator / SecondsPerFrame);
        AnimationTimeAccumulator -= SecondsPerFrame * static_cast<float>(FramesToAdvance);

        /*const int32 NextFrameIndex = (GetFrameIndex() + FramesToAdvance) % FrameCount;
        SetFrameIndex(NextFrameIndex);*/
        const int32 CurrentFrame = GetFrameIndex();
        int32       NextFrameIndex = CurrentFrame + FramesToAdvance;

        if (bLoopFlag)
        {
            NextFrameIndex = NextFrameIndex % FrameCount;
        }
        else
        {
            if (NextFrameIndex > FrameCount)
            {
                NextFrameIndex = FrameCount - 1;
                AnimationTimeAccumulator = 0.0f;
            }
        }



        SetFrameIndex(NextFrameIndex);
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

    void USubUVComponent::SetAnimationFPS(float InAnimationFPS)
    {
        AnimationFPS = (InAnimationFPS >= 0.0f) ? InAnimationFPS : 0.0f;
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
        FComponentPropertyOptions FloatOptions;
        FloatOptions.DragSpeed = 0.1f;

        FComponentPropertyOptions AtlasPathOptions;
        AtlasPathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::SpriteAtlasFile;

        Builder.AddAssetPath(
            "subuv_atlas_path", L"SubUV Atlas Path", [this]() { return GetSubUVAtlasPath(); },
            [this](const FString& InValue) { SetSubUVAtlasPath(InValue); }, AtlasPathOptions);

        Builder.AddInt(
            "frame_index", L"Frame Index", [this]() { return GetFrameIndex(); },
            [this](int32 InValue) { SetFrameIndex(InValue); }, IntOptions);
        Builder.AddFloat(
            "animation_fps", L"Animation FPS", [this]() { return GetAnimationFPS(); },
            [this](float InValue) { SetAnimationFPS(InValue); }, FloatOptions);
        Builder.AddBool(
            "looping", L"Looping", [this]() { return IsLooping(); },
            [this](bool bInValue) { SetLooping(bInValue); });
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
