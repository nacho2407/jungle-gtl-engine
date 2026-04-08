#include "BillboardRenderer.h"

#include <WICTextureLoader.h>
#include <algorithm>
#include <filesystem>

#include "Component/BillboardComponent.h"
#include "Core/Paths.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderMap.h"

FBillboardRenderer::~FBillboardRenderer()
{
	Release();
}

bool FBillboardRenderer::Initialize(FRenderer& InRenderer)
{
	Release();

	Device = InRenderer.GetDevice();
	DeviceContext = InRenderer.GetDeviceContext();
	if (!Device || !DeviceContext)
	{
		return false;
	}

	const std::filesystem::path IconDirectory = FPaths::IconDir();
	if (std::filesystem::exists(IconDirectory))
	{
		for (const auto& Entry : std::filesystem::directory_iterator(IconDirectory))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path Extension = Entry.path().extension();
			if (Extension == L".png" || Extension == L".dds")
			{
				GetOrLoadTexture(Entry.path().wstring());
			}
		}
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	const std::wstring VSPath = ShaderDir + L"SubUVVertexShader.hlsl";
	const std::wstring PSPath = ShaderDir + L"SubUVPixelShader.hlsl";

	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	BillboardMaterial = std::make_shared<FMaterial>();
	BillboardMaterial->SetOriginName("M_Billboard");
	BillboardMaterial->SetVertexShader(VS);
	BillboardMaterial->SetPixelShader(PS);

	FRasterizerStateOption RasterizerOption;
	RasterizerOption.FillMode = D3D11_FILL_SOLID;
	RasterizerOption.CullMode = D3D11_CULL_NONE;
	BillboardMaterial->SetRasterizerOption(RasterizerOption);
	BillboardMaterial->SetRasterizerState(InRenderer.GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption));

	FDepthStencilStateOption DepthOption;
	DepthOption.DepthEnable = true;
	DepthOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	BillboardMaterial->SetDepthStencilOption(DepthOption);
	BillboardMaterial->SetDepthStencilState(InRenderer.GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOption));

	FBlendStateOption BlendOption;
	BlendOption.BlendEnable = true;
	BlendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOp = D3D11_BLEND_OP_ADD;
	BlendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BillboardMaterial->SetBlendOption(BlendOption);
	BillboardMaterial->SetBlendState(InRenderer.GetRenderStateManager()->GetOrCreateBlendState(BlendOption));

	const int32 SlotIndex = BillboardMaterial->CreateConstantBuffer(Device, 16);
	if (SlotIndex >= 0)
	{
		BillboardMaterial->RegisterParameter("CellSize", SlotIndex, 0, sizeof(FVector2));
		BillboardMaterial->RegisterParameter("UVOffset", SlotIndex, sizeof(FVector2), sizeof(FVector2));
	}

	return true;
}

void FBillboardRenderer::Release()
{
	MaterialsByComponent.clear();
	TextureCache.clear();
	BillboardMaterial.reset();
	Device = nullptr;
	DeviceContext = nullptr;
}

bool FBillboardRenderer::BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const
{
	OutMesh.Vertices.clear();
	OutMesh.Indices.clear();
	OutMesh.Topology = EMeshTopology::EMT_TriangleList;

	const float HalfW = Size.X * 0.5f;
	const float HalfH = Size.Y * 0.5f;

	FVertex V0, V1, V2, V3;
	V0.Position = FVector(0.0f, -HalfW, HalfH);  V0.UV = FVector2(0.0f, 0.0f);
	V1.Position = FVector(0.0f, HalfW, HalfH);   V1.UV = FVector2(1.0f, 0.0f);
	V2.Position = FVector(0.0f, HalfW, -HalfH);  V2.UV = FVector2(1.0f, 1.0f);
	V3.Position = FVector(0.0f, -HalfW, -HalfH); V3.UV = FVector2(0.0f, 1.0f);

	V0.Color = V1.Color = V2.Color = V3.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	V0.Normal = V1.Normal = V2.Normal = V3.Normal = FVector(0.0f, 0.0f, 1.0f);

	OutMesh.Vertices.push_back(V0);
	OutMesh.Vertices.push_back(V1);
	OutMesh.Vertices.push_back(V2);
	OutMesh.Vertices.push_back(V3);

	OutMesh.Indices = { 0, 1, 2, 0, 2, 3 };
	return true;
}

FMaterial* FBillboardRenderer::GetOrCreateMaterial(const UBillboardComponent& Component)
{
	if (!BillboardMaterial)
	{
		return nullptr;
	}

	auto Found = MaterialsByComponent.find(&Component);
	if (Found == MaterialsByComponent.end())
	{
		std::unique_ptr<FDynamicMaterial> OwnedMaterial = BillboardMaterial->CreateDynamicMaterial();
		if (!OwnedMaterial)
		{
			return BillboardMaterial.get();
		}

		std::shared_ptr<FDynamicMaterial> Material(OwnedMaterial.release());
		Found = MaterialsByComponent.emplace(&Component, std::move(Material)).first;
	}

	FDynamicMaterial* Material = Found->second.get();
	if (!Material)
	{
		return BillboardMaterial.get();
	}

	std::shared_ptr<FMaterialTexture> Texture = GetOrLoadTexture(Component.GetTexturePath());
	if (!Texture)
	{
		return nullptr;
	}

	Material->SetMaterialTexture(Texture);
	const FVector2 CellSize = Component.GetUVMax() - Component.GetUVMin();
	const FVector2 UVOffset = Component.GetUVMin();
	Material->SetParameterData("CellSize", &CellSize, sizeof(FVector2));
	Material->SetParameterData("UVOffset", &UVOffset, sizeof(FVector2));
	return Material;
}

void FBillboardRenderer::PruneMaterials(const TArray<const UBillboardComponent*>& ActiveComponents)
{
	for (auto It = MaterialsByComponent.begin(); It != MaterialsByComponent.end();)
	{
		if (std::find(ActiveComponents.begin(), ActiveComponents.end(), It->first) == ActiveComponents.end())
		{
			It = MaterialsByComponent.erase(It);
			continue;
		}

		++It;
	}
}

std::shared_ptr<FMaterialTexture> FBillboardRenderer::GetOrLoadTexture(const std::wstring& Path)
{
	if (Path.empty() || !Device || !DeviceContext)
	{
		return nullptr;
	}

	auto Found = TextureCache.find(Path);
	if (Found != TextureCache.end())
	{
		return Found->second;
	}

	ID3D11ShaderResourceView* SRV = nullptr;
	HRESULT Hr = DirectX::CreateWICTextureFromFile(Device, DeviceContext, Path.c_str(), nullptr, &SRV);
	if (FAILED(Hr) || !SRV)
	{
		return nullptr;
	}

	ID3D11SamplerState* Sampler = nullptr;
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	Hr = Device->CreateSamplerState(&SamplerDesc, &Sampler);
	if (FAILED(Hr) || !Sampler)
	{
		SRV->Release();
		return nullptr;
	}

	auto MaterialTexture = std::make_shared<FMaterialTexture>();
	MaterialTexture->TextureSRV = SRV;
	MaterialTexture->SamplerState = Sampler;
	TextureCache[Path] = MaterialTexture;
	return MaterialTexture;
}
