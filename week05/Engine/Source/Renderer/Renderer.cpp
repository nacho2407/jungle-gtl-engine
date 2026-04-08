#include "Renderer.h"
#include "ShaderType.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "ShaderResource.h"
#include "Material.h"
#include "MaterialManager.h"
#include "Core/Paths.h"
#include "RenderMesh.h"
#include "Renderer/SceneCommandBuilder.h"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "Asset/ObjManager.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "ThirdParty/stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace
{
	FVector GetCameraWorldPositionFromViewMatrix(const FMatrix& ViewMatrix)
	{
		const FMatrix InvView = ViewMatrix.GetInverse();
		return FVector(InvView.M[3][0], InvView.M[3][1], InvView.M[3][2]);
	}
}

FRenderer::FRenderer(HWND InHwnd, int32 InWidth, int32 InHeight)
{
	Initialize(InHwnd, InWidth, InHeight);
}

FRenderer::~FRenderer()
{
	Release();
}

bool FRenderer::Initialize(HWND InHwnd, int32 Width, int32 Height)
{
	if (!RenderDevice.Initialize(InHwnd, Width, Height)) return false;
	ID3D11Device* Device = RenderDevice.GetDevice();
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!Device || !DeviceContext) return false;

	RenderStateManager = std::make_unique<FRenderStateManager>(Device, DeviceContext);
	RenderStateManager->PrepareCommonStates();

	if (!CreateSamplers()) return false;
	if (!ViewportCompositor.Initialize(Device)) return false;

	if (!CreateConstantBuffers()) return false;
	SetConstantBuffers();

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"VertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"PixelShader.hlsl";

	if (!ShaderManager.LoadVertexShader(Device, VSPath.c_str())) return false;
	if (!ShaderManager.LoadPixelShader(Device, PSPath.c_str())) return false;

		{
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		std::wstring ColorPSPath = ShaderDirW + L"ColorPixelShader.hlsl";
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, ColorPSPath.c_str());
		DefaultMaterial = std::make_shared<FMaterial>();
		DefaultMaterial->SetOriginName("M_Default");
		DefaultMaterial->SetVertexShader(VS);
		DefaultMaterial->SetPixelShader(PS);

		FRasterizerStateOption rasterizerOption;
		rasterizerOption.FillMode = D3D11_FILL_SOLID;
		rasterizerOption.CullMode = D3D11_CULL_BACK;
		auto RS = RenderStateManager->GetOrCreateRasterizerState(rasterizerOption);
		DefaultMaterial->SetRasterizerOption(rasterizerOption);
		DefaultMaterial->SetRasterizerState(RS);

		FDepthStencilStateOption depthStencilOption;
		depthStencilOption.DepthEnable = true;
		depthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		auto DSS = RenderStateManager->GetOrCreateDepthStencilState(depthStencilOption);
		DefaultMaterial->SetDepthStencilOption(depthStencilOption);
		DefaultMaterial->SetDepthStencilState(DSS);

		int32 SlotIndex = DefaultMaterial->CreateConstantBuffer(Device, 16);
		if (SlotIndex >= 0)
		{
			DefaultMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
			float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			DefaultMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));
		}
		FMaterialManager::Get().Register("M_Default", DefaultMaterial);
	}

		{
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		std::wstring TexturePSPath = ShaderDirW + L"TexturePixelShader.hlsl";
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, TexturePSPath.c_str());
		DefaultTextureMaterial = std::make_shared<FMaterial>();
		DefaultTextureMaterial->SetOriginName("M_Default");
		DefaultTextureMaterial->SetVertexShader(VS);
		DefaultTextureMaterial->SetPixelShader(PS);

		FRasterizerStateOption rasterizerOption;
		rasterizerOption.FillMode = D3D11_FILL_SOLID;
		rasterizerOption.CullMode = D3D11_CULL_BACK;
		auto RS = RenderStateManager->GetOrCreateRasterizerState(rasterizerOption);
		DefaultTextureMaterial->SetRasterizerOption(rasterizerOption);
		DefaultTextureMaterial->SetRasterizerState(RS);

		FDepthStencilStateOption depthStencilOption;
		depthStencilOption.DepthEnable = true;
		depthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		auto DSS = RenderStateManager->GetOrCreateDepthStencilState(depthStencilOption);
		DefaultTextureMaterial->SetDepthStencilOption(depthStencilOption);
		DefaultTextureMaterial->SetDepthStencilState(DSS);

		int32 SlotIndex = DefaultTextureMaterial->CreateConstantBuffer(Device, 32);
		if (SlotIndex >= 0)
		{
			DefaultTextureMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
			float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			DefaultTextureMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));

			DefaultTextureMaterial->RegisterParameter("UVScrollSpeed", SlotIndex, 16, 16);
			float DefaultScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			DefaultTextureMaterial->GetConstantBuffer(SlotIndex)->SetData(DefaultScroll, sizeof(DefaultScroll), 16);
		}
		FMaterialManager::Get().Register("M_Default_Texture", DefaultTextureMaterial);
	}

	TextFeature = std::make_unique<FTextRenderFeature>();
	if (!TextFeature || !TextFeature->Initialize(*this))
	{
		return false;
	}

	std::filesystem::path SubUVTexturePath = FPaths::ContentDir() / FString("Textures/SubUVDino.png");
	SubUVFeature = std::make_unique<FSubUVRenderFeature>();
	if (!SubUVFeature || !SubUVFeature->Initialize(*this, SubUVTexturePath.wstring()))
	{
		MessageBox(0, L"SubUVRenderer Initialize Failed.", 0, 0);
		return false;
	}

	BillboardFeature = std::make_unique<FBillboardRenderer>();
	if (!BillboardFeature || !BillboardFeature->Initialize(*this))
	{
		return false;
	}

	OutlineFeature = std::make_unique<FOutlineRenderFeature>();
	DebugLineFeature = std::make_unique<FDebugLineRenderFeature>();

	std::filesystem::path FolderIconPath = FPaths::AssetDir() / FString("Textures/FolderIcon.png");
	std::filesystem::path FileIconPath = FPaths::AssetDir() / FString("Textures/FileIcon.png");
	CreateTextureFromSTB(Device, FolderIconPath, &FolderIconSRV);
	CreateTextureFromSTB(Device, FileIconPath, &FileIconSRV);

	return true;
}

void FRenderer::SetConstantBuffers()
{
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	ID3D11Buffer* CBs[2] = { FrameConstantBuffer, ObjectConstantBuffer };
	DeviceContext->VSSetConstantBuffers(0, 2, CBs);
}

void FRenderer::BeginFrame()
{
	constexpr float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	RenderDevice.BeginFrame(ClearColor);
	SceneRenderer.BeginFrame();
}

void FRenderer::EndFrame()
{
	RenderDevice.EndFrame();
}

bool FRenderer::RenderGameFrame(const FGameFrameRequest& Request)
{
	if (!SceneRenderer.RenderPacketToTarget(
		*this,
		RenderDevice.GetRenderTargetView(),
		RenderDevice.GetDepthStencilView(),
		RenderDevice.GetViewport(),
		Request.ScenePacket,
		Request.SceneView,
		Request.AdditionalCommands,
		Request.bForceWireframe,
		Request.WireframeMaterial,
		Request.ClearColor))
	{
		return false;
	}

	return RenderDebugLines(Request.DebugLineRequest);
}

bool FRenderer::RenderScreenUIDrawList(const FUIDrawList& DrawList)
{
	return ScreenUIRenderer.Render(*this, DrawList);
}

bool FRenderer::ComposeViewports(const TArray<FViewportCompositeItem>& Items)
{
	RenderDevice.BindSwapChainRTV();
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!DeviceContext)
	{
		return false;
	}

	return ViewportCompositor.Compose(DeviceContext, Items);
}

bool FRenderer::RenderEditorFrame(const FEditorFrameRequest& Request)
{
	for (size_t ScenePassIndex = 0; ScenePassIndex < Request.ScenePasses.size(); ++ScenePassIndex)
	{
		const FViewportScenePassRequest& ScenePass = Request.ScenePasses[ScenePassIndex];
		if (!ScenePass.IsValid())
		{
			continue;
		}

		if (!SceneRenderer.RenderPacketToTarget(
			*this,
			ScenePass.RenderTargetView,
			ScenePass.DepthStencilView,
			ScenePass.Viewport,
			ScenePass.ScenePacket,
			ScenePass.SceneView,
			ScenePass.AdditionalCommands,
			ScenePass.bForceWireframe,
			ScenePass.WireframeMaterial,
			ScenePass.ClearColor))
		{
			continue;
		}

		if (ScenePass.OutlineRequest.bEnabled && OutlineFeature)
		{
			OutlineFeature->Render(*this, ScenePass.OutlineRequest);
		}

		if (!ScenePass.DebugLineRequest.IsEmpty())
		{
			RenderDebugLines(ScenePass.DebugLineRequest);
		}
	}

	if (!Request.CompositeItems.empty())
	{
		ComposeViewports(Request.CompositeItems);
	}
	else
	{
		RenderDevice.BindSwapChainRTV();
	}

	RenderScreenUIDrawList(Request.ScreenDrawList);
	return true;
}

bool FRenderer::RenderDebugLines(const FDebugLineRenderRequest& Request)
{
	if (!DebugLineFeature)
	{
		return Request.IsEmpty();
	}

	return DebugLineFeature->Render(*this, Request);
}

void FRenderer::ClearDepthBuffer()
{
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	ID3D11RenderTargetView* BoundRTV = nullptr;
	ID3D11DepthStencilView* BoundDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &BoundRTV, &BoundDSV);

	if (BoundDSV)
	{
		DeviceContext->ClearDepthStencilView(BoundDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	if (BoundRTV)
	{
		BoundRTV->Release();
	}
	if (BoundDSV)
	{
		BoundDSV->Release();
	}
}

FVector FRenderer::GetCameraPosition() const
{
	return GetCameraWorldPositionFromViewMatrix(ViewMatrix);
}

size_t FRenderer::GetPrevCommandCount() const
{
	return SceneRenderer.GetPrevCommandCount();
}

ID3D11DepthStencilView* FRenderer::GetDepthStencilView() const
{
	return RenderDevice.GetDepthStencilView();
}

bool FRenderer::CreateConstantBuffers()
{
	ID3D11Device* Device = RenderDevice.GetDevice();
	if (!Device)
	{
		return false;
	}

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Desc.ByteWidth = sizeof(FFrameConstantBuffer);
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &FrameConstantBuffer))) return false;

	Desc.ByteWidth = sizeof(FObjectConstantBuffer);
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ObjectConstantBuffer));
}

bool FRenderer::CreateSamplers()
{
	ID3D11Device* Device = RenderDevice.GetDevice();
	if (!Device)
	{
		return false;
	}

	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT Hr = Device->CreateSamplerState(&SamplerDesc, &NormalSampler);
	if (FAILED(Hr))
	{
		return false;
	}

	return true;
}

void FRenderer::UpdateFrameConstantBuffer()
{
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	FFrameConstantBuffer CBData;
	CBData.View = ViewMatrix.GetTransposed();
	CBData.Projection = ProjectionMatrix.GetTransposed();
	CBData.Time = static_cast<float>(GEngine->GetTimer().GetTotalTime());
	CBData.DeltaTime = GEngine->GetDeltaTime();
	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(FrameConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(FrameConstantBuffer, 0);
	}
}

void FRenderer::UpdateObjectConstantBuffer(const FMatrix& WorldMatrix)
{
	ID3D11DeviceContext* DeviceContext = RenderDevice.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	FObjectConstantBuffer CBData;
	CBData.World = WorldMatrix.GetTransposed();
	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(ObjectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(ObjectConstantBuffer, 0);
	}
}

bool FRenderer::CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV)
{
	if (FilePath == nullptr)
	{
		return false;
	}

	return CreateTextureFromSTB(Device, FPaths::ToPath(FilePath), OutSRV);
}

bool FRenderer::CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV)
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
	if (!Data) return false;

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = W; Desc.Height = H; Desc.MipLevels = 1; Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT; Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = { Data, static_cast<UINT>(W * 4), 0 };
	ID3D11Texture2D* Tex = nullptr;
	HRESULT hr = Device->CreateTexture2D(&Desc, &InitData, &Tex);
	stbi_image_free(Data);
	if (FAILED(hr)) return false;

	hr = Device->CreateShaderResourceView(Tex, nullptr, OutSRV);
	Tex->Release();
	return SUCCEEDED(hr);
}

void FRenderer::Release()
{
	ViewportCompositor.Release();
	if (OutlineFeature) OutlineFeature->Release();
	if (DebugLineFeature) DebugLineFeature->Release();
	if (TextFeature) TextFeature->Release();
	if (SubUVFeature) SubUVFeature->Release();
	if (BillboardFeature) BillboardFeature->Release();
	OutlineFeature.reset();
	DebugLineFeature.reset();
	TextFeature.reset();
	SubUVFeature.reset();
	BillboardFeature.reset();
	ShaderManager.Release(); FShaderMap::Get().Clear(); FMaterialManager::Get().Clear();
	if (NormalSampler) { NormalSampler->Release(); NormalSampler = nullptr; }
	DefaultMaterial.reset();
	if (FolderIconSRV) { FolderIconSRV->Release(); FolderIconSRV = nullptr; }
	if (FileIconSRV) { FileIconSRV->Release(); FileIconSRV = nullptr; }
	if (FrameConstantBuffer) { FrameConstantBuffer->Release(); FrameConstantBuffer = nullptr; }
	if (ObjectConstantBuffer) { ObjectConstantBuffer->Release(); ObjectConstantBuffer = nullptr; }
	RenderDevice.Release();
}

bool FRenderer::IsOccluded()
{
	return RenderDevice.IsOccluded();
}

void FRenderer::OnResize(int32 W, int32 H)
{
	if (W == 0 || H == 0) return;
	RenderDevice.OnResize(W, H);
}
