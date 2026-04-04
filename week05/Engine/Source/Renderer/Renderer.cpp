#include "Renderer.h"
#include "ShaderType.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "ShaderResource.h"
#include "Material.h"
#include "MaterialManager.h"
#include "Core/Paths.h"
#include "RenderMesh.h"
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
	struct FOutlinePostConstantBuffer
	{
		FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
		float OutlineThickness = 2.0f;
		float OutlineThreshold = 0.1f;
		float Padding[2] = {};
	};

	FVector GetCameraWorldPositionFromViewMatrix(const FMatrix& ViewMatrix)
	{
		const FMatrix InvView = ViewMatrix.GetInverse();
		return FVector(InvView.M[3][0], InvView.M[3][1], InvView.M[3][2]);
	}

	bool GetRenderTargetSize(ID3D11RenderTargetView* RenderTargetView, uint32& OutWidth, uint32& OutHeight)
	{
		if (!RenderTargetView)
		{
			return false;
		}

		ID3D11Resource* Resource = nullptr;
		RenderTargetView->GetResource(&Resource);
		if (!Resource)
		{
			return false;
		}

		ID3D11Texture2D* Texture = nullptr;
		const HRESULT Hr = Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture));
		Resource->Release();
		if (FAILED(Hr) || !Texture)
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC Desc = {};
		Texture->GetDesc(&Desc);
		Texture->Release();

		OutWidth = Desc.Width;
		OutHeight = Desc.Height;
		return true;
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

void FRenderer::SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, const D3D11_VIEWPORT& InViewport)
{
	SceneRenderTargetView = InRenderTargetView;
	SceneDepthStencilView = InDepthStencilView;
	SceneViewport = InViewport;
	bUseSceneRenderTargetOverride = (SceneRenderTargetView != nullptr && SceneDepthStencilView != nullptr);
}

void FRenderer::ClearSceneRenderTarget()
{
	SceneRenderTargetView = nullptr;
	SceneDepthStencilView = nullptr;
	SceneViewport = {};
	bUseSceneRenderTargetOverride = false;
}

void FRenderer::BeginScenePass(ID3D11RenderTargetView* InRTV, ID3D11DepthStencilView* InDSV, const D3D11_VIEWPORT& InVP)
{
	DeviceContext->OMSetRenderTargets(1, &InRTV, InDSV);
	DeviceContext->RSSetViewports(1, &InVP);
	ClearCommandList();
}

void FRenderer::EndScenePass()
{
}

void FRenderer::BindSwapChainRTV()
{
	if (RenderTargetView)
	{
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
		DeviceContext->RSSetViewports(1, &Viewport);
	}
}

void FRenderer::SetGUICallbacks(
	FGUICallback InInit,
	FGUICallback InShutdown,
	FGUICallback InNewFrame,
	FGUICallback InRender,
	FGUICallback InPostPresent)
{
	GUIInit = std::move(InInit);
	GUIShutdown = std::move(InShutdown);
	GUINewFrame = std::move(InNewFrame);
	GUIRender = std::move(InRender);
	GUIPostPresent = std::move(InPostPresent);

	if (GUIInit)
	{
		GUIInit();
	}
}

void FRenderer::SetGUIUpdateCallback(FGUICallback InUpdate)
{
	GUIUpdate = std::move(InUpdate);
}

void FRenderer::ClearViewportCallbacks()
{
	if (GUIShutdown)
	{
		GUIShutdown();
	}

	GUIInit = nullptr;
	GUIShutdown = nullptr;
	GUINewFrame = nullptr;
	GUIUpdate = nullptr;
	GUIRender = nullptr;
	GUIPostPresent = nullptr;
	PostRenderCallback = nullptr;
}

bool FRenderer::CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height)
{
	bAllowTearing = CheckTearingSupport();

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Width = Width;
	SwapChainDesc.BufferDesc.Height = Height;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = InHwnd;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.Flags = bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
	CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
	HRESULT Hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		CreateDeviceFlags, &FeatureLevel, 1,
		D3D11_SDK_VERSION, &SwapChainDesc,
		&SwapChain, &Device, nullptr, &DeviceContext
	);

	if (FAILED(Hr))
	{
		MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChain Failed.", nullptr, 0);
		return false;
	}

	return true;
}

bool FRenderer::CheckTearingSupport() const
{
	IDXGIFactory5* Factory5 = nullptr;
	HRESULT Hr = CreateDXGIFactory1(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(&Factory5));
	if (FAILED(Hr) || !Factory5)
	{
		return false;
	}

	BOOL bSupported = FALSE;
	Hr = Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bSupported, sizeof(bSupported));
	Factory5->Release();

	return SUCCEEDED(Hr) && bSupported == TRUE;
}

bool FRenderer::CreateRenderTargetAndDepthStencil(int32 Width, int32 Height)
{
	ID3D11Texture2D* BackBuffer = nullptr;
	HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
	if (FAILED(Hr)) return false;
	
	Hr = Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);
	BackBuffer->Release();
	if (FAILED(Hr)) return false;

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D* DepthTex = nullptr;
	Hr = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthTex);
	if (FAILED(Hr)) return false;
	
	Hr = Device->CreateDepthStencilView(DepthTex, nullptr, &DepthStencilView);
	DepthTex->Release();
	
	return SUCCEEDED(Hr);
}

bool FRenderer::Initialize(HWND InHwnd, int32 Width, int32 Height)
{
	Hwnd = InHwnd;

	if (!CreateDeviceAndSwapChain(Hwnd, Width, Height)) return false;
	if (!CreateRenderTargetAndDepthStencil(Width, Height)) return false;

	Viewport.TopLeftX = 0.f;
	Viewport.TopLeftY = 0.f;
	Viewport.Width = static_cast<float>(Width);
	Viewport.Height = static_cast<float>(Height);
	Viewport.MinDepth = 0.f;
	Viewport.MaxDepth = 1.f;

	RenderStateManager = std::make_unique<FRenderStateManager>(Device, DeviceContext);
	RenderStateManager->PrepareCommonStates();

	if (!CreateSamplers()) return false;

	if (!CreateConstantBuffers()) return false;
	SetConstantBuffers();

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"VertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"PixelShader.hlsl";

	if (!ShaderManager.LoadVertexShader(Device, VSPath.c_str())) return false;
	if (!ShaderManager.LoadPixelShader(Device, PSPath.c_str())) return false;

	/** 湲곕낯 Material ?앹꽦 */
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

	/** Texture ??Material ?앹꽦 */
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

	if (!TextRenderer.Initialize(this)) return false;

	std::filesystem::path SubUVTexturePath = FPaths::ContentDir() / FString("Textures/SubUVDino.png");
	if (!SubUVRenderer.Initialize(this, (SubUVTexturePath.wstring())))
	{
		MessageBox(0, L"SubUVRenderer Initialize Failed.", 0, 0);
	}

	std::filesystem::path FolderIconPath = FPaths::AssetDir() / FString("Textures/FolderIcon.png");
	std::filesystem::path FileIconPath = FPaths::AssetDir() / FString("Textures/FileIcon.png");
	CreateTextureFromSTB(Device, FolderIconPath, &FolderIconSRV);
	CreateTextureFromSTB(Device, FileIconPath, &FileIconSRV);

	return true;
}

void FRenderer::SetConstantBuffers()
{
	ID3D11Buffer* CBs[2] = { FrameConstantBuffer, ObjectConstantBuffer };
	DeviceContext->VSSetConstantBuffers(0, 2, CBs);
}

void FRenderer::BeginFrame()
{
	if (GUINewFrame) GUINewFrame();
	FrameDrawCallCount = 0;

	constexpr float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	if (RenderTargetView) DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
	if (DepthStencilView) DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11RenderTargetView* ActiveRTV = RenderTargetView;
	ID3D11DepthStencilView* ActiveDSV = DepthStencilView;
	D3D11_VIEWPORT ActiveVP = Viewport;

	if (bUseSceneRenderTargetOverride)
	{
		ActiveRTV = SceneRenderTargetView;
		ActiveDSV = SceneDepthStencilView;
		ActiveVP = SceneViewport;
		if (ActiveRTV && ActiveRTV != RenderTargetView) DeviceContext->ClearRenderTargetView(ActiveRTV, ClearColor);
		if (ActiveDSV && ActiveDSV != DepthStencilView) DeviceContext->ClearDepthStencilView(ActiveDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	DeviceContext->OMSetRenderTargets(1, &ActiveRTV, ActiveDSV);
	DeviceContext->RSSetViewports(1, &ActiveVP);

	ClearCommandList();
}

void FRenderer::ClearCommandList()
{
	CommandList.clear();
	CommandList.reserve(PrevCommandCount);
	NextSubmissionOrder = 0;
}

void FRenderer::EndFrame()
{
	if (RenderTargetView)
	{
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
		DeviceContext->RSSetViewports(1, &Viewport);
	}

	if (GUIRender) GUIRender();

	UINT SyncInterval = bVSyncEnabled ? 1 : 0;
	UINT PresentFlags = (!bVSyncEnabled && bAllowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
	HRESULT Hr = SwapChain->Present(SyncInterval, PresentFlags);
	if (Hr == DXGI_STATUS_OCCLUDED) bSwapChainOccluded = true;

	if (GUIPostPresent) GUIPostPresent();
}

void FRenderer::SubmitCommands(const FRenderCommandQueue& Queue)
{
	ViewMatrix = Queue.ViewMatrix;
	ProjectionMatrix = Queue.ProjectionMatrix;

	for (const auto& Cmd : Queue.Commands)
	{
		if (Cmd.RenderMesh) Cmd.RenderMesh->UpdateVertexAndIndexBuffer(Device, DeviceContext);
		AddCommand(Cmd);
	}
}

void FRenderer::AddCommand(const FRenderCommand& Command)
{
	CommandList.push_back(Command);
	FRenderCommand& Added = CommandList.back();
	if (!Added.Material) Added.Material = DefaultMaterial.get();
	Added.SortKey = FRenderCommand::MakeSortKey(Added.Material, Added.RenderMesh);
	Added.SubmissionOrder = NextSubmissionOrder++;
}

void FRenderer::ExecuteCommands()
{
	std::sort(CommandList.begin(), CommandList.end(),
		[](const FRenderCommand& A, const FRenderCommand& B) {
			if (A.RenderLayer != B.RenderLayer) return A.RenderLayer < B.RenderLayer;
			if (A.RenderLayer == ERenderLayer::UI) return A.SubmissionOrder < B.SubmissionOrder;
			return A.SortKey < B.SortKey;
		});

	SetConstantBuffers();
	UpdateFrameConstantBuffer();

	ExecuteRenderPass(ERenderLayer::Default);
	ClearDepthBuffer();
	ExecuteRenderPass(ERenderLayer::Overlay);
	ExecuteRenderPass(ERenderLayer::UI);
	
	if (PostRenderCallback) PostRenderCallback(this);

	PrevCommandCount = CommandList.size();
	ClearCommandList();
}

void FRenderer::ExecuteRenderPass(ERenderLayer InRenderLayer)
{
	FMaterial* CurrentMaterial = nullptr;
	FRenderMesh* CurrentMeshPtr = nullptr; // 狩?void* ?먯꽌 FRenderMesh* 濡?蹂寃?
	D3D11_PRIMITIVE_TOPOLOGY CurrentMeshTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	FMatrix CurrentWorldMatrix = FMatrix::Identity;
	bool bHasCurrentWorldMatrix = false;

	ID3D11ShaderResourceView* FontSRV = TextRenderer.GetAtlasSRV();
	ID3D11SamplerState* FontSampler = TextRenderer.GetAtlasSampler();
	ID3D11ShaderResourceView* SubUVSRV = SubUVRenderer.GetTextureSRV();
	ID3D11SamplerState* SubUVSampler = SubUVRenderer.GetSamplerState();

	FRenderCommand toFind;
	toFind.RenderLayer = InRenderLayer;
	auto it = std::lower_bound(CommandList.begin(), CommandList.end(), toFind,
		[](const FRenderCommand& A, const FRenderCommand& B) { return A.RenderLayer < B.RenderLayer; });

	RenderStateManager->RebindState();
	for (; it != CommandList.end(); it++)
	{
		const FRenderCommand& Cmd = *it;
		if (Cmd.RenderLayer != InRenderLayer) return;

		if (!Cmd.RenderMesh) continue;

		if (Cmd.Material != CurrentMaterial)
		{
			Cmd.Material->Bind(DeviceContext);

			RenderStateManager->BindState(Cmd.Material->GetRasterizerState());
			RenderStateManager->BindState(Cmd.Material->GetDepthStencilState());
			RenderStateManager->BindState(Cmd.Material->GetBlendState());

			CurrentMaterial = Cmd.Material;

			if (CurrentMaterial->GetOriginName() == "M_Font")
			{
				DeviceContext->PSSetShaderResources(0, 1, &FontSRV);
				DeviceContext->PSSetSamplers(0, 1, &FontSampler);
			}
			else if (CurrentMaterial->GetOriginName() == "M_SubUV")
			{
				DeviceContext->PSSetShaderResources(0, 1, &SubUVSRV);
				DeviceContext->PSSetSamplers(0, 1, &SubUVSampler);
			}
			else
			{
				DeviceContext->PSSetSamplers(0, 1, &NormalSampler);
			}
		}

		if (Cmd.Material)
		{
			if (Cmd.bDisableCulling)
			{
				FRasterizerStateOption RasterOpt = Cmd.Material->GetRasterizerOption();
				RasterOpt.CullMode = D3D11_CULL_NONE;
				auto OverrideRS = RenderStateManager->GetOrCreateRasterizerState(RasterOpt);
				RenderStateManager->BindState(OverrideRS);
			}
			else
			{
				RenderStateManager->BindState(Cmd.Material->GetRasterizerState());
			}

			if (Cmd.bDisableDepthTest || Cmd.bDisableDepthWrite)
			{
				FDepthStencilStateOption DepthOpt = Cmd.Material->GetDepthStencilOption();
				if (Cmd.bDisableDepthTest)
				{
					DepthOpt.DepthEnable = false;
				}
				if (Cmd.bDisableDepthWrite)
				{
					DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				}
				auto OverrideDSS = RenderStateManager->GetOrCreateDepthStencilState(DepthOpt);
				RenderStateManager->BindState(OverrideDSS);
			}
			else
			{
				RenderStateManager->BindState(Cmd.Material->GetDepthStencilState());
			}
		}

		if (Cmd.RenderMesh)
		{
			if (Cmd.RenderMesh->Vertices.empty() && Cmd.RenderMesh->Indices.empty()) continue;

			FString MatName = Cmd.Material ? Cmd.Material->GetOriginName() : "Unknown";

			if (Cmd.RenderMesh != CurrentMeshPtr)
			{
				Cmd.RenderMesh->Bind(DeviceContext);
				CurrentMeshPtr = Cmd.RenderMesh;
			}

			D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = (D3D11_PRIMITIVE_TOPOLOGY)Cmd.RenderMesh->Topology;
			if (DesiredTopology != CurrentMeshTopology)
			{
				DeviceContext->IASetPrimitiveTopology(DesiredTopology);
				CurrentMeshTopology = DesiredTopology;
			}

			if (!bHasCurrentWorldMatrix || Cmd.WorldMatrix != CurrentWorldMatrix)
			{
				UpdateObjectConstantBuffer(Cmd.WorldMatrix);
				CurrentWorldMatrix = Cmd.WorldMatrix;
				bHasCurrentWorldMatrix = true;
			}

			if (!Cmd.RenderMesh->Indices.empty())
			{
				UINT DrawCount = (Cmd.IndexCount > 0) ? Cmd.IndexCount : static_cast<UINT>(Cmd.RenderMesh->Indices.size());
				UINT StartLocation = Cmd.IndexStart;

				FString MatName = Cmd.Material ? Cmd.Material->GetOriginName() : "Unknown";

				++FrameDrawCallCount;
				DeviceContext->DrawIndexed(DrawCount, StartLocation, 0);
			}
			else
			{
				++FrameDrawCallCount;
				DeviceContext->Draw(static_cast<UINT>(Cmd.RenderMesh->Vertices.size()), 0);
			}
		}
	}
}

void FRenderer::ClearDepthBuffer()
{
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

bool FRenderer::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Desc.ByteWidth = sizeof(FFrameConstantBuffer);
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &FrameConstantBuffer))) return false;

	Desc.ByteWidth = sizeof(FObjectConstantBuffer);
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &ObjectConstantBuffer))) return false;

	Desc.ByteWidth = sizeof(FOutlinePostConstantBuffer);
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &OutlinePostConstantBuffer));
}

bool FRenderer::CreateSamplers()
{
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

	D3D11_SAMPLER_DESC OutlineSamplerDesc = SamplerDesc;
	OutlineSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	OutlineSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	OutlineSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	OutlineSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	return SUCCEEDED(Device->CreateSamplerState(&OutlineSamplerDesc, &OutlineSampler));
}

bool FRenderer::EnsureOutlineMaskResources(uint32 Width, uint32 Height)
{
	if (OutlineMaskTexture && OutlineMaskRTV && OutlineMaskSRV &&
		OutlineMaskWidth == Width && OutlineMaskHeight == Height)
	{
		return true;
	}

	ReleaseOutlineMaskResources();

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &OutlineMaskTexture)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	if (FAILED(Device->CreateRenderTargetView(OutlineMaskTexture, nullptr, &OutlineMaskRTV)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	if (FAILED(Device->CreateShaderResourceView(OutlineMaskTexture, nullptr, &OutlineMaskSRV)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	OutlineMaskWidth = Width;
	OutlineMaskHeight = Height;
	return true;
}

void FRenderer::ReleaseOutlineMaskResources()
{
	if (OutlineMaskSRV)
	{
		OutlineMaskSRV->Release();
		OutlineMaskSRV = nullptr;
	}
	if (OutlineMaskRTV)
	{
		OutlineMaskRTV->Release();
		OutlineMaskRTV = nullptr;
	}
	if (OutlineMaskTexture)
	{
		OutlineMaskTexture->Release();
		OutlineMaskTexture = nullptr;
	}

	OutlineMaskWidth = 0;
	OutlineMaskHeight = 0;
}

void FRenderer::UpdateFrameConstantBuffer()
{
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
	FObjectConstantBuffer CBData;
	CBData.World = WorldMatrix.GetTransposed();
	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(ObjectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(ObjectConstantBuffer, 0);
	}
}

void FRenderer::UpdateOutlinePostConstantBuffer(const FVector4& OutlineColor, float OutlineThickness, float OutlineThreshold)
{
	FOutlinePostConstantBuffer CBData = {};
	CBData.OutlineColor = OutlineColor;
	CBData.OutlineThickness = OutlineThickness;
	CBData.OutlineThreshold = OutlineThreshold;

	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(OutlinePostConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(OutlinePostConstantBuffer, 0);
	}

	ID3D11Buffer* Buffer = OutlinePostConstantBuffer;
	DeviceContext->PSSetConstantBuffers(0, 1, &Buffer);
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

bool FRenderer::InitOutlineResources()
{
	if (StencilWriteState && StencilEqualState && StencilNotEqualState &&
		OutlineBlendState && OutlineRasterizerState && OutlineSampler &&
		OutlinePostVS && OutlineMaskPS && OutlineSobelPS && OutlinePostConstantBuffer)
	{
		return true;
	}

	if (!StencilWriteState)
	{
		D3D11_DEPTH_STENCIL_DESC WriteDesc = {};
		WriteDesc.DepthEnable = FALSE;
		WriteDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		WriteDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		WriteDesc.StencilEnable = TRUE;
		WriteDesc.StencilReadMask = 0xFF;
		WriteDesc.StencilWriteMask = 0xFF;
		WriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		WriteDesc.BackFace = WriteDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&WriteDesc, &StencilWriteState)))
		{
			return false;
		}
	}

	if (!StencilEqualState)
	{
		D3D11_DEPTH_STENCIL_DESC EqualDesc = {};
		EqualDesc.DepthEnable = FALSE;
		EqualDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		EqualDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		EqualDesc.StencilEnable = TRUE;
		EqualDesc.StencilReadMask = 0xFF;
		EqualDesc.StencilWriteMask = 0x00;
		EqualDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		EqualDesc.BackFace = EqualDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&EqualDesc, &StencilEqualState)))
		{
			return false;
		}
	}

	if (!StencilNotEqualState)
	{
		D3D11_DEPTH_STENCIL_DESC NotEqualDesc = {};
		NotEqualDesc.DepthEnable = FALSE;
		NotEqualDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		NotEqualDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		NotEqualDesc.StencilEnable = TRUE;
		NotEqualDesc.StencilReadMask = 0xFF;
		NotEqualDesc.StencilWriteMask = 0x00;
		NotEqualDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		NotEqualDesc.BackFace = NotEqualDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&NotEqualDesc, &StencilNotEqualState)))
		{
			return false;
		}
	}

	if (!OutlineBlendState)
	{
		D3D11_BLEND_DESC BlendDesc = {};
		BlendDesc.RenderTarget[0].BlendEnable = TRUE;
		BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(Device->CreateBlendState(&BlendDesc, &OutlineBlendState)))
		{
			return false;
		}
	}

	if (!OutlineRasterizerState)
	{
		D3D11_RASTERIZER_DESC RasterDesc = {};
		RasterDesc.FillMode = D3D11_FILL_SOLID;
		RasterDesc.CullMode = D3D11_CULL_NONE;
		RasterDesc.DepthClipEnable = TRUE;
		if (FAILED(Device->CreateRasterizerState(&RasterDesc, &OutlineRasterizerState)))
		{
			return false;
		}
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	if (!OutlinePostVS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlinePostVertexShader.hlsl").c_str(), "main", "vs_5_0");
		if (!Resource || FAILED(Device->CreateVertexShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlinePostVS)))
		{
			return false;
		}
	}

	if (!OutlineMaskPS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlineMaskPixelShader.hlsl").c_str(), "main", "ps_5_0");
		if (!Resource || FAILED(Device->CreatePixelShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlineMaskPS)))
		{
			return false;
		}
	}

	if (!OutlineSobelPS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlineSobelPixelShader.hlsl").c_str(), "main", "ps_5_0");
		if (!Resource || FAILED(Device->CreatePixelShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlineSobelPS)))
		{
			return false;
		}
	}

	return true;
}

void FRenderer::RenderOutlines(const TArray<FOutlineRenderItem>& Items)
{
	if (Items.empty() || !InitOutlineResources())
	{
		return;
	}

	ID3D11RenderTargetView* BoundRTV = nullptr;
	ID3D11DepthStencilView* BoundDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &BoundRTV, &BoundDSV);
	if (!BoundRTV || !BoundDSV)
	{
		if (BoundRTV) BoundRTV->Release();
		if (BoundDSV) BoundDSV->Release();
		return;
	}

	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;
	if (!GetRenderTargetSize(BoundRTV, TargetWidth, TargetHeight) || !EnsureOutlineMaskResources(TargetWidth, TargetHeight))
	{
		BoundRTV->Release();
		BoundDSV->Release();
		return;
	}

	constexpr float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };
	constexpr float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11Buffer* NullCB = nullptr;

	DeviceContext->ClearDepthStencilView(BoundDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);
	SetConstantBuffers();
	ShaderManager.Bind(DeviceContext);
	DeviceContext->PSSetShader(nullptr, nullptr, 0);
	DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	DeviceContext->RSSetState(nullptr);
	DeviceContext->OMSetRenderTargets(0, nullptr, BoundDSV);
	DeviceContext->OMSetDepthStencilState(StencilWriteState, 1);

	for (const FOutlineRenderItem& Item : Items)
	{
		if (!Item.Mesh || !Item.Mesh->UpdateVertexAndIndexBuffer(Device, DeviceContext))
		{
			continue;
		}

		Item.Mesh->Bind(DeviceContext);
		if (Item.Mesh->Topology != EMeshTopology::EMT_Undefined)
		{
			DeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)Item.Mesh->Topology);
		}

		UpdateObjectConstantBuffer(Item.WorldMatrix);
		if (!Item.Mesh->Indices.empty())
		{
			++FrameDrawCallCount;
			DeviceContext->DrawIndexed(static_cast<UINT>(Item.Mesh->Indices.size()), 0, 0);
		}
		else
		{
			++FrameDrawCallCount;
			DeviceContext->Draw(static_cast<UINT>(Item.Mesh->Vertices.size()), 0);
		}
	}

	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
	DeviceContext->OMSetRenderTargets(1, &OutlineMaskRTV, BoundDSV);
	DeviceContext->ClearRenderTargetView(OutlineMaskRTV, ClearColor);
	DeviceContext->OMSetDepthStencilState(StencilEqualState, 1);
	DeviceContext->RSSetState(OutlineRasterizerState);
	DeviceContext->IASetInputLayout(nullptr);
	DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->VSSetShader(OutlinePostVS, nullptr, 0);
	DeviceContext->PSSetShader(OutlineMaskPS, nullptr, 0);
	++FrameDrawCallCount;
	DeviceContext->Draw(3, 0);

	DeviceContext->OMSetRenderTargets(1, &BoundRTV, BoundDSV);
	DeviceContext->OMSetDepthStencilState(StencilNotEqualState, 1);
	DeviceContext->OMSetBlendState(OutlineBlendState, BlendFactor, 0xFFFFFFFF);
	DeviceContext->VSSetShader(OutlinePostVS, nullptr, 0);
	DeviceContext->PSSetShader(OutlineSobelPS, nullptr, 0);
	UpdateOutlinePostConstantBuffer(FVector4(1.0f, 0.5f, 0.0f, 1.0f), 2.0f, 0.1f);
	DeviceContext->PSSetShaderResources(0, 1, &OutlineMaskSRV);
	DeviceContext->PSSetSamplers(0, 1, &OutlineSampler);
	++FrameDrawCallCount;
	DeviceContext->Draw(3, 0);

	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
	DeviceContext->PSSetConstantBuffers(0, 1, &NullCB);
	DeviceContext->ClearDepthStencilView(BoundDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);
	DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	DeviceContext->RSSetState(nullptr);
	ShaderManager.Bind(DeviceContext);
	SetConstantBuffers();
	RenderStateManager->RebindState();

	BoundRTV->Release();
	BoundDSV->Release();
}

void FRenderer::DrawLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	LineVertices.push_back({ Start, Color, FVector::ZeroVector });
	LineVertices.push_back({ End, Color, FVector::ZeroVector });
}

void FRenderer::DrawCube(const FVector& Center, const FVector& BoxExtent, const FVector4& Color)
{
	FVector v[8] = {
		Center + FVector(-BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z), Center + FVector(-BoxExtent.X, -BoxExtent.Y, BoxExtent.Z),
		Center + FVector(-BoxExtent.X, BoxExtent.Y, -BoxExtent.Z), Center + FVector(-BoxExtent.X, BoxExtent.Y, BoxExtent.Z),
		Center + FVector(BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z), Center + FVector(BoxExtent.X, -BoxExtent.Y, BoxExtent.Z),
		Center + FVector(BoxExtent.X, BoxExtent.Y, -BoxExtent.Z), Center + FVector(BoxExtent.X, BoxExtent.Y, BoxExtent.Z)
	};
	DrawLine(v[0], v[4], Color); DrawLine(v[4], v[6], Color); DrawLine(v[6], v[2], Color); DrawLine(v[2], v[0], Color);
	DrawLine(v[1], v[5], Color); DrawLine(v[5], v[7], Color); DrawLine(v[7], v[3], Color); DrawLine(v[3], v[1], Color);
	DrawLine(v[0], v[1], Color); DrawLine(v[4], v[5], Color); DrawLine(v[6], v[7], Color); DrawLine(v[2], v[3], Color);
}

void FRenderer::ExecuteLineCommands()
{
	if (LineVertices.empty()) return;
	ShaderManager.Bind(DeviceContext);
	DefaultMaterial->Bind(DeviceContext);
	UINT Size = static_cast<UINT>(LineVertices.size() * sizeof(FVertex));
	if (LineVertexBuffer && LineVertexBufferSize < Size) { LineVertexBuffer->Release(); LineVertexBuffer = nullptr; }
	if (!LineVertexBuffer)
	{
		D3D11_BUFFER_DESC Desc = { Size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		Device->CreateBuffer(&Desc, nullptr, &LineVertexBuffer);
		LineVertexBufferSize = Size;
	}
	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(LineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, LineVertices.data(), Size);
		DeviceContext->Unmap(LineVertexBuffer, 0);
	}
	UINT Stride = sizeof(FVertex), Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &LineVertexBuffer, &Stride, &Offset);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	UpdateObjectConstantBuffer(FMatrix::Identity);
	++FrameDrawCallCount;
	DeviceContext->Draw(static_cast<UINT>(LineVertices.size()), 0);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	LineVertices.clear();
}

void FRenderer::Release()
{
	ClearViewportCallbacks(); ClearSceneRenderTarget();
	TextRenderer.Release(); SubUVRenderer.Release();
	ShaderManager.Release(); FShaderMap::Get().Clear(); FMaterialManager::Get().Clear();
	if (NormalSampler) NormalSampler->Release();
	if (OutlineSampler) OutlineSampler->Release();
	if (StencilWriteState) StencilWriteState->Release();
	if (StencilEqualState) StencilEqualState->Release();
	if (StencilNotEqualState) StencilNotEqualState->Release();
	if (OutlineBlendState) OutlineBlendState->Release();
	if (OutlineRasterizerState) OutlineRasterizerState->Release();
	if (OutlinePostVS) OutlinePostVS->Release();
	if (OutlineMaskPS) OutlineMaskPS->Release();
	if (OutlineSobelPS) OutlineSobelPS->Release();
	ReleaseOutlineMaskResources();
	DefaultMaterial.reset();
	if (FolderIconSRV)FolderIconSRV->Release();
	if (FileIconSRV)FileIconSRV->Release();
	if (LineVertexBuffer) LineVertexBuffer->Release();
	if (FrameConstantBuffer) FrameConstantBuffer->Release();
	if (ObjectConstantBuffer) ObjectConstantBuffer->Release();
	if (OutlinePostConstantBuffer) OutlinePostConstantBuffer->Release();
	if (DepthStencilView) DepthStencilView->Release();
	if (RenderTargetView) RenderTargetView->Release();
	if (SwapChain) SwapChain->Release();
	if (DeviceContext) DeviceContext->Release();
	if (Device) Device->Release();
}

bool FRenderer::IsOccluded()
{
	if (bSwapChainOccluded && SwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) return true;
	bSwapChainOccluded = false; return false;
}

void FRenderer::OnResize(int32 W, int32 H)
{
	if (W == 0 || H == 0) return;
	ClearSceneRenderTarget();
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
	if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
	ReleaseOutlineMaskResources();
	const UINT ResizeFlags = bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	SwapChain->ResizeBuffers(0, W, H, DXGI_FORMAT_UNKNOWN, ResizeFlags);
	CreateRenderTargetAndDepthStencil(W, H);
	Viewport.Width = static_cast<float>(W); Viewport.Height = static_cast<float>(H);
}
