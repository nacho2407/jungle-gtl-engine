// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Batching/FontBatch.h"
#include "Resource/ResourceManager.h"

void FFontBatch::Create(ID3D11Device* InDevice)
{
    WorldBuffer.Create(InDevice, 1024, 1536);
    OverlayWorldBuffer.Create(InDevice, 512, 768);
    ScreenBuffer.Create(InDevice, 256, 384);

    if (const FFontResource* DefaultFont = FResourceManager::Get().FindFont(FName("Default")))
    {
        if (DefaultFont->Columns > 0 && DefaultFont->Rows > 0)
        {
            BuildCharInfoMap(DefaultFont->Columns, DefaultFont->Rows);
        }
    }
}

void FFontBatch::Release()
{
    CharInfoMap.clear();
    ClearAll();
    WorldBuffer.Release();
    OverlayWorldBuffer.Release();
    ScreenBuffer.Release();
}

void FFontBatch::BuildCharInfoMap(uint32 Columns, uint32 Rows)
{
    CharInfoMap.clear();
    CachedColumns = Columns;
    CachedRows    = Rows;

    const float CellW = 1.0f / static_cast<float>(Columns);
    const float CellH = 1.0f / static_cast<float>(Rows);

    auto AddChar = [&](uint32 Codepoint, uint32 Slot)
    {
        const uint32 Col = Slot % Columns;
        const uint32 Row = Slot / Columns;
        if (Row >= Rows)
            return;
        CharInfoMap[Codepoint] = { Col * CellW, Row * CellH, CellW, CellH };
    };

    // ASCII 32(' ') ~ 126('~')
    for (uint32 CP = 32; CP <= 126; ++CP)
        AddChar(CP, CP - 32);

    // 한글 완성형 가(U+AC00) ~ 힣(U+D7A3)
    uint32 Slot = 127;
    for (uint32 CP = 0xAC00; CP <= 0xD7A3; ++CP, ++Slot)
        AddChar(CP, Slot - 32);
}

void FFontBatch::EnsureCharInfoMap(const FFontResource* Resource)
{
    if (!Resource || Resource->Columns == 0 || Resource->Rows == 0)
        return;
    if (CachedColumns == Resource->Columns && CachedRows == Resource->Rows)
        return;
    BuildCharInfoMap(Resource->Columns, Resource->Rows);
}

void FFontBatch::GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const
{
    const auto It = CharInfoMap.find(Codepoint);
    if (It == CharInfoMap.end())
    {
        OutUVMin = FVector2(0, 0);
        OutUVMax = FVector2(0, 0);
        return;
    }
    const FCharacterInfo& Info = It->second;
    OutUVMin                   = FVector2(Info.U, Info.V);
    OutUVMax                   = FVector2(Info.U + Info.Width, Info.V + Info.Height);
}

void FFontBatch::ClearWorld()
{
    WorldBuffer.Clear();
}

void FFontBatch::ClearOverlayWorld()
{
    OverlayWorldBuffer.Clear();
}

void FFontBatch::ClearScreen()
{
    ScreenBuffer.Clear();
}

void FFontBatch::ClearAll()
{
    ClearWorld();
    ClearOverlayWorld();
    ClearScreen();
}

namespace
{
uint32 CountUTF8Codepoints(const FString& Text)
{
    uint32 Count = 0;
    for (size_t i = 0; i < Text.size(); ++i)
    {
        if ((static_cast<unsigned char>(Text[i]) & 0xC0) != 0x80)
        {
            ++Count;
        }
    }
    return Count;
}

void AddWorldTextToBuffer(TBatchBuffer<FTextureVertex>& Buffer, const FFontBatch* Batch, const FString& Text,
                          const FVector& WorldPos, const FVector& CamRight, const FVector& CamUp,
                          const FVector& WorldScale, float Scale)
{
    if (Text.empty())
        return;

    const float  CharW       = 0.5f * Scale * WorldScale.Y;
    const float  CharH       = 0.5f * Scale * WorldScale.Z;
    const size_t ByteCount   = Text.size();
    const uint32 GlyphCount  = CountUTF8Codepoints(Text);
    float        CharCursorX = (GlyphCount > 0) ? (-0.5f * static_cast<float>(GlyphCount - 1) * CharW) : 0.0f;

    const uint32 Base    = static_cast<uint32>(Buffer.Vertices.size());
    const uint32 IdxBase = static_cast<uint32>(Buffer.Indices.size());

    Buffer.Vertices.resize(Base + ByteCount * 4);
    Buffer.Indices.resize(IdxBase + ByteCount * 6);
    FTextureVertex* pV = Buffer.Vertices.data() + Base;
    uint32*         pI = Buffer.Indices.data() + IdxBase;

    const FVector HalfRight = CamRight * (CharW * 0.5f);
    const FVector HalfUp    = CamUp * (CharH * 0.5f);

    const uint8*       Ptr     = reinterpret_cast<const uint8*>(Text.c_str());
    const uint8* const End     = Ptr + ByteCount;
    uint32             CharIdx = 0;

    while (Ptr < End)
    {
        uint32 CP = 0;
        if (Ptr[0] < 0x80)
        {
            CP = Ptr[0];
            Ptr += 1;
        }
        else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)
        {
            CP = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F);
            Ptr += 2;
        }
        else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)
        {
            CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F);
            Ptr += 3;
        }
        else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)
        {
            CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F);
            Ptr += 4;
        }
        else
        {
            ++Ptr;
            continue;
        }

        FVector2 UVMin, UVMax;
        Batch->GetCharUV(CP, UVMin, UVMax);

        const FVector Center = WorldPos + CamRight * CharCursorX;

        pV[0] = { Center + HalfUp - HalfRight, { UVMin.X, UVMin.Y } };
        pV[1] = { Center + HalfUp + HalfRight, { UVMax.X, UVMin.Y } };
        pV[2] = { Center - HalfUp - HalfRight, { UVMin.X, UVMax.Y } };
        pV[3] = { Center - HalfUp + HalfRight, { UVMax.X, UVMax.Y } };

        const uint32 Vi = Base + CharIdx * 4;
        pI[0]           = Vi;
        pI[1]           = Vi + 1;
        pI[2]           = Vi + 2;
        pI[3]           = Vi + 1;
        pI[4]           = Vi + 3;
        pI[5]           = Vi + 2;

        pV += 4;
        pI += 6;
        ++CharIdx;
        CharCursorX += CharW;
    }

    Buffer.Vertices.resize(Base + CharIdx * 4);
    Buffer.Indices.resize(IdxBase + CharIdx * 6);
}
} // namespace

void FFontBatch::AddWorldText(const FString& Text,
                              const FVector& WorldPos,
                              const FVector& CamRight,
                              const FVector& CamUp,
                              const FVector& WorldScale,
                              float          Scale)
{
    AddWorldTextToBuffer(WorldBuffer, this, Text, WorldPos, CamRight, CamUp, WorldScale, Scale);
}

void FFontBatch::AddOverlayWorldText(const FString& Text,
                                     const FVector& WorldPos,
                                     const FVector& CamRight,
                                     const FVector& CamUp,
                                     const FVector& WorldScale,
                                     float          Scale)
{
    AddWorldTextToBuffer(OverlayWorldBuffer, this, Text, WorldPos, CamRight, CamUp, WorldScale, Scale);
}

void FFontBatch::AddScreenText(const FString& Text,
                               float ScreenX, float ScreenY,
                               float ViewportWidth, float ViewportHeight,
                               float Scale)
{
    if (Text.empty())
        return;
    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
        return;

    const float CharW         = 23.0f * Scale;
    const float CharH         = 23.0f * Scale;
    const float LetterSpacing = -0.5f * CharW;

    const uint32 Base      = static_cast<uint32>(ScreenBuffer.Vertices.size());
    const uint32 IdxBase   = static_cast<uint32>(ScreenBuffer.Indices.size());
    const size_t ByteCount = Text.size();

    ScreenBuffer.Vertices.resize(Base + ByteCount * 4);
    ScreenBuffer.Indices.resize(IdxBase + ByteCount * 6);

    FTextureVertex* pV = ScreenBuffer.Vertices.data() + Base;
    uint32*         pI = ScreenBuffer.Indices.data() + IdxBase;

    const uint8*       Ptr = reinterpret_cast<const uint8*>(Text.c_str());
    const uint8* const End = Ptr + ByteCount;

    uint32 CharIdx = 0;
    float  CursorX = ScreenX;

    auto PixelToClipX = [ViewportWidth](float X) -> float
    {
        return (X / ViewportWidth) * 2.0f - 1.0f;
    };

    auto PixelToClipY = [ViewportHeight](float Y) -> float
    {
        return 1.0f - (Y / ViewportHeight) * 2.0f;
    };

    while (Ptr < End)
    {
        uint32 CP = 0;
        if (Ptr[0] < 0x80)
        {
            CP = Ptr[0];
            Ptr += 1;
        }
        else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)
        {
            CP = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F);
            Ptr += 2;
        }
        else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)
        {
            CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F);
            Ptr += 3;
        }
        else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)
        {
            CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F);
            Ptr += 4;
        }
        else
        {
            ++Ptr;
            continue;
        }

        FVector2 UVMin, UVMax;
        GetCharUV(CP, UVMin, UVMax);

        const float Left   = PixelToClipX(CursorX);
        const float Right  = PixelToClipX(CursorX + CharW);
        const float Top    = PixelToClipY(ScreenY);
        const float Bottom = PixelToClipY(ScreenY + CharH);

        pV[0] = { FVector(Left, Top, 0.0f), FVector2(UVMin.X, UVMin.Y) };
        pV[1] = { FVector(Right, Top, 0.0f), FVector2(UVMax.X, UVMin.Y) };
        pV[2] = { FVector(Left, Bottom, 0.0f), FVector2(UVMin.X, UVMax.Y) };
        pV[3] = { FVector(Right, Bottom, 0.0f), FVector2(UVMax.X, UVMax.Y) };

        const uint32 Vi = Base + CharIdx * 4;
        pI[0]           = Vi;
        pI[1]           = Vi + 1;
        pI[2]           = Vi + 2;
        pI[3]           = Vi + 1;
        pI[4]           = Vi + 3;
        pI[5]           = Vi + 2;

        pV += 4;
        pI += 6;
        ++CharIdx;
        CursorX += CharW + LetterSpacing;
    }

    ScreenBuffer.Vertices.resize(Base + CharIdx * 4);
    ScreenBuffer.Indices.resize(IdxBase + CharIdx * 6);
}

bool FFontBatch::UploadWorldBuffers(ID3D11DeviceContext* Context)
{
    return WorldBuffer.Upload(Context);
}

bool FFontBatch::UploadOverlayWorldBuffers(ID3D11DeviceContext* Context)
{
    return OverlayWorldBuffer.Upload(Context);
}

bool FFontBatch::UploadScreenBuffers(ID3D11DeviceContext* Context)
{
    return ScreenBuffer.Upload(Context);
}
