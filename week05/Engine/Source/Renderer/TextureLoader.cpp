#include "Renderer/TextureLoader.h"

#include "Core/Paths.h"
#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb_image.h"

bool FTextureLoader::CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV)
{
	if (FilePath == nullptr)
	{
		return false;
	}

	return CreateTextureFromSTB(Device, FPaths::ToPath(FilePath), OutSRV);
}

bool FTextureLoader::CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV)
{
	if (Device == nullptr || OutSRV == nullptr || FilePath.empty())
	{
		return false;
	}

	std::ifstream File(FilePath, std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		return false;
	}

	const std::streamsize FileSize = File.tellg();
	if (FileSize <= 0)
	{
		return false;
	}

	File.seekg(0, std::ios::beg);
	std::vector<unsigned char> FileBytes(static_cast<size_t>(FileSize));
	if (!File.read(reinterpret_cast<char*>(FileBytes.data()), FileSize))
	{
		return false;
	}

	int W = 0;
	int H = 0;
	int C = 0;
	unsigned char* Data = stbi_load_from_memory(FileBytes.data(), static_cast<int>(FileBytes.size()), &W, &H, &C, 4);
	if (!Data)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = W;
	Desc.Height = H;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = { Data, static_cast<UINT>(W * 4), 0 };
	ID3D11Texture2D* Texture = nullptr;
	const HRESULT Hr = Device->CreateTexture2D(&Desc, &InitData, &Texture);
	stbi_image_free(Data);
	if (FAILED(Hr) || !Texture)
	{
		return false;
	}

	const HRESULT SRVHr = Device->CreateShaderResourceView(Texture, nullptr, OutSRV);
	Texture->Release();
	return SUCCEEDED(SRVHr);
}
