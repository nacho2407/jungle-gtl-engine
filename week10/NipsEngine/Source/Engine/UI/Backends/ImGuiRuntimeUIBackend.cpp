#include "UI/Backends/ImGuiRuntimeUIBackend.h"

#include "ImGui/imgui.h"
#include "UI/RuntimeUIWidget.h"

#include <cfloat>
#include <algorithm>
#include <utility>

namespace
{
    ImVec2 ToImVec2(const FRuntimeUIVector2& Value)
    {
        return ImVec2(Value.X, Value.Y);
    }
}

void FImGuiRuntimeUIBackend::SetImageResolver(FRuntimeUIImageResolver InResolver)
{
    ImageResolver = std::move(InResolver);
}

void FImGuiRuntimeUIBackend::BeginFrame(const FRuntimeUIRenderContext& Context)
{
    CurrentContext = Context;
}

void FImGuiRuntimeUIBackend::DrawPanel(const FRuntimeUIWidget& Widget)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    FRuntimeUIResolvedImage Image = ResolveImage(Widget.GetImagePath());
    if (Image.IsValid() &&
        DrawImageResolved(
            Image,
            Rect,
            Widget.GetImageUV(),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetImageDrawMode(),
            Widget.GetNineSliceBorder(),
            Widget.GetStyle().Rounding))
    {
        return;
    }

    ImDrawList* DrawList = GetDrawList();
    DrawList->AddRectFilled(
        ToImVec2(Rect.Min),
        ToImVec2(Rect.Max()),
        ToColorU32(Widget.GetStyle().BackgroundColor, Widget.GetComputedAlpha()),
        Widget.GetStyle().Rounding);
}

void FImGuiRuntimeUIBackend::DrawTextWidget(const FRuntimeUIWidget& Widget)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    ImDrawList* DrawList = GetDrawList();
    const unsigned int Color = ToColorU32(Widget.GetStyle().TextColor, Widget.GetComputedAlpha());

    ImFont* Font = ImGui::GetFont();
    const float FontSize = ImGui::GetFontSize() * Widget.GetStyle().FontScale;
    DrawList->AddText(
        Font,
        FontSize,
        ToImVec2(Rect.Min),
        Color,
        Widget.GetText().c_str());
}

void FImGuiRuntimeUIBackend::DrawButton(const FRuntimeUIWidget& Widget)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    ImDrawList* DrawList = GetDrawList();
    FRuntimeUIResolvedImage Image = ResolveImage(Widget.GetCurrentButtonImagePath());
    if (Image.IsValid() &&
        DrawImageResolved(
            Image,
            Rect,
            Widget.GetImageUV(),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetImageDrawMode(),
            Widget.GetNineSliceBorder(),
            Widget.GetStyle().Rounding))
    {
        DrawTextCentered(Widget, ToColorU32(Widget.GetStyle().TextColor, Widget.GetComputedAlpha()));
        return;
    }

    FRuntimeUIColor BaseColor = Widget.IsComputedEnabled()
        ? Widget.GetStyle().BackgroundColor
        : Widget.GetStyle().DisabledTint;

    if (Widget.IsPressed())
    {
        BaseColor = FRuntimeUIColor(BaseColor.R * 0.75f, BaseColor.G * 0.75f, BaseColor.B * 0.75f, BaseColor.A);
    }
    else if (Widget.IsHovered())
    {
        BaseColor = FRuntimeUIColor(
            std::min(BaseColor.R * 1.2f, 1.0f),
            std::min(BaseColor.G * 1.2f, 1.0f),
            std::min(BaseColor.B * 1.2f, 1.0f),
            BaseColor.A);
    }

    DrawList->AddRectFilled(
        ToImVec2(Rect.Min),
        ToImVec2(Rect.Max()),
        ToColorU32(BaseColor, Widget.GetComputedAlpha()),
        Widget.GetStyle().Rounding);

    DrawTextCentered(Widget, ToColorU32(Widget.GetStyle().TextColor, Widget.GetComputedAlpha()));
}

void FImGuiRuntimeUIBackend::DrawImage(const FRuntimeUIWidget& Widget)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    FRuntimeUIResolvedImage Image = ResolveImage(Widget.GetImagePath());

    if (Image.IsValid() &&
        DrawImageResolved(
            Image,
            Rect,
            Widget.GetImageUV(),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetImageDrawMode(),
            Widget.GetNineSliceBorder(),
            Widget.GetStyle().Rounding))
    {
        return;
    }

    DrawImagePlaceholder(Rect, Widget.GetComputedAlpha(), Widget.GetStyle().Rounding);
}

void FImGuiRuntimeUIBackend::DrawProgressBar(const FRuntimeUIWidget& Widget)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    ImDrawList* DrawList = GetDrawList();
    const float Progress = RuntimeUIClamp01(Widget.GetProgress());
    FRuntimeUIResolvedImage BackgroundImage = ResolveImage(Widget.GetProgressBackgroundImagePath());
    FRuntimeUIResolvedImage FillImage = ResolveImage(Widget.GetProgressFillImagePath());
    FRuntimeUIResolvedImage FrameImage = ResolveImage(Widget.GetProgressFrameImagePath());

    if (BackgroundImage.IsValid())
    {
        DrawImageResolved(
            BackgroundImage,
            Rect,
            Widget.GetImageUV(),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetImageDrawMode(),
            Widget.GetNineSliceBorder(),
            Widget.GetStyle().Rounding);
    }
    else
    {
        DrawList->AddRectFilled(
            ToImVec2(Rect.Min),
            ToImVec2(Rect.Max()),
            ToColorU32(FRuntimeUIColor(0.03f, 0.035f, 0.045f, 0.8f), Widget.GetComputedAlpha()),
            Widget.GetStyle().Rounding);
    }

    if (FillImage.IsValid())
    {
        DrawProgressFillImage(Widget, FillImage);
    }
    else
    {
        FRuntimeUIVector2 FillMin = Rect.Min;
        FRuntimeUIVector2 FillMax(Rect.Min.X + Rect.Size.X * Progress, Rect.Min.Y + Rect.Size.Y);
        if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::RightToLeft)
        {
            FillMin.X = Rect.Min.X + Rect.Size.X * (1.0f - Progress);
            FillMax.X = Rect.Min.X + Rect.Size.X;
        }
        else if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::BottomToTop)
        {
            FillMin.Y = Rect.Min.Y + Rect.Size.Y * (1.0f - Progress);
            FillMax = Rect.Max();
        }
        else if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::TopToBottom)
        {
            FillMax.X = Rect.Min.X + Rect.Size.X;
            FillMax.Y = Rect.Min.Y + Rect.Size.Y * Progress;
        }

        DrawList->AddRectFilled(
            ToImVec2(FillMin),
            ToImVec2(FillMax),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetStyle().Rounding);
    }

    if (FrameImage.IsValid())
    {
        DrawImageResolved(
            FrameImage,
            Rect,
            Widget.GetImageUV(),
            ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
            Widget.GetImageDrawMode(),
            Widget.GetNineSliceBorder(),
            Widget.GetStyle().Rounding);
    }
    else
    {
        DrawList->AddRect(
            ToImVec2(Rect.Min),
            ToImVec2(Rect.Max()),
            ToColorU32(Widget.GetStyle().TextColor, Widget.GetComputedAlpha() * 0.6f),
            Widget.GetStyle().Rounding);
    }
}

void FImGuiRuntimeUIBackend::EndFrame()
{
}

FRuntimeUIResolvedImage FImGuiRuntimeUIBackend::ResolveImage(const FString& ImagePath) const
{
    if (!ImageResolver || ImagePath.empty())
    {
        return {};
    }

    return ImageResolver(ImagePath);
}

bool FImGuiRuntimeUIBackend::DrawImageResolved(
    const FRuntimeUIResolvedImage& Image,
    const FRuntimeUIRect& Rect,
    const FRuntimeUIUVRect& UV,
    unsigned int Color,
    ERuntimeUIImageDrawMode DrawMode,
    const FRuntimeUIMargin& Border,
    float Rounding)
{
    if (!Image.IsValid())
    {
        return false;
    }

    if (DrawMode == ERuntimeUIImageDrawMode::NineSlice)
    {
        DrawNineSlice(Image, Rect, UV, Color, Border);
        return true;
    }

    GetDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(Image.TextureId),
        ToImVec2(Rect.Min),
        ToImVec2(Rect.Max()),
        ToImVec2(UV.Min),
        ToImVec2(UV.Max),
        Color);
    return true;
}

void FImGuiRuntimeUIBackend::DrawNineSlice(
    const FRuntimeUIResolvedImage& Image,
    const FRuntimeUIRect& Rect,
    const FRuntimeUIUVRect& UV,
    unsigned int Color,
    const FRuntimeUIMargin& Border)
{
    ImDrawList* DrawList = GetDrawList();
    const float Left = std::min(Border.Left, Rect.Size.X * 0.5f);
    const float Right = std::min(Border.Right, Rect.Size.X * 0.5f);
    const float Top = std::min(Border.Top, Rect.Size.Y * 0.5f);
    const float Bottom = std::min(Border.Bottom, Rect.Size.Y * 0.5f);

    const float UWidth = UV.Max.X - UV.Min.X;
    const float VHeight = UV.Max.Y - UV.Min.Y;
    const float ULeft = Image.Width > 0.0f ? UWidth * (Border.Left / Image.Width) : 0.0f;
    const float URight = Image.Width > 0.0f ? UWidth * (Border.Right / Image.Width) : 0.0f;
    const float VTop = Image.Height > 0.0f ? VHeight * (Border.Top / Image.Height) : 0.0f;
    const float VBottom = Image.Height > 0.0f ? VHeight * (Border.Bottom / Image.Height) : 0.0f;

    const float X[4] = { Rect.Min.X, Rect.Min.X + Left, Rect.Min.X + Rect.Size.X - Right, Rect.Min.X + Rect.Size.X };
    const float Y[4] = { Rect.Min.Y, Rect.Min.Y + Top, Rect.Min.Y + Rect.Size.Y - Bottom, Rect.Min.Y + Rect.Size.Y };
    const float U[4] = { UV.Min.X, UV.Min.X + ULeft, UV.Max.X - URight, UV.Max.X };
    const float V[4] = { UV.Min.Y, UV.Min.Y + VTop, UV.Max.Y - VBottom, UV.Max.Y };

    for (int32 Row = 0; Row < 3; ++Row)
    {
        for (int32 Col = 0; Col < 3; ++Col)
        {
            if (X[Col + 1] <= X[Col] || Y[Row + 1] <= Y[Row])
            {
                continue;
            }

            DrawList->AddImage(
                reinterpret_cast<ImTextureID>(Image.TextureId),
                ImVec2(X[Col], Y[Row]),
                ImVec2(X[Col + 1], Y[Row + 1]),
                ImVec2(U[Col], V[Row]),
                ImVec2(U[Col + 1], V[Row + 1]),
                Color);
        }
    }
}

void FImGuiRuntimeUIBackend::DrawImagePlaceholder(const FRuntimeUIRect& Rect, float Alpha, float Rounding)
{
    ImDrawList* DrawList = GetDrawList();
    DrawList->AddRectFilled(
        ToImVec2(Rect.Min),
        ToImVec2(Rect.Max()),
        ToColorU32(FRuntimeUIColor(0.08f, 0.09f, 0.1f, 0.75f), Alpha),
        Rounding);
    DrawList->AddRect(
        ToImVec2(Rect.Min),
        ToImVec2(Rect.Max()),
        ToColorU32(FRuntimeUIColor(0.45f, 0.5f, 0.6f, 0.7f), Alpha),
        Rounding);
}

void FImGuiRuntimeUIBackend::DrawProgressFillImage(const FRuntimeUIWidget& Widget, const FRuntimeUIResolvedImage& Image)
{
    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    const float Progress = RuntimeUIClamp01(Widget.GetProgress());
    FRuntimeUIRect FillRect = Rect;
    FRuntimeUIUVRect FillUV = Widget.GetImageUV();

    if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::LeftToRight)
    {
        FillRect.Size.X = Rect.Size.X * Progress;
        FillUV.Max.X = FillUV.Min.X + (Widget.GetImageUV().Max.X - Widget.GetImageUV().Min.X) * Progress;
    }
    else if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::RightToLeft)
    {
        FillRect.Min.X = Rect.Min.X + Rect.Size.X * (1.0f - Progress);
        FillRect.Size.X = Rect.Size.X * Progress;
        FillUV.Min.X = Widget.GetImageUV().Max.X - (Widget.GetImageUV().Max.X - Widget.GetImageUV().Min.X) * Progress;
    }
    else if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::BottomToTop)
    {
        FillRect.Min.Y = Rect.Min.Y + Rect.Size.Y * (1.0f - Progress);
        FillRect.Size.Y = Rect.Size.Y * Progress;
        FillUV.Min.Y = Widget.GetImageUV().Max.Y - (Widget.GetImageUV().Max.Y - Widget.GetImageUV().Min.Y) * Progress;
    }
    else if (Widget.GetProgressFillDirection() == ERuntimeUIProgressFillDirection::TopToBottom)
    {
        FillRect.Size.Y = Rect.Size.Y * Progress;
        FillUV.Max.Y = FillUV.Min.Y + (Widget.GetImageUV().Max.Y - Widget.GetImageUV().Min.Y) * Progress;
    }

    if (FillRect.Size.X <= 0.0f || FillRect.Size.Y <= 0.0f)
    {
        return;
    }

    DrawImageResolved(
        Image,
        FillRect,
        FillUV,
        ToColorU32(Widget.GetStyle().Tint, Widget.GetComputedAlpha()),
        ERuntimeUIImageDrawMode::Simple,
        FRuntimeUIMargin(),
        Widget.GetStyle().Rounding);
}

unsigned int FImGuiRuntimeUIBackend::ToColorU32(const FRuntimeUIColor& Color, float AlphaMultiplier) const
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(
        Color.R,
        Color.G,
        Color.B,
        RuntimeUIClamp01(Color.A * AlphaMultiplier)));
}

void FImGuiRuntimeUIBackend::DrawTextCentered(const FRuntimeUIWidget& Widget, unsigned int Color)
{
    if (Widget.GetText().empty())
    {
        return;
    }

    const FRuntimeUIRect& Rect = Widget.GetComputedRect();
    ImDrawList* DrawList = GetDrawList();
    ImFont* Font = ImGui::GetFont();
    const float FontSize = ImGui::GetFontSize() * Widget.GetStyle().FontScale;
    const ImVec2 TextSize = Font->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Widget.GetText().c_str());
    const ImVec2 TextPos(
        Rect.Min.X + (Rect.Size.X - TextSize.x) * 0.5f,
        Rect.Min.Y + (Rect.Size.Y - TextSize.y) * 0.5f);

    DrawList->AddText(Font, FontSize, TextPos, Color, Widget.GetText().c_str());
}

ImDrawList* FImGuiRuntimeUIBackend::GetDrawList() const
{
    if (CurrentContext.RenderMode == ERuntimeUIRenderMode::PIE)
    {
        return ImGui::GetWindowDrawList();
    }
    return ImGui::GetForegroundDrawList();
}
