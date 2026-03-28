#include "Renderer.h"
#include "ShaderType.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "Material.h"
#include "MaterialManager.h"
#include "Core/Paths.h"
#include "Primitive/PrimitiveBase.h"
#include "RenderMesh.h"
#include <cassert>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static FVector GetCameraWorldPositionFromViewMatrix(const FMatrix& ViewMatrix)
{
	const FMatrix InvView = ViewMatrix.GetInverse();
	return FVector(InvView.M[3][0], InvView.M[3][1], InvView.M[3][2]);
}

CRenderer::CRenderer(HWND InHwnd, int32 InWidth, int32 InHeight)
{
	Initialize(InHwnd, InWidth, InHeight);
}

CRenderer::~CRenderer()
{
	Release();
}

void CRenderer::SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, const D3D11_VIEWPORT& InViewport)
{
	SceneRenderTargetView = InRenderTargetView;
	SceneDepthStencilView = InDepthStencilView;
	SceneViewport = InViewport;
	bUseSceneRenderTargetOverride = (SceneRenderTargetView != nullptr && SceneDepthStencilView != nullptr);
}

void CRenderer::ClearSceneRenderTarget()
{
	SceneRenderTargetView = nullptr;
	SceneDepthStencilView = nullptr;
	SceneViewport = {};
	bUseSceneRenderTargetOverride = false;
}

void CRenderer::SetGUICallbacks(
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

void CRenderer::SetGUIUpdateCallback(FGUICallback InUpdate)
{
	GUIUpdate = std::move(InUpdate);
}

void CRenderer::ClearViewportCallbacks()
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

bool CRenderer::CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height)
{
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

bool CRenderer::CreateRenderTargetAndDepthStencil(int32 Width, int32 Height)
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

bool CRenderer::Initialize(HWND InHwnd, int32 Width, int32 Height)
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

	RenderStateManager = std::make_unique<CRenderStateManager>(Device, DeviceContext);
	RenderStateManager->PrepareCommonStates();

	if (!CreateConstantBuffers()) return false;
	SetConstantBuffers();

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"VertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"PixelShader.hlsl";

	if (!ShaderManager.LoadVertexShader(Device, VSPath.c_str())) return false;
	if (!ShaderManager.LoadPixelShader(Device, PSPath.c_str())) return false;

	/** 기본 Material 생성 */
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

	/** Texture 용 Material 생성 */
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

		int32 SlotIndex = DefaultTextureMaterial->CreateConstantBuffer(Device, 16);
		if (SlotIndex >= 0)
		{
			DefaultTextureMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
			float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			DefaultTextureMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));
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
	CreateTextureFromSTB(Device, FolderIconPath.string().c_str(), &FolderIconSRV);
	CreateTextureFromSTB(Device, FileIconPath.string().c_str(), &FileIconSRV);

	return true;
}

void CRenderer::SetConstantBuffers()
{
	ID3D11Buffer* CBs[2] = { FrameConstantBuffer, ObjectConstantBuffer };
	DeviceContext->VSSetConstantBuffers(0, 2, CBs);
}

void CRenderer::BeginFrame()
{
	if (GUINewFrame) GUINewFrame();
	if (GUIUpdate) GUIUpdate();

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

void CRenderer::ClearCommandList()
{
	PrevCommandCount = CommandList.size();
	CommandList.clear();
	CommandList.reserve(PrevCommandCount);	
}

void CRenderer::EndFrame()
{
	if (RenderTargetView)
	{
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
		DeviceContext->RSSetViewports(1, &Viewport);
	}

	if (GUIRender) GUIRender();

	UINT SyncInterval = bVSyncEnabled ? 1 : 0;
	HRESULT Hr = SwapChain->Present(SyncInterval, 0);
	if (Hr == DXGI_STATUS_OCCLUDED) bSwapChainOccluded = true;

	if (GUIPostPresent) GUIPostPresent();
}

void CRenderer::SubmitCommands(const FRenderCommandQueue& Queue)
{
	ViewMatrix = Queue.ViewMatrix;
	ProjectionMatrix = Queue.ProjectionMatrix;

	for (const auto& Cmd : Queue.Commands)
	{
		if (Cmd.RenderMesh) Cmd.RenderMesh->UpdateVertexAndIndexBuffer(Device, DeviceContext);
		if (Cmd.MeshData) Cmd.MeshData->UpdateVertexAndIndexBuffer(Device);
		AddCommand(Cmd);
	}
}

void CRenderer::AddCommand(const FRenderCommand& Command)
{
	CommandList.push_back(Command);
	FRenderCommand& Added = CommandList.back();
	if (!Added.Material) Added.Material = DefaultMaterial.get();
	Added.SortKey = FRenderCommand::MakeSortKey(Added.Material, Added.MeshData);
}

void CRenderer::ExecuteCommands()
{
	std::sort(CommandList.begin(), CommandList.end(),
		[](const FRenderCommand& A, const FRenderCommand& B) {
			if (A.RenderLayer != B.RenderLayer) return A.RenderLayer < B.RenderLayer;
			return A.SortKey < B.SortKey;
		});

	SetConstantBuffers();
	UpdateFrameConstantBuffer();

	ExecuteRenderPass(ERenderLayer::Default);
	ClearDepthBuffer();
	ExecuteRenderPass(ERenderLayer::Overlay);
	
	if (PostRenderCallback) PostRenderCallback(this);
}

void CRenderer::ExecuteRenderPass(ERenderLayer InRenderLayer)
{
	FMaterial* CurrentMaterial = nullptr;
	void* CurrentMeshPtr = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY CurrentMeshTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

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
		auto Cmd = *it;
		if (Cmd.RenderLayer != InRenderLayer) return;
		if (!Cmd.MeshData && !Cmd.RenderMesh) continue;
		// if (!Cmd.MeshData || (Cmd.MeshData->Vertices.empty() && Cmd.MeshData->Indices.empty())) continue;

		if (Cmd.Material != CurrentMaterial)
		{
			Cmd.Material->Bind(DeviceContext);
			
			// RenderStateManager를 통한 일괄 상태 바인딩 (캐싱 활용)
			RenderStateManager->BindState(Cmd.Material->GetRasterizerState());
			RenderStateManager->BindState(Cmd.Material->GetDepthStencilState());
			RenderStateManager->BindState(Cmd.Material->GetBlendState());

			CurrentMaterial = Cmd.Material;

			/** 특수 머티리얼 아틀라스 바인딩 보조 */
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
				// SRV 는 일반 Material 안에서 bind
				DeviceContext->PSSetSamplers(0, 1, &NormalSampler);
			}
		}

		if (Cmd.MeshData != CurrentMeshPtr)
		{
			Cmd.MeshData->Bind(DeviceContext);
			CurrentMeshPtr = Cmd.MeshData;
		}
		if (Cmd.RenderMesh)
		{
			if (Cmd.RenderMesh->Vertices.empty() && Cmd.RenderMesh->Indices.empty()) continue;

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

			UpdateObjectConstantBuffer(Cmd.WorldMatrix);

			if (!Cmd.RenderMesh->Indices.empty())
				DeviceContext->DrawIndexed(static_cast<UINT>(Cmd.RenderMesh->Indices.size()), 0, 0);
			else
				DeviceContext->Draw(static_cast<UINT>(Cmd.RenderMesh->Vertices.size()), 0);
		}
		// =========================================================
		// 💀 [구형 아키텍처 그리기] (기존 로직)
		// =========================================================
		else if (Cmd.MeshData)
		{
			if (Cmd.MeshData->Vertices.empty() && Cmd.MeshData->Indices.empty()) continue;

			if (Cmd.MeshData != CurrentMeshPtr)
			{
				Cmd.MeshData->Bind(DeviceContext);
				CurrentMeshPtr = Cmd.MeshData;
			}

			D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = (D3D11_PRIMITIVE_TOPOLOGY)Cmd.MeshData->Topology;
			if (DesiredTopology != CurrentMeshTopology)
			{
				DeviceContext->IASetPrimitiveTopology(DesiredTopology);
				CurrentMeshTopology = DesiredTopology;
			}

			UpdateObjectConstantBuffer(Cmd.WorldMatrix);

			if (!Cmd.MeshData->Indices.empty())
				DeviceContext->DrawIndexed(static_cast<UINT>(Cmd.MeshData->Indices.size()), 0, 0);
			else
				DeviceContext->Draw(static_cast<UINT>(Cmd.MeshData->Vertices.size()), 0);
		}

		/*D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = (D3D11_PRIMITIVE_TOPOLOGY)CurrentMeshPtr->Topology;
		if (DesiredTopology != CurrentMeshTopology)
		{
			DeviceContext->IASetPrimitiveTopology(DesiredTopology);
			CurrentMeshTopology = DesiredTopology;
		}

		UpdateObjectConstantBuffer(Cmd.WorldMatrix);
		
		if (!Cmd.MeshData->Indices.empty())
			DeviceContext->DrawIndexed(static_cast<UINT>(Cmd.MeshData->Indices.size()), 0, 0);
		else if (!Cmd.MeshData->Vertices.empty())
			DeviceContext->Draw(static_cast<UINT>(Cmd.MeshData->Vertices.size()), 0);*/
	}
}

void CRenderer::ClearDepthBuffer()
{
	if (SceneDepthStencilView) DeviceContext->ClearDepthStencilView(SceneDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

FVector CRenderer::GetCameraPosition() const
{
	return GetCameraWorldPositionFromViewMatrix(ViewMatrix);
}

bool CRenderer::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Desc.ByteWidth = sizeof(FFrameConstantBuffer);
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &FrameConstantBuffer))) return false;

	Desc.ByteWidth = sizeof(FObjectConstantBuffer);
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ObjectConstantBuffer));
}

void CRenderer::UpdateFrameConstantBuffer()
{
	FFrameConstantBuffer CBData;
	CBData.View = ViewMatrix.GetTransposed();
	CBData.Projection = ProjectionMatrix.GetTransposed();
	D3D11_MAPPED_SUBRESOURCE Mapped;
	if (SUCCEEDED(DeviceContext->Map(FrameConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(FrameConstantBuffer, 0);
	}
}

void CRenderer::UpdateObjectConstantBuffer(const FMatrix& WorldMatrix)
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

bool CRenderer::CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV)
{
	int W, H, C;
	unsigned char* Data = stbi_load(FilePath, &W, &H, &C, 4);
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

bool CRenderer::InitOutlineResources()
{
	if (StencilWriteState && StencilTestState && OutlinePS) return true;

	D3D11_DEPTH_STENCIL_DESC WriteDesc = {};
	WriteDesc.DepthEnable = TRUE; WriteDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; WriteDesc.DepthFunc = D3D11_COMPARISON_LESS;
	WriteDesc.StencilEnable = TRUE; WriteDesc.StencilReadMask = 0xFF; WriteDesc.StencilWriteMask = 0xFF;
	WriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE; WriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
	WriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE; WriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	WriteDesc.BackFace = WriteDesc.FrontFace;
	if (FAILED(Device->CreateDepthStencilState(&WriteDesc, &StencilWriteState))) return false;

	D3D11_DEPTH_STENCIL_DESC TestDesc = {};
	TestDesc.DepthEnable = FALSE; TestDesc.StencilEnable = TRUE; TestDesc.StencilReadMask = 0xFF; TestDesc.StencilWriteMask = 0xFF;
	TestDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; TestDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	TestDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; TestDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	TestDesc.BackFace = TestDesc.FrontFace;
	if (FAILED(Device->CreateDepthStencilState(&TestDesc, &StencilTestState))) return false;

	FString PSPath = (FPaths::ShaderDir() / "OutlinePixelShader.hlsl").string();
	OutlinePS = FShaderMap::Get().GetOrCreatePixelShader(Device, FPaths::ToWide(PSPath).c_str());
	return OutlinePS != nullptr;
}

void CRenderer::RenderOutline(FMeshData* Mesh, const FMatrix& WorldMatrix, float OutlineScale)
{
	if (!Mesh || !InitOutlineResources()) return;
	Mesh->UpdateVertexAndIndexBuffer(Device);
	Mesh->Bind(DeviceContext);

	ID3D11RenderTargetView* ActiveRTV = bUseSceneRenderTargetOverride ? SceneRenderTargetView : RenderTargetView;
	ID3D11DepthStencilView* ActiveDSV = bUseSceneRenderTargetOverride ? SceneDepthStencilView : DepthStencilView;

	DeviceContext->OMSetRenderTargets(0, nullptr, ActiveDSV);
	DeviceContext->OMSetDepthStencilState(StencilWriteState, 1);
	UpdateObjectConstantBuffer(WorldMatrix);
	DeviceContext->DrawIndexed(static_cast<UINT>(Mesh->Indices.size()), 0, 0);

	DeviceContext->OMSetRenderTargets(1, &ActiveRTV, ActiveDSV);
	DeviceContext->OMSetDepthStencilState(StencilTestState, 1);
	UpdateObjectConstantBuffer(FMatrix::MakeScale(OutlineScale) * WorldMatrix);
	OutlinePS->Bind(DeviceContext);
	DeviceContext->DrawIndexed(static_cast<UINT>(Mesh->Indices.size()), 0, 0);

	ShaderManager.Bind(DeviceContext);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void CRenderer::DrawLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	LineVertices.push_back({ Start, Color, FVector::ZeroVector });
	LineVertices.push_back({ End, Color, FVector::ZeroVector });
}

void CRenderer::DrawCube(const FVector& Center, const FVector& BoxExtent, const FVector4& Color)
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

void CRenderer::ExecuteLineCommands()
{
	if (LineVertices.empty()) return;
	ShaderManager.Bind(DeviceContext);
	DefaultMaterial->Bind(DeviceContext);
	UINT Size = static_cast<UINT>(LineVertices.size() * sizeof(FPrimitiveVertex));
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
	UINT Stride = sizeof(FPrimitiveVertex), Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &LineVertexBuffer, &Stride, &Offset);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	UpdateObjectConstantBuffer(FMatrix::Identity);
	DeviceContext->Draw(static_cast<UINT>(LineVertices.size()), 0);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	LineVertices.clear();
}

void CRenderer::Release()
{
	ClearViewportCallbacks(); ClearSceneRenderTarget();
	TextRenderer.Release(); SubUVRenderer.Release();
	ShaderManager.Release(); FShaderMap::Get().Clear(); FMaterialManager::Get().Clear();
	if (NormalSampler) NormalSampler->Release();
	if (StencilWriteState) StencilWriteState->Release();
	if (StencilTestState) StencilTestState->Release();
	OutlinePS.reset(); DefaultMaterial.reset();
	if (FolderIconSRV)FolderIconSRV->Release();
	if (FileIconSRV)FileIconSRV->Release();
	if (LineVertexBuffer) LineVertexBuffer->Release();
	if (FrameConstantBuffer) FrameConstantBuffer->Release();
	if (ObjectConstantBuffer) ObjectConstantBuffer->Release();
	if (DepthStencilView) DepthStencilView->Release();
	if (RenderTargetView) RenderTargetView->Release();
	if (SwapChain) SwapChain->Release();
	if (DeviceContext) DeviceContext->Release();
	if (Device) Device->Release();
}

bool CRenderer::IsOccluded()
{
	if (bSwapChainOccluded && SwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) return true;
	bSwapChainOccluded = false; return false;
}

void CRenderer::OnResize(int32 W, int32 H)
{
	if (W == 0 || H == 0) return;
	ClearSceneRenderTarget();
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
	if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
	SwapChain->ResizeBuffers(0, W, H, DXGI_FORMAT_UNKNOWN, 0);
	CreateRenderTargetAndDepthStencil(W, H);
	Viewport.Width = static_cast<float>(W); Viewport.Height = static_cast<float>(H);
}
