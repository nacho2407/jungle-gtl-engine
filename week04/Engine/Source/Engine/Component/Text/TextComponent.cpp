#include "TextComponent.h"

namespace Engine::Component
{
    void UTextComponent::SetText(const FString& InText) { Text = InText; }

    void UTextComponent::SetFontResource(FFontResource* InFontResource)
    {
        FontResource = InFontResource;
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

    REGISTER_CLASS(Engine::Component, UTextComponent)
} // namespace Engine::Component
