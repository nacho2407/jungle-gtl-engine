#pragma once

#include "Engine/Component/Sprite/AtlasComponent.h"
#include "Renderer/RenderAsset/FontResource.h"

namespace Engine::Component
{
    class ENGINE_API UTextComponent : public UAtlasComponent
    {
        DECLARE_RTTI(UTextComponent, UAtlasComponent)

      public:
        UTextComponent() = default;
        ~UTextComponent() override = default;

        const FString& GetText() const { return Text; }
        void           SetText(const FString& InText);

        const FFontResource* GetFontResource() const { return FontResource; }
        FFontResource*       GetFontResource() { return FontResource; }
        void                 SetFontResource(FFontResource* InFontResource);

        float GetTextScale() const { return TextScale; }
        void  SetTextScale(float InTextScale);

        float GetLetterSpacing() const { return LetterSpacing; }
        void  SetLetterSpacing(float InLetterSpacing);

        float GetLineSpacing() const { return LineSpacing; }
        void  SetLineSpacing(float InLineSpacing);

        bool GetBillboard() const { return bBillboard; }
        void SetBillboard(bool bInBillboard);

        const FVector& GetBillboardOffset() const { return BillboardOffset; }
        void           SetBillboardOffset(const FVector& InBillboardOffset);

      protected:
        FString        Text;
        FFontResource* FontResource = nullptr;

        FColor TextColor = FColor::White();
        float  TextScale = 1.0f;
        float  LetterSpacing = 0.0f;
        float  LineSpacing = 0.0f;

        bool    bBillboard = true;
        FVector BillboardOffset = FVector(0.0f, 0.0f, 0.0f);
    };
} // namespace Engine::Component