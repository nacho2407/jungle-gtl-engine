#include "ObjComponent.h"
#include "Primitive/PrimitiveObj.h"
#include "Object/Class.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Material.h"
#include "Renderer/ShaderMap.h"
#include "Core/Paths.h"

#include "ThirdParty/stb_image.h"
#include <d3d11.h>

IMPLEMENT_RTTI(UObjComponent, UPrimitiveComponent)

void UObjComponent::Initialize()
{ 
}

void UObjComponent::LoadPrimitive(const FString& FilePath)
{
	Primitive = std::make_unique<CPrimitiveObj>(FilePath);	
}

void UObjComponent::LoadTexture(ID3D11Device* Device, const FString& FilePath)
{
	if (DynamicMaterialOwner)
	{
		Material = DynamicMaterialOwner.get();
	}
	else
	{
		std::shared_ptr<FMaterial> DefaultMaterial = FMaterialManager::Get().FindByName("M_Default_Texture");
		/** 임시 객체 저장용 */
		DynamicMaterialOwner = DefaultMaterial->CreateDynamicMaterial();
		/** dangling pointer 위험성에 대해 인지 필요 (시간상...) */
		Material = DynamicMaterialOwner.get();
	}

	/** 텍스쳐 로드 */
	int width = 0, height = 0, channels = 0;

	unsigned char* data = stbi_load(
		FPaths::ToAbsolutePath(FilePath).c_str(),
		&width,
		&height,
		&channels,
		STBI_rgb_alpha // 강제 RGBA
	);

	if (!data)
	{
		// TODO: fallback texture
		return;
	}

	ID3D11Texture2D* texture = nullptr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ⭐ diffuse면 SRGB 추천
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data;
	initData.SysMemPitch = width * 4;

	HRESULT hr = Device->CreateTexture2D(&desc, &initData, &texture);

	if (FAILED(hr))
	{
		stbi_image_free(data);
		return;
	}

	ID3D11ShaderResourceView* srv = nullptr;

	hr = Device->CreateShaderResourceView(texture, nullptr, &srv);

	// Texture는 SRV 만들었으면 바로 버려도 됨
	texture->Release();

	if (FAILED(hr))
	{
		stbi_image_free(data);
		return;
	}

	stbi_image_free(data);

	std::shared_ptr<FMaterialTexture> MT = std::make_shared<FMaterialTexture>();
	MT->TextureSRV = srv;
	Material->SetMaterialTexture(MT);
}
