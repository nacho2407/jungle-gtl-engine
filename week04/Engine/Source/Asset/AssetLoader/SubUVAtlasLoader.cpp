#include "Core/CoreMinimal.h"
#include "SubUVAtlasLoader.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <objbase.h>
#include <wincodec.h>

#include "Asset/SubUVAtlasAsset.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/MemoryTracker.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace
{
    class FScopedComInitializer
    {
      public:
        FScopedComInitializer()
        {
            Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            bInitialized = Result == S_OK || Result == S_FALSE;
            bUsable = bInitialized || Result == RPC_E_CHANGED_MODE;
        }

        ~FScopedComInitializer()
        {
            if (bInitialized)
            {
                CoUninitialize();
            }
        }

        bool IsUsable() const { return bUsable; }

      private:
        HRESULT Result = E_FAIL;
        bool    bInitialized = false;
        bool    bUsable = false;
    };

    bool DecodeWithWICBytes(const TArray<uint8>& Bytes, uint32& OutWidth, uint32& OutHeight,
                            TArray<uint8>& OutPixels)
    {
        if (Bytes.empty())
        {
            return false;
        }

        if (Bytes.size() > static_cast<size_t>(std::numeric_limits<DWORD>::max()))
        {
            return false;
        }

        FScopedComInitializer ComInitializer;
        if (!ComInitializer.IsUsable())
        {
            return false;
        }

        TComPtr<IWICImagingFactory> ImagingFactory;
        HRESULT Hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&ImagingFactory));
        if (FAILED(Hr))
        {
            return false;
        }

        TComPtr<IWICStream> Stream;
        Hr = ImagingFactory->CreateStream(&Stream);
        if (FAILED(Hr))
        {
            return false;
        }

        Hr = Stream->InitializeFromMemory(
            reinterpret_cast<WICInProcPointer>(const_cast<uint8*>(Bytes.data())),
            static_cast<DWORD>(Bytes.size()));
        if (FAILED(Hr))
        {
            return false;
        }

        TComPtr<IWICBitmapDecoder> Decoder;
        Hr = ImagingFactory->CreateDecoderFromStream(Stream.Get(), nullptr,
                                                     WICDecodeMetadataCacheOnLoad, &Decoder);
        if (FAILED(Hr))
        {
            return false;
        }

        TComPtr<IWICBitmapFrameDecode> Frame;
        Hr = Decoder->GetFrame(0, &Frame);
        if (FAILED(Hr))
        {
            return false;
        }

        UINT Width = 0;
        UINT Height = 0;
        Hr = Frame->GetSize(&Width, &Height);
        if (FAILED(Hr) || Width == 0 || Height == 0)
        {
            return false;
        }

        const uint64 Stride = static_cast<uint64>(Width) * 4ull;
        const uint64 BufferSize = Stride * static_cast<uint64>(Height);
        if (Stride > static_cast<uint64>(std::numeric_limits<UINT>::max()) ||
            BufferSize > static_cast<uint64>(std::numeric_limits<UINT>::max()) ||
            BufferSize > static_cast<uint64>(std::numeric_limits<size_t>::max()))
        {
            return false;
        }

        TComPtr<IWICFormatConverter> FormatConverter;
        Hr = ImagingFactory->CreateFormatConverter(&FormatConverter);
        if (FAILED(Hr))
        {
            return false;
        }

        Hr = FormatConverter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                         WICBitmapDitherTypeNone, nullptr, 0.0f,
                                         WICBitmapPaletteTypeCustom);
        if (FAILED(Hr))
        {
            return false;
        }

        OutWidth = static_cast<uint32>(Width);
        OutHeight = static_cast<uint32>(Height);
        OutPixels.resize(static_cast<size_t>(BufferSize));

        Hr = FormatConverter->CopyPixels(nullptr, static_cast<UINT>(Stride),
                                         static_cast<UINT>(BufferSize),
                                         reinterpret_cast<BYTE*>(OutPixels.data()));
        if (FAILED(Hr))
        {
            OutWidth = 0;
            OutHeight = 0;
            OutPixels.clear();
            return false;
        }

        return true;
    }

    FString GetStemName(const FString& FileName)
    {
        namespace fs = std::filesystem;
        return fs::path(FileName).stem().string();
    }

} // namespace

FSubUVAtlasLoader::FSubUVAtlasLoader(FD3D11RHI* InRHI) : RHI(InRHI) {}

bool FSubUVAtlasLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Params.ExplicitType != EAssetType::Unknown &&
        Params.ExplicitType != EAssetType::SpriteAtlas)
    {
        return false;
    }

    return HasExtension(Path, {L".json"});
}

EAssetType FSubUVAtlasLoader::GetAssetType() const { return EAssetType::SpriteAtlas; }

UAsset* FSubUVAtlasLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    FSubUVAtlasResource AtlasResource;
    if (!ParseSubUVJson(Source, AtlasResource))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] Failed at sprite atlas parse. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return nullptr;
    }

    USubUVAtlasAsset* NewAsset = new USubUVAtlasAsset();
    NewAsset->Initialize(Source, AtlasResource);
    return NewAsset;
}

bool FSubUVAtlasLoader::ParseSubUVJson(const FSourceRecord& Source,
                                       FSubUVAtlasResource& OutAtlas) const
{
    OutAtlas.Reset();

    if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at source bytes validation. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return false;
    }

    const nlohmann::json Root =
        nlohmann::json::parse(Source.FileBytes.begin(), Source.FileBytes.end(), nullptr, false);
    if (Root.is_discarded())
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at JSON parse. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return false;
    }

    if (!ParseMeta(Root, OutAtlas))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at meta parse. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return false;
    }

    if (!ParseFrames(Root, OutAtlas.Frames))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at frame parse. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        OutAtlas.Reset();
        return false;
    }

    if (!ParseSequences(OutAtlas.Frames, OutAtlas.Sequences))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at sequence parse. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        OutAtlas.Reset();
        return false;
    }

    if (OutAtlas.PageFiles.empty())
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] ParseSubUVJson failed at atlas page discovery. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return false;
    }

    if (!LoadAtlasTexture(Source, OutAtlas.PageFiles[0], OutAtlas.Atlas))
    {
        UE_LOG(
            Asset, ELogVerbosity::Warning,
            "[SubUVAtlasLoader] ParseSubUVJson failed at atlas texture load. Path=%s AtlasPage=%s",
            WidePathToUtf8(Source.NormalizedPath).c_str(), OutAtlas.PageFiles[0].c_str());
        OutAtlas.Reset();
        return false;
    }

    if (OutAtlas.Common.ScaleW == 0)
    {
        OutAtlas.Common.ScaleW = OutAtlas.Atlas.Width;
    }

    if (OutAtlas.Common.ScaleH == 0)
    {
        OutAtlas.Common.ScaleH = OutAtlas.Atlas.Height;
    }

    if (OutAtlas.Common.Pages == 0)
    {
        OutAtlas.Common.Pages = static_cast<uint32>(OutAtlas.PageFiles.size());
    }

    if (OutAtlas.Info.FrameCount == 0)
    {
        OutAtlas.Info.FrameCount = static_cast<uint32>(OutAtlas.Frames.size());
    }

    if (OutAtlas.Info.Columns == 0)
    {
        OutAtlas.Info.Columns = static_cast<uint32>(OutAtlas.Frames.size());
    }

    if (OutAtlas.Info.Rows == 0)
    {
        OutAtlas.Info.Rows = 1;
    }

    if (OutAtlas.Info.Name.empty())
    {
        OutAtlas.Info.Name = "Default";
    }

    return true;
}

bool FSubUVAtlasLoader::ParseMeta(const nlohmann::json& Root, FSubUVAtlasResource& OutAtlas) const
{
    if (!Root.contains("meta") || !Root["meta"].is_object())
    {
        return false;
    }

    const auto& Meta = Root["meta"];

    const FString ImageFile = Meta.value("image", "");
    if (ImageFile.empty())
    {
        return false;
    }

    OutAtlas.PageFiles.clear();
    OutAtlas.PageFiles.push_back(ImageFile);

    OutAtlas.Info.Name = GetStemName(ImageFile);
    OutAtlas.Info.Texture = ImageFile;
    OutAtlas.Info.FPS = 0.0f;
    OutAtlas.Info.bLoop = true;

    OutAtlas.Common.Pages = 1;
    OutAtlas.Common.bPacked = true;

    if (Meta.contains("size") && Meta["size"].is_object())
    {
        const auto& Size = Meta["size"];
        OutAtlas.Common.ScaleW = Size.value("w", 0u);
        OutAtlas.Common.ScaleH = Size.value("h", 0u);
    }

    return true;
}

bool FSubUVAtlasLoader::ParseFrames(const nlohmann::json& Root,
                                    TArray<FSubUVFrame>&  OutFrames) const
{
    if (!Root.contains("frames") || !Root["frames"].is_object())
    {
        return false;
    }

    OutFrames.clear();

    uint32 FrameId = 0;

    for (auto It = Root["frames"].begin(); It != Root["frames"].end(); ++It)
    {
        const FString         FrameName = It.key();
        const nlohmann::json& Entry = It.value();

        if (!Entry.is_object())
        {
            continue;
        }

        if (!Entry.contains("frame") || !Entry["frame"].is_object())
        {
            continue;
        }

        const auto& FrameRect = Entry["frame"];

        FSubUVFrame Frame;
        Frame.Id = FrameId++;
        Frame.X = FrameRect.value("x", 0u);
        Frame.Y = FrameRect.value("y", 0u);
        Frame.Width = FrameRect.value("w", 0u);
        Frame.Height = FrameRect.value("h", 0u);
        Frame.Duration = 0.0f;

        if (Entry.contains("pivot") && Entry["pivot"].is_object())
        {
            const auto& Pivot = Entry["pivot"];
            Frame.PivotX = Pivot.value("x", 0.5f);
            Frame.PivotY = Pivot.value("y", 0.5f);
        }
        else
        {
            Frame.PivotX = 0.5f;
            Frame.PivotY = 0.5f;
        }

        OutFrames.push_back(Frame);
    }

    return !OutFrames.empty();
}

bool FSubUVAtlasLoader::ParseSequences(const TArray<FSubUVFrame>&     Frames,
                                       TMap<FString, FSubUVSequence>& OutSequences) const
{
    OutSequences.clear();

    if (Frames.empty())
    {
        return false;
    }

    FSubUVSequence DefaultSequence;
    DefaultSequence.Name = "Default";
    DefaultSequence.StartFrame = 0;
    DefaultSequence.EndFrame = static_cast<uint32>(Frames.size() - 1);
    DefaultSequence.bLoop = true;

    OutSequences.insert_or_assign(DefaultSequence.Name, DefaultSequence);
    return true;
}

bool FSubUVAtlasLoader::LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile,
                                         FTextureResource& OutAtlas) const
{
    const FWString AtlasPath = ResolveSiblingPath(JsonSource.NormalizedPath, PageFile);
    if (AtlasPath.empty())
    {
        return false;
    }

    const std::shared_ptr<FDecodedAtlasImage> DecodedImage = GetOrDecodeAtlas(AtlasPath);
    if (!DecodedImage)
    {
        return false;
    }

    const FSourceRecord* AtlasSource = AtlasSourceCache.GetOrLoad(AtlasPath);
    if (!AtlasSource)
    {
        return false;
    }

    const std::shared_ptr<FTextureResource> AtlasResource =
        GetOrCreateAtlasResource(*AtlasSource, *DecodedImage);
    if (!AtlasResource)
    {
        return false;
    }

    OutAtlas = *AtlasResource;
    return true;
}

std::shared_ptr<FSubUVAtlasLoader::FDecodedAtlasImage>
FSubUVAtlasLoader::GetOrDecodeAtlas(const FWString& AtlasPath) const
{
    const FSourceRecord* AtlasSource = AtlasSourceCache.GetOrLoad(AtlasPath);
    if (!AtlasSource || AtlasSource->SourceHash.empty())
    {
        return nullptr;
    }

    auto It = DecodeCache.find(AtlasSource->SourceHash);
    if (It != DecodeCache.end())
    {
        return It->second;
    }

    std::shared_ptr<FDecodedAtlasImage> DecodedImage = std::make_shared<FDecodedAtlasImage>();
    if (!DecodeWithWIC(*AtlasSource, *DecodedImage))
    {
        return nullptr;
    }

    auto [InsertedIt, _] =
        DecodeCache.insert_or_assign(AtlasSource->SourceHash, std::move(DecodedImage));
    return InsertedIt->second;
}

std::shared_ptr<FTextureResource>
FSubUVAtlasLoader::GetOrCreateAtlasResource(const FSourceRecord&      AtlasSource,
                                            const FDecodedAtlasImage& DecodedImage) const
{
    if (AtlasSource.SourceHash.empty())
    {
        return nullptr;
    }

    auto It = ResourceCache.find(AtlasSource.SourceHash);
    if (It != ResourceCache.end())
    {
        return It->second;
    }

    std::shared_ptr<FTextureResource> Resource = std::make_shared<FTextureResource>();
    if (!CreateTextureResource(DecodedImage, *Resource))
    {
        return nullptr;
    }

    auto [InsertedIt, _] =
        ResourceCache.insert_or_assign(AtlasSource.SourceHash, std::move(Resource));
    return InsertedIt->second;
}

bool FSubUVAtlasLoader::DecodeWithWIC(const FSourceRecord& AtlasSource,
                                      FDecodedAtlasImage&  OutImage) const
{
    if (!AtlasSource.bFileBytesLoaded)
    {
        return false;
    }

    OutImage = {};
    return DecodeWithWICBytes(AtlasSource.FileBytes, OutImage.Width, OutImage.Height,
                              OutImage.Pixels);
}

bool FSubUVAtlasLoader::CreateTextureResource(const FDecodedAtlasImage& DecodedImage,
                                              FTextureResource&         OutAtlas) const
{
    if (!RHI || !RHI->GetDevice())
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] CreateTextureResource failed at D3D device validation.");
        return false;
    }

    if (DecodedImage.Width == 0 || DecodedImage.Height == 0 || DecodedImage.Pixels.empty())
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] CreateTextureResource failed at decoded atlas validation.");
        return false;
    }

    if (DecodedImage.Width > (std::numeric_limits<UINT>::max() / 4u))
    {
        return false;
    }

    const UINT RowPitch = DecodedImage.Width * 4u;

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = DecodedImage.Width;
    TextureDesc.Height = DecodedImage.Height;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = DecodedImage.Pixels.data();
    InitialData.SysMemPitch = RowPitch;

    TComPtr<ID3D11Texture2D> Texture;
    HRESULT Hr = RHI->GetDevice()->CreateTexture2D(&TextureDesc, &InitialData, &Texture);
    if (FAILED(Hr))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] CreateTextureResource failed at CreateTexture2D. HRESULT=0x%08x",
               static_cast<uint32>(Hr));
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;

    TComPtr<ID3D11ShaderResourceView> SRV;
    Hr = RHI->GetDevice()->CreateShaderResourceView(Texture.Get(), &SRVDesc, &SRV);
    if (FAILED(Hr))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[SubUVAtlasLoader] CreateTextureResource failed at CreateShaderResourceView. "
               "HRESULT=0x%08x",
               static_cast<uint32>(Hr));
        return false;
    }

    OutAtlas = {};
    OutAtlas.Width = DecodedImage.Width;
    OutAtlas.Height = DecodedImage.Height;
    OutAtlas.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    OutAtlas.Texture = std::move(Texture);
    OutAtlas.SRV = std::move(SRV);
    OutAtlas.TextureAllocationHandle = GMemoryTracker.TrackTextureAllocation(
        GMemoryTracker.EstimateTexture2DSizeBytes(TextureDesc));
    return true;
}

FWString FSubUVAtlasLoader::ResolveSiblingPath(const FWString& BaseFilePath,
                                               const FString&  RelativePath) const
{
    if (BaseFilePath.empty() || RelativePath.empty())
    {
        return {};
    }

    namespace fs = std::filesystem;

    const fs::path BasePath(BaseFilePath);
    const fs::path Relative(RelativePath);
    const fs::path CombinedPath =
        Relative.is_absolute() ? Relative : (BasePath.parent_path() / Relative);

    std::error_code ErrorCode;
    fs::path        Normalized = fs::weakly_canonical(CombinedPath, ErrorCode);
    if (ErrorCode)
    {
        Normalized = CombinedPath.lexically_normal();
    }

    return Normalized.native();
}
