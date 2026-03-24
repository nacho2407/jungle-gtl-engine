#pragma once

#include "Engine/Component/Sprite/AtlasComponent.h"
#include "Renderer/RenderAsset/FontResource.h"

namespace Engine::Component
{
    class ENGINE_API UAtlasTextComponent : public UAtlasComponent
    {
        DECLARE_RTTI(UAtlasTextComponent, UAtlasComponent)

      public:
        UAtlasTextComponent() = default;
        ~UAtlasTextComponent() override = default;

        const FString& GetText() const { return Text; }
        void           SetText(const FString& InText);

        const FFontResource* GetFontResource() const { return FontResource; }
        FFontResource*       GetFontResource() { return FontResource; }
        void                 SetFontResource(FFontResource* InFontResource);
        const FString&       GetFontPath() const { return FontPath; }
        void                 SetFontPath(const FString& InFontPath);

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
        void           DescribeProperties(FComponentPropertyBuilder& Builder) override;
        void           ResolveAssetReferences(UAssetManager* InAssetManager) override;

      protected:
        FString        Text;
        FFontResource* FontResource = nullptr;
        FString        FontPath;

        float TextScale = 1.0f;
        float LetterSpacing = 0.0f;
        float LineSpacing = 0.0f;
    };
} // namespace Engine::Component
