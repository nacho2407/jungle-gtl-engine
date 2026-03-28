#include "Renderer/SubUVRenderer.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderResource.h"
#include "Renderer/ShaderType.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/ShaderMap.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Primitive/PrimitiveBase.h"
#include "Core/Paths.h"
#include <WICTextureLoader.h>
#include <cstring>
#include <algorithm>

FSubUVRenderer::~FSubUVRenderer()
{
	Release();
}

bool FSubUVRenderer::Initialize(FRenderer* InRenderer, const std::wstring& TexturePath)
{
	Release();

	if (!InRenderer)
	{
		return false;
	}

	Device = InRenderer->GetDevice();
	DeviceContext = InRenderer->GetDeviceContext();

	if (!Device || !DeviceContext)
	{
		return false;
	}

	// 텍스처 및 샘플러 생성
	HRESULT Hr = DirectX::CreateWICTextureFromFile(Device, DeviceContext, TexturePath.c_str(), nullptr, &TextureSRV);
	if (FAILED(Hr)) return false;

	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	Hr = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
	if (FAILED(Hr)) return false;

	// 전용 머티리얼 구성
	const std::wstring ShaderDir = FPaths::ShaderDir();
	const std::wstring VSPath = ShaderDir + L"SubUVVertexShader.hlsl";
	const std::wstring PSPath = ShaderDir + L"SubUVPixelShader.hlsl";

	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	SubUVMaterial = std::make_shared<FMaterial>();
	SubUVMaterial->SetOriginName("M_SubUV");
	SubUVMaterial->SetVertexShader(VS);
	SubUVMaterial->SetPixelShader(PS);

	// 래스터라이저 설정 (컬링 방지)
	FRasterizerStateOption rasterizerOption;
	rasterizerOption.FillMode = D3D11_FILL_SOLID;
	rasterizerOption.CullMode = D3D11_CULL_NONE;
	auto RS = InRenderer->GetRenderStateManager()->GetOrCreateRasterizerState(rasterizerOption);
	SubUVMaterial->SetRasterizerOption(rasterizerOption);
	SubUVMaterial->SetRasterizerState(RS);

	// 깊이 설정
	FDepthStencilStateOption depthOption;
	depthOption.DepthEnable = true;
	depthOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	auto DSS = InRenderer->GetRenderStateManager()->GetOrCreateDepthStencilState(depthOption);
	SubUVMaterial->SetDepthStencilOption(depthOption);
	SubUVMaterial->SetDepthStencilState(DSS);

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
	SubUVMaterial->SetBlendOption(blendOption);
	SubUVMaterial->SetBlendState(BS);

	// b2: SubUV 파라미터 (CellSize(8), UVOffset(8)) = 16 bytes
	int32 SlotIndex = SubUVMaterial->CreateConstantBuffer(Device, 16);
	if (SlotIndex >= 0)
	{
		SubUVMaterial->RegisterParameter("CellSize", SlotIndex, 0, 8);
		SubUVMaterial->RegisterParameter("UVOffset", SlotIndex, 8, 8);
	}

	return true;
}

void FSubUVRenderer::Release()
{
	if (TextureSRV) { TextureSRV->Release(); TextureSRV = nullptr; }
	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }
	SubUVMaterial.reset();
	Device = nullptr;
	DeviceContext = nullptr;
}

bool FSubUVRenderer::BuildSubUVMesh(const FVector2& Size, FMeshData& OutMesh) const
{
	OutMesh.Vertices.clear();
	OutMesh.Indices.clear();
	OutMesh.Topology = EMeshTopology::EMT_TriangleList;

	const float HalfW = Size.X * 0.5f;
	const float HalfH = Size.Y * 0.5f;

	FPrimitiveVertex V0, V1, V2, V3;
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

void FSubUVRenderer::UpdateAnimationParams(
	int32 Columns, int32 Rows, int32 TotalFrames,
	int32 FirstFrame, int32 LastFrame,
	float FPS, float ElapsedTime, bool bLoop)
{
	if (!SubUVMaterial || Columns <= 0 || Rows <= 0) return;

	float FrameFloat = ElapsedTime * FPS;
	int32 AnimationFrame = static_cast<int32>(FrameFloat);

	int32 FirstRow = FirstFrame / Columns;
	int32 LastRow = LastFrame / Columns;

	FirstRow = std::max<int32>(0, std::min<int32>(FirstRow, Rows - 1));
	LastRow = std::max<int32>(0, std::min<int32>(LastRow, Rows - 1));

	if (FirstRow > LastRow)
	{
		std::swap(FirstRow, LastRow);
	}

	const int32 PlayableRowCount = LastRow - FirstRow + 1;

	int32 RowIndex = FirstRow;

	if (PlayableRowCount > 0)
	{
		if (bLoop)
		{
			RowIndex = FirstRow + (AnimationFrame % PlayableRowCount);
		}
		else
		{
			RowIndex = FirstRow + std::min<int32>(AnimationFrame, PlayableRowCount - 1);
		}
	}

	const int32 TargetColumn = 0;
	int32 FrameIndex = RowIndex * Columns + TargetColumn;

	const int32 Col = FrameIndex % Columns;
	const int32 Row = FrameIndex / Columns;

	FVector2 CellSize(1.0f / static_cast<float>(Columns), 1.0f / static_cast<float>(Rows));
	FVector2 UVOffset(static_cast<float>(Col) * CellSize.X, static_cast<float>(Row) * CellSize.Y);

	SubUVMaterial->SetParameterData("CellSize", &CellSize, 8);
	SubUVMaterial->SetParameterData("UVOffset", &UVOffset, 8);
}
