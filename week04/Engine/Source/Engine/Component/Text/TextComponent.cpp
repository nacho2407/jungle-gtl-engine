#include "TextComponent.h"

#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    void UTextComponent::SetText(const FString& InText) { Text = InText; }

    void UTextComponent::SetFontResource(FFontResource* InFontResource)
    {
        FontResource = InFontResource;
    }

    void UTextComponent::SetFontPath(const FString& InFontPath)
    {
        FontPath = InFontPath;
        FontResource = nullptr;
    }

    void UTextComponent::SetTextScale(float InTextScale) { TextScale = InTextScale; }

    void UTextComponent::SetLetterSpacing(float InLetterSpacing)
    {
        LetterSpacing = InLetterSpacing;
    }

    void UTextComponent::SetLineSpacing(float InLineSpacing) { LineSpacing = InLineSpacing; }

    void UTextComponent::SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }

    void UTextComponent::SetBillboardOffset(const FVector& InBillboardOffset)
    {
        BillboardOffset = InBillboardOffset;
    }

    void UTextComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UAtlasComponent::DescribeProperties(Builder);

        FComponentPropertyOptions FineFloatOptions;
        FineFloatOptions.DragSpeed = 0.01f;

        Builder.AddString(
            "text", L"Text", [this]() { return GetText(); },
            [this](const FString& InValue) { SetText(InValue); });
        Builder.AddAssetPath(
            "font_path", L"Font Path", [this]() { return GetFontPath(); },
            [this](const FString& InValue) { SetFontPath(InValue); });
        Builder.AddFloat(
            "text_scale", L"Text Scale", [this]() { return GetTextScale(); },
            [this](float InValue) { SetTextScale(InValue); }, FineFloatOptions);
        Builder.AddFloat(
            "letter_spacing", L"Letter Spacing", [this]() { return GetLetterSpacing(); },
            [this](float InValue) { SetLetterSpacing(InValue); }, FineFloatOptions);
        Builder.AddFloat(
            "line_spacing", L"Line Spacing", [this]() { return GetLineSpacing(); },
            [this](float InValue) { SetLineSpacing(InValue); }, FineFloatOptions);
        Builder.AddBool(
            "billboard", L"Billboard", [this]() { return GetBillboard(); },
            [this](bool bInValue) { SetBillboard(bInValue); });
        Builder.AddVector3(
            "billboard_offset", L"Billboard Offset",
            [this]() { return GetBillboardOffset(); },
            [this](const FVector& InValue) { SetBillboardOffset(InValue); });
    }

    REGISTER_CLASS(Engine::Component, UTextComponent)
} // namespace Engine::Component
