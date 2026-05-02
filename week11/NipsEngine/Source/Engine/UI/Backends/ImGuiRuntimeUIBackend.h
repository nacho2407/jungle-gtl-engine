#pragma once

#include "Core/Containers/String.h"
#include "UI/RuntimeUIBackend.h"

#include <functional>

struct FRuntimeUIResolvedImage
{
    void* TextureId = nullptr;
    float Width = 0.0f;
    float Height = 0.0f;

    bool IsValid() const { return TextureId != nullptr && Width > 0.0f && Height > 0.0f; }
};

using FRuntimeUIImageResolver = std::function<FRuntimeUIResolvedImage(const FString& ImagePath)>;

class FImGuiRuntimeUIBackend : public IRuntimeUIBackend
{
public:
    void SetImageResolver(FRuntimeUIImageResolver InResolver);

    void BeginFrame(const FRuntimeUIRenderContext& Context) override;
    void DrawPanel(const FRuntimeUIWidget& Widget) override;
    void DrawTextWidget(const FRuntimeUIWidget& Widget) override;
    void DrawButton(const FRuntimeUIWidget& Widget) override;
    void DrawImage(const FRuntimeUIWidget& Widget) override;
    void DrawProgressBar(const FRuntimeUIWidget& Widget) override;
    void EndFrame() override;

private:
    struct ImDrawList* GetDrawList() const;
    FRuntimeUIResolvedImage ResolveImage(const FString& ImagePath) const;
    bool DrawImageResolved(
        const FRuntimeUIResolvedImage& Image,
        const FRuntimeUIRect& Rect,
        const FRuntimeUIUVRect& UV,
        unsigned int Color,
        ERuntimeUIImageDrawMode DrawMode,
        const FRuntimeUIMargin& Border,
        float Rounding);
    void DrawNineSlice(
        const FRuntimeUIResolvedImage& Image,
        const FRuntimeUIRect& Rect,
        const FRuntimeUIUVRect& UV,
        unsigned int Color,
        const FRuntimeUIMargin& Border);
    void DrawImagePlaceholder(const FRuntimeUIRect& Rect, float Alpha, float Rounding);
    void DrawProgressFillImage(const FRuntimeUIWidget& Widget, const FRuntimeUIResolvedImage& Image);
    unsigned int ToColorU32(const FRuntimeUIColor& Color, float AlphaMultiplier = 1.0f) const;
    void DrawTextCentered(const FRuntimeUIWidget& Widget, unsigned int Color);

    FRuntimeUIRenderContext CurrentContext;
    FRuntimeUIImageResolver ImageResolver;
};
