#include "AtlasTextComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/FontAsset.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Engine/Game/Actor.h"
#include "SceneIO/SceneAssetPath.h"

namespace Engine::Component
{
    void UAtlasTextComponent::SetText(const FString& InText) { Text = InText; }

    void UAtlasTextComponent::SetFontResource(FFontResource* InFontResource)
    {
        FontResource = InFontResource;
    }

    void UAtlasTextComponent::SetFontPath(const FString& InFontPath)
    {
        FontPath = InFontPath;
        FontResource = nullptr;
    }

    void UAtlasTextComponent::SetTextScale(float InTextScale) { TextScale = InTextScale; }

    void UAtlasTextComponent::SetLetterSpacing(float InLetterSpacing)
    {
        LetterSpacing = InLetterSpacing;
    }

    void UAtlasTextComponent::SetLineSpacing(float InLineSpacing) { LineSpacing = InLineSpacing; }

    void UAtlasTextComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UAtlasComponent::DescribeProperties(Builder);
        FComponentPropertyOptions FontPathOptions;
        FontPathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::FontFile;

        Builder.AddString(
            "text", L"Text", [this]() { return GetText(); },
            [this](const FString& InValue) { SetText(InValue); });
        Builder.AddFloat(
            "text_scale", L"Text Scale", [this]() { return GetTextScale(); },
            [this](float InValue) { SetTextScale(InValue); });
        Builder.AddFloat(
            "letter_spacing", L"Letter Spacing", [this]() { return GetLetterSpacing(); },
            [this](float InValue) { SetLetterSpacing(InValue); });
        Builder.AddFloat(
            "line_spacing", L"Line Spacing", [this]() { return GetLineSpacing(); },
            [this](float InValue) { SetLineSpacing(InValue); });
        Builder.AddVector3(
            "billboard_offset", L"Billboard Offset", [this]() { return GetBillboardOffset(); },
            [this](const FVector& InValue) { SetBillboardOffset(InValue); });
        Builder.AddAssetPath(
            "font_path", L"Font Path", [this]() { return GetFontPath(); },
            [this](const FString& InValue) { SetFontPath(InValue); }, FontPathOptions);
    }

    void UAtlasTextComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        FontResource = nullptr;

        if (InAssetManager == nullptr || FontPath.empty())
        {
            return;
        }

        const std::filesystem::path AbsolutePath =
            Engine::SceneIO::ResolveSceneAssetPathToAbsolute(FontPath);
        if (AbsolutePath.empty())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to resolve font path for AtlasTextComponent: %s", FontPath.c_str());
            return;
        }

        FAssetLoadParams LoadParams;
        LoadParams.ExplicitType = EAssetType::Font;

        UAsset*     LoadedAsset = InAssetManager->Load(AbsolutePath.native(), LoadParams);
        UFontAsset* FontAsset = Cast<UFontAsset>(LoadedAsset);
        if (FontAsset != nullptr)
        {
            SetFontResource(&FontAsset->GetResource());
        }
    }

    FMatrix UAtlasTextComponent::GetRenderPlacementWorld(const AActor& InOwnerActor) const
    {
        return InOwnerActor.GetWorldMatrix();
    }

    FVector UAtlasTextComponent::GetRenderPlacementOffset(const AActor& InOwnerActor) const
    {
        (void)InOwnerActor;
        return FVector::ZeroVector;
    }

    REGISTER_CLASS(Engine::Component, UAtlasTextComponent)
} // namespace Engine::Component
