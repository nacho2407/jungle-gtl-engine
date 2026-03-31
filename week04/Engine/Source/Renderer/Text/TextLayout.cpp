#include "Renderer/Text/TextLayout.h"

#include "Renderer/Types/RenderDebugColors.h"

namespace
{
    constexpr float DefaultLineHeight = 16.0f;
    constexpr uint32 Utf8ReplacementCodePoint = static_cast<uint32>('?');

    TArray<uint32> DecodeUtf8CodePoints(const FString& InText)
    {
        TArray<uint32> CodePoints;
        CodePoints.reserve(InText.size());

        const uint8* Bytes = reinterpret_cast<const uint8*>(InText.data());
        const size_t ByteCount = InText.size();

        size_t Index = 0;
        while (Index < ByteCount)
        {
            const uint8 Lead = Bytes[Index];

            if (Lead <= 0x7F)
            {
                CodePoints.push_back(static_cast<uint32>(Lead));
                ++Index;
                continue;
            }

            uint32 CodePoint = Utf8ReplacementCodePoint;
            size_t SequenceLength = 1;

            auto IsContinuationByte =
                [Bytes, ByteCount](size_t InIndex)
            {
                return InIndex < ByteCount && (Bytes[InIndex] & 0xC0) == 0x80;
            };

            if ((Lead & 0xE0) == 0xC0)
            {
                SequenceLength = 2;
                if (IsContinuationByte(Index + 1))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x1F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 1] & 0x3F);

                    if (Candidate >= 0x80)
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF0) == 0xE0)
            {
                SequenceLength = 3;
                if (IsContinuationByte(Index + 1) && IsContinuationByte(Index + 2))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x0F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 2] & 0x3F);

                    if (Candidate >= 0x800 && !(Candidate >= 0xD800 && Candidate <= 0xDFFF))
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF8) == 0xF0)
            {
                SequenceLength = 4;
                if (IsContinuationByte(Index + 1) && IsContinuationByte(Index + 2) &&
                    IsContinuationByte(Index + 3))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x07)) << 18) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 2] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 3] & 0x3F);

                    if (Candidate >= 0x10000 && Candidate <= 0x10FFFF)
                    {
                        CodePoint = Candidate;
                    }
                }
            }

            CodePoints.push_back(CodePoint);
            Index += SequenceLength;
        }

        return CodePoints;
    }

    enum class EResolvedGlyphKind : uint8
    {
        Normal,
        QuestionFallback,
        Missing,
    };

    struct FResolvedGlyph
    {
        const FFontGlyph* Glyph = nullptr;
        EResolvedGlyphKind Kind = EResolvedGlyphKind::Missing;
    };

    FResolvedGlyph ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint)
    {
        if (const FFontGlyph* Glyph = InFont.FindGlyph(InCodePoint))
        {
            if (Glyph->IsValid())
            {
                return {Glyph, EResolvedGlyphKind::Normal};
            }
        }

        if (const FFontGlyph* QuestionGlyph = InFont.FindGlyph(static_cast<uint32>('?')))
        {
            if (QuestionGlyph->IsValid())
            {
                return {QuestionGlyph, EResolvedGlyphKind::QuestionFallback};
            }
        }

        return {nullptr, EResolvedGlyphKind::Missing};
    }

    float GetMissingGlyphAdvance(const FFontResource& InFont, float InLineHeight, float InScale)
    {
        if (const FFontGlyph* SpaceGlyph = InFont.FindGlyph(static_cast<uint32>(' ')))
        {
            if (SpaceGlyph->IsValid())
            {
                return static_cast<float>(SpaceGlyph->XAdvance) * InScale;
            }
        }

        return InLineHeight * 0.5f * InScale;
    }
} // namespace

FTextLayoutResult BuildTextLayout(const FTextRenderItem& InItem)
{
    FTextLayoutResult Layout;

    if (InItem.FontResource == nullptr || InItem.Text.empty())
    {
        return Layout;
    }

    const float RawLineHeight = (InItem.FontResource->Common.LineHeight > 0)
                                    ? static_cast<float>(InItem.FontResource->Common.LineHeight)
                                    : DefaultLineHeight;

    const float UnitScale =
        (RawLineHeight > 0.0f) ? (1.0f / RawLineHeight) : (1.0f / DefaultLineHeight);

    const float Scale = (InItem.TextScale > 0.0f) ? (InItem.TextScale * UnitScale) : UnitScale;
    const float LetterSpacing = InItem.LetterSpacing * UnitScale;
    const float LineSpacing = InItem.LineSpacing * UnitScale;

    const float MissingAdvance = GetMissingGlyphAdvance(*InItem.FontResource, RawLineHeight, Scale);
    const float MissingWidth = MissingAdvance;
    const float MissingHeight = RawLineHeight * Scale;

    float PenX = 0.0f;
    float PenY = 0.0f;
    bool  bHasBounds = false;

    const TArray<uint32> CodePoints = DecodeUtf8CodePoints(InItem.Text);

    for (uint32 CodePoint : CodePoints)
    {
        if (CodePoint == static_cast<uint32>('\r'))
        {
            continue;
        }

        if (CodePoint == static_cast<uint32>('\n'))
        {
            PenX = 0.0f;
            PenY += RawLineHeight * Scale + LineSpacing;
            continue;
        }

        if (CodePoint == static_cast<uint32>(' '))
        {
            float SpaceAdvance = 0.0f;

            if (const FFontGlyph* SpaceGlyph =
                    InItem.FontResource->FindGlyph(static_cast<uint32>(' ')))
            {
                if (SpaceGlyph->IsValid())
                {
                    SpaceAdvance = static_cast<float>(SpaceGlyph->XAdvance) * Scale;
                }
            }

            if (SpaceAdvance <= 0.0f)
            {
                SpaceAdvance = RawLineHeight * 0.25f * Scale;
            }

            PenX += SpaceAdvance;
            continue;
        }

        const FResolvedGlyph Resolved = ResolveGlyph(*InItem.FontResource, CodePoint);

        FTextLayoutGlyph OutGlyph;

        if (Resolved.Kind == EResolvedGlyphKind::Missing)
        {
            OutGlyph.Glyph = nullptr;
            OutGlyph.bSolidColorQuad = true;
            OutGlyph.SolidColor = RenderDebugColors::MissingGlyph;

            OutGlyph.MinX = PenX;
            OutGlyph.MinY = PenY;
            OutGlyph.MaxX = PenX + MissingWidth;
            OutGlyph.MaxY = PenY + MissingHeight;

            PenX += MissingAdvance + LetterSpacing;
        }
        else
        {
            const FFontGlyph* Glyph = Resolved.Glyph;

            OutGlyph.Glyph = Glyph;
            OutGlyph.bSolidColorQuad = false;
            OutGlyph.MinX = PenX + static_cast<float>(Glyph->XOffset) * Scale;
            OutGlyph.MinY = PenY + static_cast<float>(Glyph->YOffset) * Scale;
            OutGlyph.MaxX = OutGlyph.MinX + static_cast<float>(Glyph->Width) * Scale;
            OutGlyph.MaxY = OutGlyph.MinY + static_cast<float>(Glyph->Height) * Scale;

            PenX += Scale * static_cast<float>(Glyph->XAdvance);
            PenX += LetterSpacing;
        }

        Layout.Glyphs.push_back(OutGlyph);

        if (!bHasBounds)
        {
            Layout.MinX = OutGlyph.MinX;
            Layout.MinY = OutGlyph.MinY;
            Layout.MaxX = OutGlyph.MaxX;
            Layout.MaxY = OutGlyph.MaxY;
            bHasBounds = true;
        }
        else
        {
            Layout.MinX = std::min(Layout.MinX, OutGlyph.MinX);
            Layout.MinY = std::min(Layout.MinY, OutGlyph.MinY);
            Layout.MaxX = std::max(Layout.MaxX, OutGlyph.MaxX);
            Layout.MaxY = std::max(Layout.MaxY, OutGlyph.MaxY);
        }
    }

    return Layout;
}

