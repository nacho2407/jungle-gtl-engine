#include "AtlasTextComponent.h"

#include "Engine/Component/Core/ComponentProperty.h"

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

    void UAtlasTextComponent::SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }

    void UAtlasTextComponent::SetBillboardOffset(const FVector& InBillboardOffset)
    {
        BillboardOffset = InBillboardOffset;
    }

    void UAtlasTextComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UAtlasComponent::DescribeProperties(Builder);

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
        Builder.AddBool(
            "billboard", L"Billboard", [this]() { return GetBillboard(); },
            [this](bool bInValue) { SetBillboard(bInValue); });
        Builder.AddVector3(
            "billboard_offset", L"Billboard Offset",
            [this]() { return GetBillboardOffset(); },
            [this](const FVector& InValue) { SetBillboardOffset(InValue); });
        Builder.AddAssetPath(
            "font_path", L"Font Path", [this]() { return GetFontPath(); },
            [this](const FString& InValue) { SetFontPath(InValue); });
    }

    REGISTER_CLASS(Engine::Component, UAtlasTextComponent)
} // namespace Engine::Component
