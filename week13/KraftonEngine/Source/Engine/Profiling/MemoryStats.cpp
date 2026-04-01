#include "MemoryStats.h"

#include <d3d11.h>
#include <algorithm>

namespace
{
	uint64 GetBitsPerPixel(DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return 128;

		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 96;

		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_Y416:
		case DXGI_FORMAT_Y210:
		case DXGI_FORMAT_Y216:
			return 64;

		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_AYUV:
		case DXGI_FORMAT_Y410:
		case DXGI_FORMAT_P010:
		case DXGI_FORMAT_P016:
		case DXGI_FORMAT_V408:
			return 32;

		case DXGI_FORMAT_P208:
		case DXGI_FORMAT_V208:
			return 16;

		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_A8P8:
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			return 16;

		case DXGI_FORMAT_NV12:
		case DXGI_FORMAT_420_OPAQUE:
		case DXGI_FORMAT_NV11:
			return 12;

		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_AI44:
		case DXGI_FORMAT_IA44:
		case DXGI_FORMAT_P8:
			return 8;

		case DXGI_FORMAT_R1_UNORM:
			return 1;

		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			return 4;

		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return 8;

		default:
			return 0;
		}
	}

	uint64 GetSurfaceSize(uint32 Width, uint32 Height, DXGI_FORMAT Format)
	{
		if (Width == 0 || Height == 0)
		{
			return 0;
		}

		switch (Format)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		{
			const uint64 NumBlocksWide = std::max<uint64>(1, (static_cast<uint64>(Width) + 3) / 4);
			const uint64 NumBlocksHigh = std::max<uint64>(1, (static_cast<uint64>(Height) + 3) / 4);
			return NumBlocksWide * NumBlocksHigh * 8;
		}

		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		{
			const uint64 NumBlocksWide = std::max<uint64>(1, (static_cast<uint64>(Width) + 3) / 4);
			const uint64 NumBlocksHigh = std::max<uint64>(1, (static_cast<uint64>(Height) + 3) / 4);
			return NumBlocksWide * NumBlocksHigh * 16;
		}

		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
			return ((static_cast<uint64>(Width) + 1) >> 1) * 4 * static_cast<uint64>(Height);

		case DXGI_FORMAT_NV12:
		case DXGI_FORMAT_420_OPAQUE:
			return ((static_cast<uint64>(Width) + 1) >> 1) * ((static_cast<uint64>(Height) + 1) >> 1) * 6;

		case DXGI_FORMAT_P010:
		case DXGI_FORMAT_P016:
			return ((static_cast<uint64>(Width) + 1) >> 1) * ((static_cast<uint64>(Height) + 1) >> 1) * 12;

		case DXGI_FORMAT_NV11:
			return (((static_cast<uint64>(Width) + 3) >> 2) * 4) * static_cast<uint64>(Height) * 2;

		default:
		{
			const uint64 BitsPerPixel = GetBitsPerPixel(Format);
			return (static_cast<uint64>(Width) * static_cast<uint64>(Height) * BitsPerPixel + 7) / 8;
		}
		}
	}

	uint32 GetMipCount(uint32 MipLevels, uint32 Width, uint32 Height, uint32 Depth)
	{
		if (MipLevels != 0)
		{
			return MipLevels;
		}

		uint32 Count = 1;
		while (Width > 1 || Height > 1 || Depth > 1)
		{
			Width = std::max<uint32>(1, Width >> 1);
			Height = std::max<uint32>(1, Height >> 1);
			Depth = std::max<uint32>(1, Depth >> 1);
			++Count;
		}
		return Count;
	}

	uint64 GetTexture1DSize(const D3D11_TEXTURE1D_DESC& Desc)
	{
		const uint32 MipCount = GetMipCount(Desc.MipLevels, Desc.Width, 1, 1);
		const uint64 BitsPerPixel = GetBitsPerPixel(Desc.Format);
		uint64 TotalBytes = 0;
		uint32 Width = Desc.Width;

		for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			const uint64 MipBytes = (static_cast<uint64>(Width) * BitsPerPixel + 7) / 8;
			TotalBytes += MipBytes * static_cast<uint64>(Desc.ArraySize);
			Width = std::max<uint32>(1, Width >> 1);
		}

		return TotalBytes;
	}

	uint64 GetTexture2DSize(const D3D11_TEXTURE2D_DESC& Desc)
	{
		const uint32 MipCount = GetMipCount(Desc.MipLevels, Desc.Width, Desc.Height, 1);
		const uint64 SampleCount = std::max<uint32>(1, Desc.SampleDesc.Count);
		uint64 TotalBytes = 0;
		uint32 Width = Desc.Width;
		uint32 Height = Desc.Height;

		for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			TotalBytes += GetSurfaceSize(Width, Height, Desc.Format) * static_cast<uint64>(Desc.ArraySize) * SampleCount;
			Width = std::max<uint32>(1, Width >> 1);
			Height = std::max<uint32>(1, Height >> 1);
		}

		return TotalBytes;
	}

	uint64 GetTexture3DSize(const D3D11_TEXTURE3D_DESC& Desc)
	{
		const uint32 MipCount = GetMipCount(Desc.MipLevels, Desc.Width, Desc.Height, Desc.Depth);
		uint64 TotalBytes = 0;
		uint32 Width = Desc.Width;
		uint32 Height = Desc.Height;
		uint32 Depth = Desc.Depth;

		for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
		{
			TotalBytes += GetSurfaceSize(Width, Height, Desc.Format) * static_cast<uint64>(Depth);
			Width = std::max<uint32>(1, Width >> 1);
			Height = std::max<uint32>(1, Height >> 1);
			Depth = std::max<uint32>(1, Depth >> 1);
		}

		return TotalBytes;
	}
}

uint64 MemoryStats::TotalAllocationBytes = 0;
uint32 MemoryStats::TotalAllocationCount = 0;
uint64 MemoryStats::PixelShaderMemory = 0;
uint64 MemoryStats::VertexShaderMemory = 0;
uint64 MemoryStats::TextureMemory = 0;
uint64 MemoryStats::VertexBufferMemory = 0;
uint64 MemoryStats::IndexBufferMemory = 0;
uint64 MemoryStats::StaticMeshCPUMemory = 0;

uint64 MemoryStats::CalculateTextureMemory(ID3D11Resource* Resource)
{
	if (!Resource)
	{
		return 0;
	}

	D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	Resource->GetType(&Dimension);

	switch (Dimension)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
	{
		ID3D11Texture1D* Texture = nullptr;
		if (FAILED(Resource->QueryInterface(__uuidof(ID3D11Texture1D), reinterpret_cast<void**>(&Texture))) || !Texture)
		{
			return 0;
		}

		D3D11_TEXTURE1D_DESC Desc = {};
		Texture->GetDesc(&Desc);
		Texture->Release();
		return GetTexture1DSize(Desc);
	}

	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
	{
		ID3D11Texture2D* Texture = nullptr;
		if (FAILED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture))) || !Texture)
		{
			return 0;
		}

		D3D11_TEXTURE2D_DESC Desc = {};
		Texture->GetDesc(&Desc);
		Texture->Release();
		return GetTexture2DSize(Desc);
	}

	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
	{
		ID3D11Texture3D* Texture = nullptr;
		if (FAILED(Resource->QueryInterface(__uuidof(ID3D11Texture3D), reinterpret_cast<void**>(&Texture))) || !Texture)
		{
			return 0;
		}

		D3D11_TEXTURE3D_DESC Desc = {};
		Texture->GetDesc(&Desc);
		Texture->Release();
		return GetTexture3DSize(Desc);
	}

	default:
		return 0;
	}
}
