#include "AtlasTextComponent.h"

namespace Engine::Component
{
    void UAtlasTextComponent::SetText(const FString& InText) { Text = InText; }

    void UAtlasTextComponent::SetFontResource(FFontResource* InFontResource)
    {
        FontResource = InFontResource;
    }

    void UAtlasTextComponent::SetTextScale(float InTextScale) { TextScale = InTextScale; }

    void UAtlasTextComponent::SetLetterSpacing(float InLetterSpacing)
    {
        LetterSpacing = InLetterSpacing;
    }

    void UAtlasTextComponent::SetLineSpacing(float InLineSpacing) { LineSpacing = InLineSpacing; }

    REGISTER_CLASS(Engine::Component, UAtlasTextComponent)
} // namespace Engine::Component
