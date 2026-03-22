#include "Core/CoreMinimal.h"
#include "TextureLoader.h"

#include "Renderer/RenderAsset/FTextureResource.h"
#include "Texture2DAsset.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <objbase.h>
#include <wincodec.h>

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

	FTextureBuildSettings MakeTextureBuildSettings(const FAssetLoadParams& Params)
	{
		return Params.TextureSettings;
	}

	uint64 HashTextureBuildSettings(const FTextureBuildSettings& Settings)
	{
		uint64 Result = 14695981039346656037ull;

		auto Combine = [&Result](uint64 Value)
		{
			Result ^= Value + 0x9e3779b97f4a7c15ull + (Result << 6) + (Result >> 2);
		};

		Combine(static_cast<uint64>(Settings.bSRGB));
		Combine(static_cast<uint64>(Settings.bGenerateMips));
		Combine(static_cast<uint64>(Settings.bIsNormalMap));
		Combine(static_cast<uint64>(Settings.Format));

		return Result;
	}

	DXGI_FORMAT ResolveTextureFormat(const FTextureBuildSettings& Settings)
	{
		switch (Settings.Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return Settings.bSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return Settings.bSRGB ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
		default:
			return Settings.Format;
		}
	}
}

FTextureLoader::FTextureLoader(FD3D11DynamicRHI* InRHI)
	: RHI(InRHI)
{
}

bool FTextureLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
	if (Path.empty())
	{
		return false;
	}

	if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Texture)
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

	return Extension == L".png"
		|| Extension == L".jpg"
		|| Extension == L".jpeg"
		|| Extension == L".bmp";
}

EAssetType FTextureLoader::GetAssetType() const
{
	return EAssetType::Texture;
}

uint64 FTextureLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
	const FTextureBuildSettings Settings = MakeTextureBuildSettings(Params);
	return HashTextureBuildSettings(Settings);
}

UAsset* FTextureLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
	const std::shared_ptr<FDecodedImage> DecodedImage = GetOrDecode(Source);
	if (!DecodedImage)
	{
		return nullptr;
	}

	const FTextureBuildSettings Settings = MakeTextureBuildSettings(Params);
	const std::shared_ptr<FTextureResource> Resource = GetOrCreateResource(Source.SourceHash, *DecodedImage, Settings);
	if (!Resource)
	{
		return nullptr;
	}

	UTexture2DAsset* TextureAsset = new UTexture2DAsset();
	TextureAsset->Initialize(Source, Settings, Resource, DecodedImage->Width, DecodedImage->Height);

	return TextureAsset;
}

std::shared_ptr<FDecodedImage> FTextureLoader::GetOrDecode(const FSourceRecord& Source)
{
	if (Source.SourceHash.empty())
	{
		return nullptr;
	}

	auto It = DecodeCache.find(Source.SourceHash);
	if (It != DecodeCache.end())
	{
		return It->second;
	}

	std::shared_ptr<FDecodedImage> DecodedImage = std::make_shared<FDecodedImage>();
	if (!DecodeWithWIC(Source, *DecodedImage))
	{
		return nullptr;
	}

	auto [InsertedIt, _] = DecodeCache.insert_or_assign(Source.SourceHash, std::move(DecodedImage));
	return InsertedIt->second;
}

std::shared_ptr<FTextureResource> FTextureLoader::GetOrCreateResource(const FString& SourceHash, const FDecodedImage& Image,
                                                                      const FTextureBuildSettings& Settings)
{
	if (SourceHash.empty())
	{
		return nullptr;
	}

	const FTextureBuildKey Key{ SourceHash, Settings };

	auto It = ResourceCache.find(Key);
	if (It != ResourceCache.end())
	{
		return It->second;
	}

	std::shared_ptr<FTextureResource> Resource = std::make_shared<FTextureResource>();
	if (!CreateTextureResource(Image, Settings, *Resource))
	{
		return nullptr;
	}

	auto [InsertedIt, _] = ResourceCache.insert_or_assign(Key, std::move(Resource));
	return InsertedIt->second;
}

bool FTextureLoader::DecodeWithWIC(const FSourceRecord& Source, FDecodedImage& OutImage)
{
	if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
	{
		return false;
	}

	if (Source.FileBytes.size() > static_cast<size_t>(std::numeric_limits<DWORD>::max()))
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
		reinterpret_cast<WICInProcPointer>(const_cast<uint8*>(Source.FileBytes.data())),
		static_cast<DWORD>(Source.FileBytes.size()));
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

	OutImage = {};
	OutImage.Width = static_cast<uint32>(Width);
	OutImage.Height = static_cast<uint32>(Height);
	OutImage.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	OutImage.Pixels.resize(static_cast<size_t>(BufferSize));

	Hr = FormatConverter->CopyPixels(
		nullptr,
		static_cast<UINT>(Stride),
		static_cast<UINT>(BufferSize),
		reinterpret_cast<BYTE*>(OutImage.Pixels.data()));
	if (FAILED(Hr))
	{
		OutImage = {};
		return false;
	}

	return true;
}

bool FTextureLoader::CreateTextureResource(const FDecodedImage& Image, const FTextureBuildSettings& Settings,
                                           FTextureResource& OutResource)
{
	if (!RHI || !RHI->GetDevice())
	{
		return false;
	}

	if (Image.Width == 0 || Image.Height == 0 || Image.Pixels.empty())
	{
		return false;
	}

	const DXGI_FORMAT TextureFormat = ResolveTextureFormat(Settings);
	if (TextureFormat != DXGI_FORMAT_R8G8B8A8_UNORM
		&& TextureFormat != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		return false;
	}

	const UINT RowPitch = Image.Width * 4u;

	TComPtr<ID3D11Texture2D> Texture;
	TComPtr<ID3D11ShaderResourceView> SRV;
	HRESULT Hr = S_OK;

	if (Settings.bGenerateMips)
	{
		if (!RHI->GetDeviceContext())
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC TextureDesc = {};
		TextureDesc.Width = Image.Width;
		TextureDesc.Height = Image.Height;
		TextureDesc.MipLevels = 0;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = TextureFormat;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		Hr = RHI->GetDevice()->CreateTexture2D(&TextureDesc, nullptr, &Texture);
		if (FAILED(Hr))
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = TextureFormat;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

		Hr = RHI->GetDevice()->CreateShaderResourceView(Texture.Get(), &SRVDesc, &SRV);
		if (FAILED(Hr))
		{
			return false;
		}

		RHI->GetDeviceContext()->UpdateSubresource(Texture.Get(), 0, nullptr, Image.Pixels.data(), RowPitch, 0);
		RHI->GetDeviceContext()->GenerateMips(SRV.Get());
	}
	else
	{
		D3D11_TEXTURE2D_DESC TextureDesc = {};
		TextureDesc.Width = Image.Width;
		TextureDesc.Height = Image.Height;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = TextureFormat;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
		TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA InitialData = {};
		InitialData.pSysMem = Image.Pixels.data();
		InitialData.SysMemPitch = RowPitch;

		Hr = RHI->GetDevice()->CreateTexture2D(&TextureDesc, &InitialData, &Texture);
		if (FAILED(Hr))
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = TextureFormat;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = 1;

		Hr = RHI->GetDevice()->CreateShaderResourceView(Texture.Get(), &SRVDesc, &SRV);
		if (FAILED(Hr))
		{
			return false;
		}
	}

	OutResource = {};
	OutResource.Width = Image.Width;
	OutResource.Height = Image.Height;
	OutResource.Format = TextureFormat;
	OutResource.Texture = std::move(Texture);
	OutResource.SRV = std::move(SRV);

	return true;
}
