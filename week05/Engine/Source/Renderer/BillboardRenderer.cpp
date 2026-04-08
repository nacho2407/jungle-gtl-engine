#include "BillboardRenderer.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/ShaderMap.h"
#include "Core/Paths.h"
#include <WICTextureLoader.h>
#include <cstring>
#include <algorithm>

FBillboardRenderer::~FBillboardRenderer()
{
	Release();
}

bool FBillboardRenderer::Initialize(FRenderer* InRenderer)
{
	Release();

	if (!InRenderer)
		return false;

	Device = InRenderer->GetDevice();
	DeviceContext = InRenderer->GetDeviceContext();

	if (!Device || !DeviceContext)
		return false;


	// 파일에 포함되어 있는 텍스처 로딩
	for (auto& Entry : std::filesystem::directory_iterator(FPaths::IconDir()))
	{
		if (Entry.path().extension() == L".png" || Entry.path().extension() == L".dds")
		{
			GetOrLoadTexture(Entry.path().wstring());
		}
	}

	// 전용 머티리얼 구성
	const std::wstring ShaderDir = FPaths::ShaderDir();
	const std::wstring VSPath = ShaderDir + L"SubUVVertexShader.hlsl";
	const std::wstring PSPath = ShaderDir + L"SubUVPixelShader.hlsl";

	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	BillboardMaterial = std::make_shared<FMaterial>();
	BillboardMaterial->SetOriginName("M_Billboard");
	BillboardMaterial->SetVertexShader(VS);
	BillboardMaterial->SetPixelShader(PS);

	// 래스터라이저 설정 (컬링 방지)
	FRasterizerStateOption rasterizerOption;
	rasterizerOption.FillMode = D3D11_FILL_SOLID;
	rasterizerOption.CullMode = D3D11_CULL_NONE;
	auto RS = InRenderer->GetRenderStateManager()->GetOrCreateRasterizerState(rasterizerOption);
	BillboardMaterial->SetRasterizerOption(rasterizerOption);
	BillboardMaterial->SetRasterizerState(RS);

	// 깊이 설정
	FDepthStencilStateOption depthOption;
	depthOption.DepthEnable = true;
	depthOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	auto DSS = InRenderer->GetRenderStateManager()->GetOrCreateDepthStencilState(depthOption);
	BillboardMaterial->SetDepthStencilOption(depthOption);
	BillboardMaterial->SetDepthStencilState(DSS);

	// 알파 블렌딩 설정
	FBlendStateOption blendOption;
	blendOption.BlendEnable = true;
	blendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendOption.BlendOp = D3D11_BLEND_OP_ADD;
	blendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
	blendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	auto BS = InRenderer->GetRenderStateManager()->GetOrCreateBlendState(blendOption);
	BillboardMaterial->SetBlendOption(blendOption);
	BillboardMaterial->SetBlendState(BS);


	int32 SlotIndex = BillboardMaterial->CreateConstantBuffer(Device, 16);
	if (SlotIndex >= 0)
	{
		BillboardMaterial->RegisterParameter("CellSize", SlotIndex, 0, 8);
		BillboardMaterial->RegisterParameter("UVOffset", SlotIndex, 8, 8);
	}

	return true;
}

void FBillboardRenderer::Release()
{
	BillboardMaterial.reset();
	Device = nullptr;
	DeviceContext = nullptr;
}

bool FBillboardRenderer::BuildBillboardMesh(const FVector2& Size, FRenderMesh& OutMesh) const
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

	V0.Color = V1.Color = V2.Color = V3.Color = FVector4(1, 1, 1, 1);
	V0.Normal = V1.Normal = V2.Normal = V3.Normal = FVector(0, 0, 1);

	OutMesh.Vertices.push_back(V0);
	OutMesh.Vertices.push_back(V1);
	OutMesh.Vertices.push_back(V2);
	OutMesh.Vertices.push_back(V3);

	OutMesh.Indices = { 0, 1, 2, 0, 2, 3 };

	return true;
}

std::shared_ptr<FMaterialTexture> FBillboardRenderer::GetOrLoadTexture(const std::wstring& Path)
{
	if (TextureCache.contains(Path))
		return TextureCache[Path];

	// 텍스처 및 샘플러 생성
	ID3D11ShaderResourceView* SRV = nullptr;
	HRESULT Hr = DirectX::CreateWICTextureFromFile(Device, DeviceContext, Path.c_str(), nullptr, &SRV);
	if (FAILED(Hr)) return nullptr;

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

	auto MatTex = std::make_shared<FMaterialTexture>();
	MatTex->TextureSRV = SRV;
	MatTex->SamplerState = Sampler;

	TextureCache[Path] = MatTex;
	return MatTex;
}
