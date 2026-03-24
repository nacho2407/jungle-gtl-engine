#include "Core/CoreMinimal.h"
#include "FontAtlasLoader.h"

#include "FontAsset.h"

FFontAtlasLoader::FFontAtlasLoader(FD3D11DynamicRHI* InRHI) : RHI(InRHI)
{
}

bool FFontAtlasLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Path.empty())
    {
        return false;
    }
    
    if (Params.ExplicitType != EAssetType::Unknown &&
        Params.ExplicitType != EAssetType::Font)
    {
        return false;
    }
    
    std::filesystem::path FilePath(Path);
    FWString Ext = FilePath.extension().wstring();
    
    for (wchar_t & Ch : Ext)
    {
        Ch = static_cast<wchar_t>(std::tolower(Ch));
    }
    
    return Ext == L".json";
}

EAssetType FFontAtlasLoader::GetAssetType() const
{
    return EAssetType::Font;
}

uint64 FFontAtlasLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
    uint64 Hash = 14695981039346656037ull;
    Hash ^= static_cast<uint64>(GetAssetType());
    Hash *= 1099511628211ull;
    
    return Hash;
}

UAsset* FFontAtlasLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    FFontResource FontResource;
    if (!ParseFontAtlasJson(Source,FontResource))
    {
        return nullptr;
    }
    
    UFontAsset * NewFontAsset = new UFontAsset();
    NewFontAsset->Initialize(Source, FontResource);
    
    return NewFontAsset;
}

bool FFontAtlasLoader::ParseFontAtlasJson(const FSourceRecord& Source, FFontResource& OutFont) const
{
    OutFont.Reset();

    std::ifstream File(Source.AbsolutePath);
    if (!File.is_open())
    {
        return false;
    }

    nlohmann::json Root;
    try
    {
        File >> Root;
    }
    catch (...)
    {
        return false;
    }

    if (!ParseInfo(Root, OutFont.Info))
    {
        return false;
    }

    if (!ParseCommon(Root, OutFont.Common))
    {
        return false;
    }

    if (!ParsePages(Root, OutFont.PageFiles))
    {
        return false;
    }

    if (!ParseChars(Root, OutFont.Glyphs))
    {
        return false;
    }

    if (OutFont.PageFiles.empty())
    {
        return false;
    }

    // 현재는 단일 atlas page 기준
    if (!LoadAtlasTexture(Source, OutFont.PageFiles[0], OutFont.Atlas))
    {
        OutFont.Reset();
        return false;
    }

    // common 값이 비어 있으면 실제 atlas 크기로 보정
    if (OutFont.Common.ScaleW == 0)
    {
        OutFont.Common.ScaleW = OutFont.Atlas.Width;
    }

    if (OutFont.Common.ScaleH == 0)
    {
        OutFont.Common.ScaleH = OutFont.Atlas.Height;
    }

    return true;
}

bool FFontAtlasLoader::ParseInfo(const nlohmann::json& Root, FFontInfo& OutInfo) const
{
    if (!Root.contains("info"))
    {
        return false;
    }

    const auto& Info = Root["info"];

    OutInfo.Face     = Info.value("face", "");
    OutInfo.Size     = Info.value("size", 0);
    OutInfo.bBold    = Info.value("bold", 0) != 0;
    OutInfo.bItalic  = Info.value("italic", 0) != 0;
    OutInfo.bUnicode = Info.value("unicode", 0) != 0;

    OutInfo.StretchH = Info.value("stretchH", 100);
    OutInfo.bSmooth  = Info.value("smooth", 1) != 0;
    OutInfo.AA       = Info.value("aa", 1);
    OutInfo.Outline  = Info.value("outline", 0);

    if (Info.contains("padding") && Info["padding"].is_array() && Info["padding"].size() == 4)
    {
        OutInfo.PaddingLeft   = Info["padding"][0].get<int32>();
        OutInfo.PaddingTop    = Info["padding"][1].get<int32>();
        OutInfo.PaddingRight  = Info["padding"][2].get<int32>();
        OutInfo.PaddingBottom = Info["padding"][3].get<int32>();
    }

    if (Info.contains("spacing") && Info["spacing"].is_array() && Info["spacing"].size() == 2)
    {
        OutInfo.SpacingX = Info["spacing"][0].get<int32>();
        OutInfo.SpacingY = Info["spacing"][1].get<int32>();
    }

    return true;
}

bool FFontAtlasLoader::ParseCommon(const nlohmann::json& Root, FFontCommon& OutCommon) const
{
    if (!Root.contains("common"))
    {
        return false;
    }

    const auto& Common = Root["common"];

    OutCommon.LineHeight   = Common.value("lineHeight", 0u);
    OutCommon.Base         = Common.value("base", 0u);
    OutCommon.ScaleW       = Common.value("scaleW", 0u);
    OutCommon.ScaleH       = Common.value("scaleH", 0u);
    OutCommon.Pages        = Common.value("pages", 0u);
    OutCommon.bPacked      = Common.value("packed", 0) != 0;

    OutCommon.AlphaChannel = Common.value("alphaChnl", 0u);
    OutCommon.RedChannel   = Common.value("redChnl", 0u);
    OutCommon.GreenChannel = Common.value("greenChnl", 0u);
    OutCommon.BlueChannel  = Common.value("blueChnl", 0u);

    return true;
}

bool FFontAtlasLoader::ParsePages(const nlohmann::json& Root, TArray<FString>& OutPages) const
{
    if (!Root.contains("pages") || !Root["pages"].is_array())
    {
        return false;
    }

    for (const auto& Page : Root["pages"])
    {
        if (!Page.contains("file"))
        {
            continue;
        }

        OutPages.push_back(Page["file"].get<std::string>());
    }

    return !OutPages.empty();
}

bool FFontAtlasLoader::ParseChars(const nlohmann::json& Root, TArray<FFontGlyph>& OutGlyphs) const
{
    if (!Root.contains("chars") || !Root["chars"].is_array())
    {
        return false;                                                                                                                                                                                                                                                                                                                          
    }

    for (const auto& Ch : Root["chars"])
    {
        FFontGlyph Glyph;
        Glyph.Id       = Ch.value("id", 0u);
        Glyph.X        = Ch.value("x", 0u);
        Glyph.Y        = Ch.value("y", 0u);
        Glyph.Width    = Ch.value("width", 0u);
        Glyph.Height   = Ch.value("height", 0u);
        Glyph.XOffset  = Ch.value("xoffset", 0);
        Glyph.YOffset  = Ch.value("yoffset", 0);
        Glyph.XAdvance = Ch.value("xadvance", 0);
        Glyph.Page     = Ch.value("page", 0u);
        Glyph.Channel  = Ch.value("chnl", 0u);

        OutGlyphs[Glyph.Id] = Glyph;
    }

    return true;
}

bool FFontAtlasLoader::LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile,
    FTextureResource& OutAtlas) const
{
}

FWString FFontAtlasLoader::ResolveSiblingPath(const FWString& BaseFilePath,
    const FString& RelativePath) const
{
}