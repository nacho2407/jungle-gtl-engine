#include "Core/CoreMinimal.h"
#include "Core/CoreMinimal.h"
#include "FontAtlasLoader.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <objbase.h>
#include <wincodec.h>

#include "FontAsset.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"

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

		bool IsUsable() const
		{
			return bUsable;
		}

	private:
		HRESULT Result = E_FAIL;
		bool bInitialized = false;
		bool bUsable = false;
	};

	bool DecodeWithWICBytes(const TArray<uint8>& Bytes, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutPixels)
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
		HRESULT Hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
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
		Hr = ImagingFactory->CreateDecoderFromStream(Stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &Decoder);
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
		if (Stride > static_cast<uint64>(std::numeric_limits<UINT>::max())
			|| BufferSize > static_cast<uint64>(std::numeric_limits<UINT>::max())
			|| BufferSize > static_cast<uint64>(std::numeric_limits<size_t>::max()))
		{
			return false;
		}

		TComPtr<IWICFormatConverter> FormatConverter;
		Hr = ImagingFactory->CreateFormatConverter(&FormatConverter);
		if (FAILED(Hr))
		{
			return false;
		}

		Hr = FormatConverter->Initialize(
			Frame.Get(),
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(Hr))
		{
			return false;
		}

		OutWidth = static_cast<uint32>(Width);
		OutHeight = static_cast<uint32>(Height);
		OutPixels.resize(static_cast<size_t>(BufferSize));

		Hr = FormatConverter->CopyPixels(
			nullptr,
			static_cast<UINT>(Stride),
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
}

FFontAtlasLoader::FFontAtlasLoader(FD3D11DynamicRHI* InRHI)
	: RHI(InRHI)
{
}

bool FFontAtlasLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
	if (Path.empty())
	{
		return false;
	}

	if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Font)
	{
		return false;
	}

	const std::filesystem::path FilePath(Path);
	FWString Extension = FilePath.extension().native();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](wchar_t Ch)
		{
			return static_cast<wchar_t>(std::towlower(Ch));
		});

	return Extension == L".json";
}

EAssetType FFontAtlasLoader::GetAssetType() const
{
	return EAssetType::Font;
}

uint64 FFontAtlasLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
	(void)Params;

	uint64 Hash = 14695981039346656037ull;
	Hash ^= static_cast<uint64>(GetAssetType());
	Hash *= 1099511628211ull;
	return Hash;
}

UAsset* FFontAtlasLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
	(void)Params;

	FFontResource FontResource;
	if (!ParseFontAtlasJson(Source, FontResource))
	{
		return nullptr;
	}

	UFontAsset* NewFontAsset = new UFontAsset();
	NewFontAsset->Initialize(Source, FontResource);
	return NewFontAsset;
}

bool FFontAtlasLoader::ParseFontAtlasJson(const FSourceRecord& Source, FFontResource& OutFont) const
{
	OutFont.Reset();

	if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
	{
		return false;
	}

	const nlohmann::json Root = nlohmann::json::parse(Source.FileBytes.begin(), Source.FileBytes.end(), nullptr, false);
	if (Root.is_discarded())
	{
		return false;
	}

	if (!ParseInfo(Root, OutFont.Info)
		|| !ParseCommon(Root, OutFont.Common)
		|| !ParsePages(Root, OutFont.PageFiles)
		|| !ParseChars(Root, OutFont.Glyphs))
	{
		return false;
	}

	if (OutFont.PageFiles.empty())
	{
		return false;
	}

	if (!LoadAtlasTexture(Source, OutFont.PageFiles[0], OutFont.Atlas))
	{
		OutFont.Reset();
		return false;
	}

	if (OutFont.Common.ScaleW == 0)
	{
		OutFont.Common.ScaleW = OutFont.Atlas.Width;
	}

	if (OutFont.Common.ScaleH == 0)
	{
		OutFont.Common.ScaleH = OutFont.Atlas.Height;
	}

	if (OutFont.Common.Pages == 0)
	{
		OutFont.Common.Pages = static_cast<uint32>(OutFont.PageFiles.size());
	}

	return true;
}

bool FFontAtlasLoader::ParseInfo(const nlohmann::json& Root, FFontInfo& OutInfo) const
{
	if (!Root.contains("info") || !Root["info"].is_object())
	{
		return false;
	}

	const auto& Info = Root["info"];

	OutInfo.Face = Info.value("face", "");
	OutInfo.Size = Info.value("size", 0);
	OutInfo.bBold = Info.value("bold", 0) != 0;
	OutInfo.bItalic = Info.value("italic", 0) != 0;
	OutInfo.bUnicode = Info.value("unicode", 0) != 0;

	OutInfo.StretchH = Info.value("stretchH", 100);
	OutInfo.bSmooth = Info.value("smooth", 1) != 0;
	OutInfo.AA = Info.value("aa", 1);
	OutInfo.Outline = Info.value("outline", 0);

	if (Info.contains("padding") && Info["padding"].is_array() && Info["padding"].size() == 4)
	{
		OutInfo.PaddingLeft = Info["padding"][0].get<int32>();
		OutInfo.PaddingTop = Info["padding"][1].get<int32>();
		OutInfo.PaddingRight = Info["padding"][2].get<int32>();
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
	if (!Root.contains("common") || !Root["common"].is_object())
	{
		return false;
	}

	const auto& Common = Root["common"];

	OutCommon.LineHeight = Common.value("lineHeight", 0u);
	OutCommon.Base = Common.value("base", 0u);
	OutCommon.ScaleW = Common.value("scaleW", 0u);
	OutCommon.ScaleH = Common.value("scaleH", 0u);
	OutCommon.Pages = Common.value("pages", 0u);
	OutCommon.bPacked = Common.value("packed", 0) != 0;

	OutCommon.AlphaChannel = Common.value("alphaChnl", 0u);
	OutCommon.RedChannel = Common.value("redChnl", 0u);
	OutCommon.GreenChannel = Common.value("greenChnl", 0u);
	OutCommon.BlueChannel = Common.value("blueChnl", 0u);

	return true;
}

bool FFontAtlasLoader::ParsePages(const nlohmann::json& Root, TArray<FString>& OutPages) const
{
	if (!Root.contains("pages") || !Root["pages"].is_array())
	{
		return false;
	}

	OutPages.clear();
	for (const auto& Page : Root["pages"])
	{
		if (Page.is_string())
		{
			OutPages.push_back(Page.get<FString>());
			continue;
		}

		if (Page.is_object() && Page.contains("file") && Page["file"].is_string())
		{
			OutPages.push_back(Page["file"].get<FString>());
		}
	}

	return !OutPages.empty();
}

bool FFontAtlasLoader::ParseChars(const nlohmann::json& Root, TMap<uint32, FFontGlyph>& OutGlyphs) const
{
	if (!Root.contains("chars") || !Root["chars"].is_array())
	{
		return false;
	}

	OutGlyphs.clear();
	for (const auto& Ch : Root["chars"])
	{
		FFontGlyph Glyph;
		Glyph.Id = Ch.value("id", 0u);
		Glyph.X = Ch.value("x", 0u);
		Glyph.Y = Ch.value("y", 0u);
		Glyph.Width = Ch.value("width", 0u);
		Glyph.Height = Ch.value("height", 0u);
		Glyph.XOffset = Ch.value("xoffset", 0);
		Glyph.YOffset = Ch.value("yoffset", 0);
		Glyph.XAdvance = Ch.value("xadvance", 0);
		Glyph.Page = Ch.value("page", 0u);
		Glyph.Channel = Ch.value("chnl", 0u);

		OutGlyphs.insert_or_assign(Glyph.Id, Glyph);
	}

	return !OutGlyphs.empty();
}

bool FFontAtlasLoader::LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile,
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

	const std::shared_ptr<FTextureResource> AtlasResource = GetOrCreateAtlasResource(*AtlasSource, *DecodedImage);
	if (!AtlasResource)
	{
		return false;
	}

	OutAtlas = *AtlasResource;
	return true;
}

std::shared_ptr<FFontAtlasLoader::FDecodedAtlasImage> FFontAtlasLoader::GetOrDecodeAtlas(const FWString& AtlasPath) const
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

	auto [InsertedIt, _] = DecodeCache.insert_or_assign(AtlasSource->SourceHash, std::move(DecodedImage));
	return InsertedIt->second;
}

std::shared_ptr<FTextureResource> FFontAtlasLoader::GetOrCreateAtlasResource(const FSourceRecord& AtlasSource,
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

	auto [InsertedIt, _] = ResourceCache.insert_or_assign(AtlasSource.SourceHash, std::move(Resource));
	return InsertedIt->second;
}

bool FFontAtlasLoader::DecodeWithWIC(const FSourceRecord& AtlasSource, FDecodedAtlasImage& OutImage) const
{
	if (!AtlasSource.bFileBytesLoaded)
	{
		return false;
	}

	OutImage = {};
	return DecodeWithWICBytes(AtlasSource.FileBytes, OutImage.Width, OutImage.Height, OutImage.Pixels);
}

bool FFontAtlasLoader::CreateTextureResource(const FDecodedAtlasImage& DecodedImage,
	FTextureResource& OutAtlas) const
{
	if (!RHI || !RHI->GetDevice())
	{
		return false;
	}

	if (DecodedImage.Width == 0 || DecodedImage.Height == 0 || DecodedImage.Pixels.empty())
	{
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
		return false;
	}

	OutAtlas = {};
	OutAtlas.Width = DecodedImage.Width;
	OutAtlas.Height = DecodedImage.Height;
	OutAtlas.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	OutAtlas.Texture = std::move(Texture);
	OutAtlas.SRV = std::move(SRV);
	return true;
}

FWString FFontAtlasLoader::ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const
{
	if (BaseFilePath.empty() || RelativePath.empty())
	{
		return {};
	}

	namespace fs = std::filesystem;

	const fs::path BasePath(BaseFilePath);
	const fs::path Relative(RelativePath);
	const fs::path CombinedPath = Relative.is_absolute() ? Relative : (BasePath.parent_path() / Relative);

	std::error_code ErrorCode;
	fs::path Normalized = fs::weakly_canonical(CombinedPath, ErrorCode);
	if (ErrorCode)
	{
		Normalized = CombinedPath.lexically_normal();
	}

	return Normalized.native();
}
