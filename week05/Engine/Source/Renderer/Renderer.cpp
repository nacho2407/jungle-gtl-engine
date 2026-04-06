#include "Renderer.h"
#include "ShaderType.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "ShaderResource.h"
#include "Material.h"
#include "ObjectUniformStream.h"
#include "PassExecutor.h"
#include "MaterialManager.h"
#include "TextureLoader.h"
#include "Core/Paths.h"
#include "RenderMesh.h"
#include "SceneRenderer.h"
#include <cassert>
#include <algorithm>

#include "Asset/ObjManager.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <chrono>

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

namespace
{
	void ReleaseIfValid(IUnknown*& Resource)
	{
		if (Resource)
		{
			Resource->Release();
			Resource = nullptr;
		}
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

void FRenderer::SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, ID3D11ShaderResourceView* InDepthShaderResourceView, const D3D11_VIEWPORT& InViewport)
{
	SceneRenderTargetView = InRenderTargetView;
	SceneDepthStencilView = InDepthStencilView;
	SceneDepthShaderResourceView = InDepthShaderResourceView;
	SceneViewport = InViewport;
	bUseSceneRenderTargetOverride = (SceneRenderTargetView != nullptr && SceneDepthStencilView != nullptr);
}

void FRenderer::ClearSceneRenderTarget()
{
	SceneRenderTargetView = nullptr;
	SceneDepthStencilView = nullptr;
	SceneDepthShaderResourceView = nullptr;
	ActiveSceneDepthShaderResourceView = DepthTextureResources.DepthSRV;
	SceneViewport = {};
	bUseSceneRenderTargetOverride = false;
}

void FRenderer::BeginScenePass(ID3D11RenderTargetView* InRTV, ID3D11DepthStencilView* InDSV, ID3D11ShaderResourceView* InDepthSRV, const D3D11_VIEWPORT& InVP)
{
	DeviceContext->OMSetRenderTargets(1, &InRTV, InDSV);
	DeviceContext->RSSetViewports(1, &InVP);
	ActiveSceneDepthShaderResourceView = InDepthSRV ? InDepthSRV : DepthTextureResources.DepthSRV;
	EnsureHZBResources(static_cast<uint32>(InVP.Width), static_cast<uint32>(InVP.Height));
	ClearCommandList();
}

void FRenderer::EndScenePass()
{
}

void FRenderer::BindSwapChainRTV()
{
	if (RenderTargetView)
	{
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthTextureResources.DSV);
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

	FDepthStencilTextureResources DepthResources = {};
	if (!CreateDepthStencilTextureResources(Width, Height, DepthResources))
	{
		return false;
	}

	DepthTextureResources = DepthResources;
	return true;
}

bool FRenderer::CreateDepthStencilTextureResources(int32 Width, int32 Height, FDepthStencilTextureResources& OutResources)
{
	if (!Device)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &OutResources.Texture)))
	{
		ReleaseDepthStencilTextureResources(OutResources);
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;
	if (FAILED(Device->CreateDepthStencilView(OutResources.Texture, &DSVDesc, &OutResources.DSV)))
	{
		ReleaseDepthStencilTextureResources(OutResources);
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;
	if (FAILED(Device->CreateShaderResourceView(OutResources.Texture, &SRVDesc, &OutResources.DepthSRV)))
	{
		ReleaseDepthStencilTextureResources(OutResources);
		return false;
	}

	OutResources.Width = static_cast<uint32>(Width);
	OutResources.Height = static_cast<uint32>(Height);
	return true;
}

void FRenderer::ReleaseDepthStencilTextureResources(FDepthStencilTextureResources& InOutResources)
{
	IUnknown* Resource = reinterpret_cast<IUnknown*>(InOutResources.DepthSRV);
	ReleaseIfValid(Resource);
	InOutResources.DepthSRV = nullptr;

	Resource = reinterpret_cast<IUnknown*>(InOutResources.DSV);
	ReleaseIfValid(Resource);
	InOutResources.DSV = nullptr;

	Resource = reinterpret_cast<IUnknown*>(InOutResources.Texture);
	ReleaseIfValid(Resource);
	InOutResources.Texture = nullptr;
	InOutResources.Width = 0;
	InOutResources.Height = 0;
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
	if (!CreateHZBConstantBuffer()) return false;
	if (!CreateOcclusionConstantBuffer()) return false;
	SetConstantBuffers();
	ObjectUniformStream = std::make_unique<FObjectUniformStream>();
	if (!ObjectUniformStream->Initialize(Device, DeviceContext, ObjectConstantBuffer)) return false;
	SceneRenderer = std::make_unique<FSceneRenderer>(this);
	PassExecutor = std::make_unique<FPassExecutor>(this);
	CurrentRenderFrame = std::make_unique<FSceneRenderFrame>();

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"VertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"PixelShader.hlsl";

	if (!ShaderManager.LoadVertexShader(Device, VSPath.c_str())) return false;
	if (!ShaderManager.LoadPixelShader(Device, PSPath.c_str())) return false;
	if (!InitializeHZBShaders()) return false;

	/** 疫꿸퀡??Material ??밴쉐 */
	{
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		std::wstring ColorPSPath = ShaderDirW + L"ColorPixelShader.hlsl";
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, ColorPSPath.c_str());
		DefaultMaterial = std::make_shared<FMaterial>();
		DefaultMaterial->SetOriginName("M_Default");
		DefaultMaterial->SetVertexShader(VS);
		DefaultMaterial->SetPixelShader(PS);

		int32 SlotIndex = DefaultMaterial->CreateConstantBuffer(Device, 16);
		if (SlotIndex >= 0)
		{
			DefaultMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
			float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			DefaultMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));
		}
		FMaterialManager::Get().Register("M_Default", DefaultMaterial);
	}

	/** Texture ??Material ??밴쉐 */
	{
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		std::wstring TexturePSPath = ShaderDirW + L"TexturePixelShader.hlsl";
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, TexturePSPath.c_str());
		DefaultTextureMaterial = std::make_shared<FMaterial>();
		DefaultTextureMaterial->SetOriginName("M_Default");
		DefaultTextureMaterial->SetVertexShader(VS);
		DefaultTextureMaterial->SetPixelShader(PS);

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
	PollStaticMeshOcclusionReadback();
	FrameDrawCallCount = 0;
	FrameStaticMeshDrawCallCount = 0;
	FrameStaticMeshSkippedDrawCallCount = 0;
	FrameStaticMeshPotentialDrawCallCount = 0;
	FrameStaticMeshSkippedBeforeBuildDrawCommandsCount = 0;
	FrameStaticMeshSkippedLateDrawCount = 0;
	bStaticMeshOcclusionSkipActive = false;
	StaticMeshOcclusionSkipMask.clear();

	constexpr float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	if (RenderTargetView)
	{
		DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
	}

	if (DepthTextureResources.DSV)
	{
		DeviceContext->ClearDepthStencilView(DepthTextureResources.DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	ID3D11RenderTargetView* ActiveRTV = RenderTargetView;
	ID3D11DepthStencilView* ActiveDSV = DepthTextureResources.DSV;
	D3D11_VIEWPORT ActiveVP = Viewport;
	ActiveSceneDepthShaderResourceView = DepthTextureResources.DepthSRV;

	if (bUseSceneRenderTargetOverride)
	{
		ActiveRTV = SceneRenderTargetView;
		ActiveDSV = SceneDepthStencilView;
		ActiveSceneDepthShaderResourceView = SceneDepthShaderResourceView ? SceneDepthShaderResourceView : DepthTextureResources.DepthSRV;
		ActiveVP = SceneViewport;
		if (ActiveRTV && ActiveRTV != RenderTargetView) DeviceContext->ClearRenderTargetView(ActiveRTV, ClearColor);
		if (ActiveDSV && ActiveDSV != DepthTextureResources.DSV) DeviceContext->ClearDepthStencilView(ActiveDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	DeviceContext->OMSetRenderTargets(1, &ActiveRTV, ActiveDSV);
	DeviceContext->RSSetViewports(1, &ActiveVP);
	EnsureHZBResources(static_cast<uint32>(ActiveVP.Width), static_cast<uint32>(ActiveVP.Height));

	ClearCommandList();
	if (CurrentRenderFrame)
	{
		CurrentRenderFrame->Reset();
	}
	StaticMeshOcclusionSnapshot.Clear();
	GpuOcclusionCandidatesScratch.clear();
	if (ObjectUniformStream)
	{
		ObjectUniformStream->Reset();
	}
}

void FRenderer::ClearCommandList()
{
	PendingCommandQueue.Clear();
	PendingCommandQueue.Reserve(PrevCommandCount);
}

void FRenderer::EndFrame()
{
	if (RenderTargetView)
	{
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthTextureResources.DSV);
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
	PendingCommandQueue = Queue;
	ViewMatrix = Queue.ViewMatrix;
	ProjectionMatrix = Queue.ProjectionMatrix;
}

void FRenderer::SubmitCommands(FRenderCommandQueue&& Queue)
{
	PendingCommandQueue = std::move(Queue);
	ViewMatrix = PendingCommandQueue.ViewMatrix;
	ProjectionMatrix = PendingCommandQueue.ProjectionMatrix;
}

void FRenderer::ExecuteCommands()
{
	if (!SceneRenderer || !PassExecutor || !CurrentRenderFrame)
	{
		return;
	}

	const auto ExecuteStartTime = std::chrono::high_resolution_clock::now();
	BuildStaticMeshOcclusionSkipMask();
	SceneRenderer->BuildRenderFrame(PendingCommandQueue, *CurrentRenderFrame);
	ViewMatrix = CurrentRenderFrame->View.ViewMatrix;
	ProjectionMatrix = CurrentRenderFrame->View.ProjectionMatrix;
	const bool bGpuOcclusionEnabled = (GEngine && GEngine->IsGpuOcclusionCullingEnabled());

	SetConstantBuffers();
	UpdateFrameConstantBuffer();
	PassExecutor->Execute(*CurrentRenderFrame);
	if (bGpuOcclusionEnabled)
	{
		BuildHZB();
		ExecuteStaticMeshOcclusionPass();
	}
	else if (GEngine)
	{
		StaticMeshOcclusionSnapshot.Clear();
		GpuOcclusionCandidatesScratch.clear();
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.bHZBBuildSucceeded = false;
		Stats.HZBMipCount = 0;
		Stats.bOcclusionPassDispatched = false;
		Stats.OcclusionCandidateCount = 0;
	}
	const auto ExecuteEndTime = std::chrono::high_resolution_clock::now();
	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.ExecuteRenderCommandsCpuMs += std::chrono::duration<double, std::milli>(ExecuteEndTime - ExecuteStartTime).count();
		Stats.TotalDrawCallCount = FrameDrawCallCount;
		Stats.StaticMeshDrawCallCount = FrameStaticMeshDrawCallCount;
		Stats.StaticMeshDrawCallCountBeforeOcclusion = FrameStaticMeshPotentialDrawCallCount;
		Stats.StaticMeshDrawSkippedCount = FrameStaticMeshSkippedDrawCallCount;
		Stats.StaticMeshSkippedBeforeBuildDrawCommandsCount = FrameStaticMeshSkippedBeforeBuildDrawCommandsCount;
		Stats.StaticMeshSkippedLateDrawCount = FrameStaticMeshSkippedLateDrawCount;
		Stats.bOcclusionSkipApplied = bStaticMeshOcclusionSkipActive &&
			(FrameStaticMeshSkippedBeforeBuildDrawCommandsCount > 0 || FrameStaticMeshSkippedLateDrawCount > 0);
		if (bGpuOcclusionEnabled)
		{
			Stats.HZBMipCount = HZBResources.MipCount;
		}
	}

	if (PostRenderCallback) PostRenderCallback(this);

	PrevCommandCount = PendingCommandQueue.Commands.size();
	ClearCommandList();
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

ID3D11DepthStencilView* FRenderer::GetDepthStencilView() const
{
	return DepthTextureResources.DSV;
}

ID3D11ShaderResourceView* FRenderer::GetDepthShaderResourceView() const
{
	return DepthTextureResources.DepthSRV;
}

uint32 FRenderer::GetFrameStaticMeshDrawCallCount() const
{
	return FrameStaticMeshDrawCallCount;
}

void FRenderer::BuildStaticMeshOcclusionSkipMask()
{
	bStaticMeshOcclusionSkipActive = false;
	StaticMeshOcclusionSkipMask.clear();

	if (!GEngine || !GEngine->IsGpuOcclusionCullingEnabled())
	{
		return;
	}

	const FStaticMeshOcclusionReadbackResult& ReadbackResult = LatestStaticMeshOcclusionReadbackResult;
	const TArray<FStaticMeshOcclusionCandidate>& CurrentCandidates = PendingCommandQueue.StaticMeshOcclusionCandidates;
	if (!ReadbackResult.bReady || !ReadbackResult.bSizeMatched || CurrentCandidates.empty())
	{
		return;
	}

	if (ReadbackResult.CandidateCount != CurrentCandidates.size() ||
		ReadbackResult.Snapshot.CandidateKeys.size() != CurrentCandidates.size() ||
		ReadbackResult.VisibilityValues.size() != CurrentCandidates.size())
	{
		return;
	}

	StaticMeshOcclusionSkipMask.resize(CurrentCandidates.size(), 0);
	uint32 MatchedCandidateCount = 0;
	uint32 SkippableCandidateCount = 0;

	for (size_t CandidateIndex = 0; CandidateIndex < CurrentCandidates.size(); ++CandidateIndex)
	{
		const FStaticMeshOcclusionCandidate& CurrentCandidate = CurrentCandidates[CandidateIndex];
		if (CurrentCandidate.MatchKey != ReadbackResult.Snapshot.CandidateKeys[CandidateIndex])
		{
			continue;
		}

		++MatchedCandidateCount;
		if (ReadbackResult.VisibilityValues[CandidateIndex] == 0)
		{
			StaticMeshOcclusionSkipMask[CandidateIndex] = 1;
			++SkippableCandidateCount;
		}
	}

	bStaticMeshOcclusionSkipActive = (MatchedCandidateCount > 0 && SkippableCandidateCount > 0);
}

bool FRenderer::ShouldSkipStaticMeshDraw(const FMeshDrawCommand& Command) const
{
	if (!bStaticMeshOcclusionSkipActive || !Command.bStaticMesh)
	{
		return false;
	}

	const uint32 CandidateIndex = Command.StaticMeshOcclusionCandidateIndex;
	if (CandidateIndex == GInvalidOcclusionCandidateIndex || CandidateIndex >= StaticMeshOcclusionSkipMask.size())
	{
		return false;
	}

	return StaticMeshOcclusionSkipMask[CandidateIndex] != 0;
}

bool FRenderer::ShouldSkipStaticMeshCandidate(uint32 CandidateIndex) const
{
	if (!bStaticMeshOcclusionSkipActive)
	{
		return false;
	}

	if (CandidateIndex == GInvalidOcclusionCandidateIndex || CandidateIndex >= StaticMeshOcclusionSkipMask.size())
	{
		return false;
	}

	return StaticMeshOcclusionSkipMask[CandidateIndex] != 0;
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

bool FRenderer::CreateHZBConstantBuffer()
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.ByteWidth = sizeof(FHZBBuildConstants);
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &HZBConstantBuffer));
}

bool FRenderer::CreateOcclusionConstantBuffer()
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.ByteWidth = sizeof(FOcclusionPassConstants);
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &OcclusionConstantBuffer));
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

bool FRenderer::EnsureHZBResources(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0 || !Device)
	{
		return false;
	}

	uint32 DesiredMipCount = 1;
	uint32 MipWidth = Width;
	uint32 MipHeight = Height;
	while (MipWidth > 1 || MipHeight > 1)
	{
		MipWidth = (std::max)(MipWidth >> 1, 1u);
		MipHeight = (std::max)(MipHeight >> 1, 1u);
		++DesiredMipCount;
	}

	if (HZBResources.Texture &&
		HZBResources.Width == Width &&
		HZBResources.Height == Height &&
		HZBResources.MipCount == DesiredMipCount)
	{
		return true;
	}

	ReleaseHZBResources();

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = DesiredMipCount;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R32_FLOAT;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &HZBResources.Texture)))
	{
		ReleaseHZBResources();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC FullSRVDesc = {};
	FullSRVDesc.Format = Desc.Format;
	FullSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	FullSRVDesc.Texture2D.MostDetailedMip = 0;
	FullSRVDesc.Texture2D.MipLevels = DesiredMipCount;
	if (FAILED(Device->CreateShaderResourceView(HZBResources.Texture, &FullSRVDesc, &HZBResources.FullSRV)))
	{
		ReleaseHZBResources();
		return false;
	}

	HZBResources.MipSRVs.reserve(DesiredMipCount);
	HZBResources.MipUAVs.reserve(DesiredMipCount);

	for (uint32 MipIndex = 0; MipIndex < DesiredMipCount; ++MipIndex)
	{
		ID3D11ShaderResourceView* MipSRV = nullptr;
		D3D11_SHADER_RESOURCE_VIEW_DESC MipSRVDesc = {};
		MipSRVDesc.Format = Desc.Format;
		MipSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		MipSRVDesc.Texture2D.MostDetailedMip = MipIndex;
		MipSRVDesc.Texture2D.MipLevels = 1;
		if (FAILED(Device->CreateShaderResourceView(HZBResources.Texture, &MipSRVDesc, &MipSRV)))
		{
			ReleaseHZBResources();
			return false;
		}
		HZBResources.MipSRVs.push_back(MipSRV);

		ID3D11UnorderedAccessView* MipUAV = nullptr;
		D3D11_UNORDERED_ACCESS_VIEW_DESC MipUAVDesc = {};
		MipUAVDesc.Format = Desc.Format;
		MipUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		MipUAVDesc.Texture2D.MipSlice = MipIndex;
		if (FAILED(Device->CreateUnorderedAccessView(HZBResources.Texture, &MipUAVDesc, &MipUAV)))
		{
			ReleaseHZBResources();
			return false;
		}
		HZBResources.MipUAVs.push_back(MipUAV);
	}

	HZBResources.Width = Width;
	HZBResources.Height = Height;
	HZBResources.MipCount = DesiredMipCount;
	return true;
}

void FRenderer::ReleaseHZBResources()
{
	for (ID3D11ShaderResourceView* SRV : HZBResources.MipSRVs)
	{
		IUnknown* Resource = reinterpret_cast<IUnknown*>(SRV);
		ReleaseIfValid(Resource);
	}
	HZBResources.MipSRVs.clear();

	for (ID3D11UnorderedAccessView* UAV : HZBResources.MipUAVs)
	{
		IUnknown* Resource = reinterpret_cast<IUnknown*>(UAV);
		ReleaseIfValid(Resource);
	}
	HZBResources.MipUAVs.clear();

	IUnknown* Resource = reinterpret_cast<IUnknown*>(HZBResources.FullSRV);
	ReleaseIfValid(Resource);
	HZBResources.FullSRV = nullptr;

	Resource = reinterpret_cast<IUnknown*>(HZBResources.Texture);
	ReleaseIfValid(Resource);
	HZBResources.Texture = nullptr;

	HZBResources.Width = 0;
	HZBResources.Height = 0;
	HZBResources.MipCount = 0;
}

void FRenderer::BuildHZB()
{
	if (!DeviceContext || !ActiveSceneDepthShaderResourceView || !HZBResources.Texture || !HZBConstantBuffer)
	{
		if (GEngine)
		{
			FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
			Stats.bHZBBuildSucceeded = false;
			Stats.HZBMipCount = HZBResources.MipCount;
		}
		return;
	}

	if (!InitializeHZBShaders() || !HZBInitializeComputeShader || !HZBReduceComputeShader || HZBResources.MipUAVs.empty())
	{
		if (GEngine)
		{
			FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
			Stats.bHZBBuildSucceeded = false;
			Stats.HZBMipCount = HZBResources.MipCount;
		}
		return;
	}

	const auto BuildStartTime = std::chrono::high_resolution_clock::now();

	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	ID3D11RenderTargetView* BoundRTV = nullptr;
	ID3D11DepthStencilView* BoundDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &BoundRTV, &BoundDSV);
	if (BoundRTV)
	{
		DeviceContext->OMSetRenderTargets(1, &BoundRTV, nullptr);
	}
	else
	{
		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	}

	FHZBBuildConstants Constants = {};
	Constants.SourceWidth = HZBResources.Width;
	Constants.SourceHeight = HZBResources.Height;
	Constants.DestWidth = HZBResources.Width;
	Constants.DestHeight = HZBResources.Height;
	UpdateHZBConstantBuffer(Constants);

	ID3D11Buffer* HZBCB = HZBConstantBuffer;
	DeviceContext->CSSetConstantBuffers(0, 1, &HZBCB);
	DeviceContext->CSSetShaderResources(0, 1, &ActiveSceneDepthShaderResourceView);
	ID3D11UnorderedAccessView* Mip0UAV = HZBResources.MipUAVs[0];
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &Mip0UAV, nullptr);
	HZBInitializeComputeShader->Dispatch(DeviceContext, (HZBResources.Width + 7) / 8, (HZBResources.Height + 7) / 8, 1);
	DeviceContext->CSSetShaderResources(0, 1, &NullSRV);
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);

	for (uint32 MipIndex = 1; MipIndex < HZBResources.MipCount; ++MipIndex)
	{
		const uint32 PrevMipWidth = (std::max)(HZBResources.Width >> (MipIndex - 1), 1u);
		const uint32 PrevMipHeight = (std::max)(HZBResources.Height >> (MipIndex - 1), 1u);
		const uint32 CurrMipWidth = (std::max)(HZBResources.Width >> MipIndex, 1u);
		const uint32 CurrMipHeight = (std::max)(HZBResources.Height >> MipIndex, 1u);

		FHZBBuildConstants ReduceConstants = {};
		ReduceConstants.SourceWidth = PrevMipWidth;
		ReduceConstants.SourceHeight = PrevMipHeight;
		ReduceConstants.DestWidth = CurrMipWidth;
		ReduceConstants.DestHeight = CurrMipHeight;
		UpdateHZBConstantBuffer(ReduceConstants);

		ID3D11ShaderResourceView* PrevMipSRV = HZBResources.MipSRVs[MipIndex - 1];
		ID3D11UnorderedAccessView* CurrMipUAV = HZBResources.MipUAVs[MipIndex];
		DeviceContext->CSSetConstantBuffers(0, 1, &HZBCB);
		DeviceContext->CSSetShaderResources(0, 1, &PrevMipSRV);
		DeviceContext->CSSetUnorderedAccessViews(0, 1, &CurrMipUAV, nullptr);
		HZBReduceComputeShader->Dispatch(DeviceContext, (CurrMipWidth + 7) / 8, (CurrMipHeight + 7) / 8, 1);
		DeviceContext->CSSetShaderResources(0, 1, &NullSRV);
		DeviceContext->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
	}

	ID3D11Buffer* NullCB = nullptr;
	DeviceContext->CSSetConstantBuffers(0, 1, &NullCB);
	DeviceContext->CSSetShader(nullptr, nullptr, 0);
	if (BoundRTV || BoundDSV)
	{
		DeviceContext->OMSetRenderTargets(1, &BoundRTV, BoundDSV);
	}
	if (BoundRTV)
	{
		BoundRTV->Release();
	}
	if (BoundDSV)
	{
		BoundDSV->Release();
	}

	const auto BuildEndTime = std::chrono::high_resolution_clock::now();
	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.bHZBBuildSucceeded = true;
		Stats.HZBMipCount = HZBResources.MipCount;
		Stats.HZBBuildCpuMs += std::chrono::duration<double, std::milli>(BuildEndTime - BuildStartTime).count();
	}
}

bool FRenderer::InitializeHZBShaders()
{
	if (HZBInitializeComputeShader && HZBReduceComputeShader)
	{
		return true;
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	if (!HZBInitializeComputeShader)
	{
		HZBInitializeComputeShader = FShaderMap::Get().GetOrCreateComputeShader(Device, (ShaderDir + L"HZBInitializeComputeShader.hlsl").c_str());
	}
	if (!HZBReduceComputeShader)
	{
		HZBReduceComputeShader = FShaderMap::Get().GetOrCreateComputeShader(Device, (ShaderDir + L"HZBReduceComputeShader.hlsl").c_str());
	}

	return HZBInitializeComputeShader != nullptr && HZBReduceComputeShader != nullptr;
}

void FRenderer::UpdateHZBConstantBuffer(const FHZBBuildConstants& Constants)
{
	if (!DeviceContext || !HZBConstantBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (SUCCEEDED(DeviceContext->Map(HZBConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &Constants, sizeof(Constants));
		DeviceContext->Unmap(HZBConstantBuffer, 0);
	}
}

bool FRenderer::EnsureStaticMeshOcclusionResources(uint32 CandidateCount)
{
	if (!Device || CandidateCount == 0)
	{
		return true;
	}

	if (StaticMeshOcclusionResources.CandidateBuffer &&
		StaticMeshOcclusionResources.CandidateSRV &&
		StaticMeshOcclusionResources.VisibilityBuffer &&
		StaticMeshOcclusionResources.VisibilityUAV &&
		StaticMeshOcclusionResources.CandidateCapacity >= CandidateCount)
	{
		return true;
	}

	ReleaseStaticMeshOcclusionResources();

	D3D11_BUFFER_DESC CandidateDesc = {};
	CandidateDesc.ByteWidth = sizeof(FGpuOcclusionCandidate) * CandidateCount;
	CandidateDesc.Usage = D3D11_USAGE_DYNAMIC;
	CandidateDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	CandidateDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CandidateDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	CandidateDesc.StructureByteStride = sizeof(FGpuOcclusionCandidate);
	if (FAILED(Device->CreateBuffer(&CandidateDesc, nullptr, &StaticMeshOcclusionResources.CandidateBuffer)))
	{
		ReleaseStaticMeshOcclusionResources();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC CandidateSRVDesc = {};
	CandidateSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	CandidateSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	CandidateSRVDesc.Buffer.FirstElement = 0;
	CandidateSRVDesc.Buffer.NumElements = CandidateCount;
	if (FAILED(Device->CreateShaderResourceView(StaticMeshOcclusionResources.CandidateBuffer, &CandidateSRVDesc, &StaticMeshOcclusionResources.CandidateSRV)))
	{
		ReleaseStaticMeshOcclusionResources();
		return false;
	}

	D3D11_BUFFER_DESC VisibilityDesc = {};
	VisibilityDesc.ByteWidth = sizeof(uint32) * CandidateCount;
	VisibilityDesc.Usage = D3D11_USAGE_DEFAULT;
	VisibilityDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	VisibilityDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	VisibilityDesc.StructureByteStride = sizeof(uint32);
	if (FAILED(Device->CreateBuffer(&VisibilityDesc, nullptr, &StaticMeshOcclusionResources.VisibilityBuffer)))
	{
		ReleaseStaticMeshOcclusionResources();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC VisibilityUAVDesc = {};
	VisibilityUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	VisibilityUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	VisibilityUAVDesc.Buffer.FirstElement = 0;
	VisibilityUAVDesc.Buffer.NumElements = CandidateCount;
	if (FAILED(Device->CreateUnorderedAccessView(StaticMeshOcclusionResources.VisibilityBuffer, &VisibilityUAVDesc, &StaticMeshOcclusionResources.VisibilityUAV)))
	{
		ReleaseStaticMeshOcclusionResources();
		return false;
	}

	StaticMeshOcclusionResources.CandidateCapacity = CandidateCount;
	return true;
}

void FRenderer::ReleaseStaticMeshOcclusionResources()
{
	IUnknown* Resource = reinterpret_cast<IUnknown*>(StaticMeshOcclusionResources.CandidateSRV);
	ReleaseIfValid(Resource);
	StaticMeshOcclusionResources.CandidateSRV = nullptr;

	Resource = reinterpret_cast<IUnknown*>(StaticMeshOcclusionResources.CandidateBuffer);
	ReleaseIfValid(Resource);
	StaticMeshOcclusionResources.CandidateBuffer = nullptr;

	Resource = reinterpret_cast<IUnknown*>(StaticMeshOcclusionResources.VisibilityUAV);
	ReleaseIfValid(Resource);
	StaticMeshOcclusionResources.VisibilityUAV = nullptr;

	Resource = reinterpret_cast<IUnknown*>(StaticMeshOcclusionResources.VisibilityBuffer);
	ReleaseIfValid(Resource);
	StaticMeshOcclusionResources.VisibilityBuffer = nullptr;

	StaticMeshOcclusionResources.CandidateCapacity = 0;
}

bool FRenderer::EnsureStaticMeshOcclusionReadbackBuffer(FStaticMeshOcclusionReadbackSlot& InOutSlot, uint32 CandidateCount)
{
	if (!Device || CandidateCount == 0)
	{
		return true;
	}

	if (InOutSlot.StagingBuffer && InOutSlot.CandidateCapacity >= CandidateCount)
	{
		return true;
	}

	IUnknown* Resource = reinterpret_cast<IUnknown*>(InOutSlot.StagingBuffer);
	ReleaseIfValid(Resource);
	InOutSlot.StagingBuffer = nullptr;
	InOutSlot.CandidateCapacity = 0;
	InOutSlot.ClearFrameState();

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = sizeof(uint32) * CandidateCount;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = sizeof(uint32);

	if (FAILED(Device->CreateBuffer(&Desc, nullptr, &InOutSlot.StagingBuffer)))
	{
		return false;
	}

	InOutSlot.CandidateCapacity = CandidateCount;
	return true;
}

FStaticMeshOcclusionReadbackSlot* FRenderer::AcquireStaticMeshOcclusionReadbackSlot()
{
	for (uint32 Attempt = 0; Attempt < OcclusionReadbackSlotCount; ++Attempt)
	{
		const uint32 SlotIndex = (NextOcclusionReadbackSlotIndex + Attempt) % OcclusionReadbackSlotCount;
		FStaticMeshOcclusionReadbackSlot& Slot = StaticMeshOcclusionReadbackSlots[SlotIndex];
		if (!Slot.bCopyIssued)
		{
			NextOcclusionReadbackSlotIndex = (SlotIndex + 1) % OcclusionReadbackSlotCount;
			return &Slot;
		}
	}

	return nullptr;
}

void FRenderer::IssueStaticMeshOcclusionReadback()
{
	if (!DeviceContext || !StaticMeshOcclusionResources.VisibilityBuffer)
	{
		return;
	}

	const uint32 CandidateCount = static_cast<uint32>(StaticMeshOcclusionSnapshot.CandidateKeys.size());
	if (CandidateCount == 0)
	{
		return;
	}

	FStaticMeshOcclusionReadbackSlot* Slot = AcquireStaticMeshOcclusionReadbackSlot();
	if (!Slot)
	{
		return;
	}

	if (!EnsureStaticMeshOcclusionReadbackBuffer(*Slot, CandidateCount) || !Slot->StagingBuffer)
	{
		return;
	}

	D3D11_BOX CopyRegion = {};
	CopyRegion.left = 0;
	CopyRegion.right = sizeof(uint32) * CandidateCount;
	CopyRegion.top = 0;
	CopyRegion.bottom = 1;
	CopyRegion.front = 0;
	CopyRegion.back = 1;
	DeviceContext->CopySubresourceRegion(Slot->StagingBuffer, 0, 0, 0, 0, StaticMeshOcclusionResources.VisibilityBuffer, 0, &CopyRegion);

	Slot->Snapshot = std::move(StaticMeshOcclusionSnapshot);
	Slot->CandidateCount = CandidateCount;
	Slot->FrameSerial = ++OcclusionFrameSerial;
	Slot->bCopyIssued = true;
	Slot->bCompleted = false;
	Slot->VisibilityValues.clear();
	Slot->VisibleCount = 0;
	Slot->OccludedCount = 0;

	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.bOcclusionReadbackIssued = true;
		Stats.OcclusionReadbackCandidateCount = CandidateCount;
	}
}

void FRenderer::PollStaticMeshOcclusionReadback()
{
	if (!DeviceContext)
	{
		return;
	}

	for (FStaticMeshOcclusionReadbackSlot& Slot : StaticMeshOcclusionReadbackSlots)
	{
		if (!Slot.bCopyIssued || !Slot.StagingBuffer)
		{
			continue;
		}

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		const HRESULT Hr = DeviceContext->Map(Slot.StagingBuffer, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &Mapped);
		if (Hr == DXGI_ERROR_WAS_STILL_DRAWING)
		{
			continue;
		}

		if (FAILED(Hr))
		{
			Slot.ClearFrameState();
			continue;
		}

		Slot.VisibilityValues.resize(Slot.CandidateCount);
		memcpy(Slot.VisibilityValues.data(), Mapped.pData, sizeof(uint32) * Slot.CandidateCount);
		DeviceContext->Unmap(Slot.StagingBuffer, 0);

		Slot.VisibleCount = 0;
		for (uint32 VisibilityValue : Slot.VisibilityValues)
		{
			if (VisibilityValue != 0)
			{
				++Slot.VisibleCount;
			}
		}
		Slot.OccludedCount = Slot.CandidateCount >= Slot.VisibleCount ? (Slot.CandidateCount - Slot.VisibleCount) : 0;
		Slot.bCompleted = true;

		LatestStaticMeshOcclusionReadbackResult.Snapshot = std::move(Slot.Snapshot);
		LatestStaticMeshOcclusionReadbackResult.VisibilityValues = std::move(Slot.VisibilityValues);
		LatestStaticMeshOcclusionReadbackResult.CandidateCount = Slot.CandidateCount;
		LatestStaticMeshOcclusionReadbackResult.VisibleCount = Slot.VisibleCount;
		LatestStaticMeshOcclusionReadbackResult.OccludedCount = Slot.OccludedCount;
		LatestStaticMeshOcclusionReadbackResult.FrameSerial = Slot.FrameSerial;
		LatestStaticMeshOcclusionReadbackResult.bReady = true;
		LatestStaticMeshOcclusionReadbackResult.bSizeMatched =
			(LatestStaticMeshOcclusionReadbackResult.Snapshot.CandidateKeys.size() == LatestStaticMeshOcclusionReadbackResult.VisibilityValues.size());

		if (GEngine)
		{
			FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
			Stats.bOcclusionReadbackCompleted = true;
			Stats.OcclusionReadbackCandidateCount = Slot.CandidateCount;
			Stats.OcclusionVisibleCount = Slot.VisibleCount;
			Stats.OcclusionOccludedCount = Slot.OccludedCount;
			Stats.bOcclusionSnapshotReadbackMatched = LatestStaticMeshOcclusionReadbackResult.bSizeMatched;
		}

		Slot.ClearFrameState();
	}
}

void FRenderer::ReleaseStaticMeshOcclusionReadbackResources()
{
	for (FStaticMeshOcclusionReadbackSlot& Slot : StaticMeshOcclusionReadbackSlots)
	{
		IUnknown* Resource = reinterpret_cast<IUnknown*>(Slot.StagingBuffer);
		ReleaseIfValid(Resource);
		Slot.StagingBuffer = nullptr;
		Slot.CandidateCapacity = 0;
		Slot.ClearFrameState();
	}

	LatestStaticMeshOcclusionReadbackResult.Reset();
	NextOcclusionReadbackSlotIndex = 0;
	OcclusionFrameSerial = 0;
}

bool FRenderer::InitializeOcclusionShaders()
{
	if (StaticMeshOcclusionComputeShader)
	{
		return true;
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	StaticMeshOcclusionComputeShader = FShaderMap::Get().GetOrCreateComputeShader(Device, (ShaderDir + L"StaticMeshOcclusionComputeShader.hlsl").c_str());
	return StaticMeshOcclusionComputeShader != nullptr;
}

void FRenderer::BuildStaticMeshOcclusionSnapshot()
{
	StaticMeshOcclusionSnapshot.Clear();
	GpuOcclusionCandidatesScratch.clear();

	const TArray<FStaticMeshOcclusionCandidate>& SourceCandidates = PendingCommandQueue.StaticMeshOcclusionCandidates;
	StaticMeshOcclusionSnapshot.Reserve(SourceCandidates.size());
	GpuOcclusionCandidatesScratch.reserve(SourceCandidates.size());

	for (size_t SourceIndex = 0; SourceIndex < SourceCandidates.size(); ++SourceIndex)
	{
		const FStaticMeshOcclusionCandidate& SourceCandidate = SourceCandidates[SourceIndex];
		const uint32 DenseIndex = static_cast<uint32>(SourceIndex);

		StaticMeshOcclusionSnapshot.CandidateKeys.push_back(SourceCandidate.MatchKey);

		FGpuOcclusionCandidate GpuCandidate = {};
		GpuCandidate.BoundsCenter = SourceCandidate.BoundsCenter;
		GpuCandidate.BoundsRadius = SourceCandidate.BoundsRadius;
		GpuCandidate.BoundsExtent = SourceCandidate.BoundsExtent;
		GpuCandidate.DenseIndex = DenseIndex;
		GpuOcclusionCandidatesScratch.push_back(GpuCandidate);
	}

	if (GEngine)
	{
		GEngine->GetMutableRenderInstrumentationStats().OcclusionCandidateCount = static_cast<uint32>(SourceCandidates.size());
	}
}

bool FRenderer::UploadStaticMeshOcclusionCandidates()
{
	if (!DeviceContext || GpuOcclusionCandidatesScratch.empty() || !StaticMeshOcclusionResources.CandidateBuffer)
	{
		return GpuOcclusionCandidatesScratch.empty();
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(StaticMeshOcclusionResources.CandidateBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	memcpy(Mapped.pData, GpuOcclusionCandidatesScratch.data(), sizeof(FGpuOcclusionCandidate) * GpuOcclusionCandidatesScratch.size());
	DeviceContext->Unmap(StaticMeshOcclusionResources.CandidateBuffer, 0);
	return true;
}

void FRenderer::UpdateOcclusionConstantBuffer(const FOcclusionPassConstants& Constants)
{
	if (!DeviceContext || !OcclusionConstantBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (SUCCEEDED(DeviceContext->Map(OcclusionConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &Constants, sizeof(Constants));
		DeviceContext->Unmap(OcclusionConstantBuffer, 0);
	}
}

void FRenderer::ExecuteStaticMeshOcclusionPass()
{
	BuildStaticMeshOcclusionSnapshot();
	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.bOcclusionPassDispatched = false;
		Stats.bOcclusionReadbackIssued = false;
	}

	if (GpuOcclusionCandidatesScratch.empty())
	{
		return;
	}

	if (GEngine && !GEngine->GetRenderInstrumentationStats().bHZBBuildSucceeded)
	{
		return;
	}

	if (!DeviceContext || !HZBResources.FullSRV || !OcclusionConstantBuffer)
	{
		return;
	}

	if (!InitializeOcclusionShaders() || !StaticMeshOcclusionComputeShader)
	{
		return;
	}

	const uint32 CandidateCount = static_cast<uint32>(GpuOcclusionCandidatesScratch.size());
	if (!EnsureStaticMeshOcclusionResources(CandidateCount) || !UploadStaticMeshOcclusionCandidates())
	{
		return;
	}

	const auto DispatchStartTime = std::chrono::high_resolution_clock::now();

	FOcclusionPassConstants Constants = {};
	Constants.View = ViewMatrix.GetTransposed();
	Constants.Projection = ProjectionMatrix.GetTransposed();
	Constants.ViewProjection = (ViewMatrix * ProjectionMatrix).GetTransposed();
	Constants.ViewWidth = HZBResources.Width;
	Constants.ViewHeight = HZBResources.Height;
	Constants.CandidateCount = CandidateCount;
	Constants.HZBMipCount = HZBResources.MipCount;
	UpdateOcclusionConstantBuffer(Constants);

	ID3D11ShaderResourceView* SRVs[2] =
	{
		HZBResources.FullSRV,
		StaticMeshOcclusionResources.CandidateSRV
	};
	ID3D11UnorderedAccessView* VisibilityUAV = StaticMeshOcclusionResources.VisibilityUAV;
	ID3D11Buffer* OcclusionCB = OcclusionConstantBuffer;
	DeviceContext->CSSetConstantBuffers(0, 1, &OcclusionCB);
	DeviceContext->CSSetShaderResources(0, 2, SRVs);
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &VisibilityUAV, nullptr);
	StaticMeshOcclusionComputeShader->Dispatch(DeviceContext, (CandidateCount + 63) / 64, 1, 1);

	ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	ID3D11Buffer* NullCB = nullptr;
	DeviceContext->CSSetShaderResources(0, 2, NullSRVs);
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
	DeviceContext->CSSetConstantBuffers(0, 1, &NullCB);
	DeviceContext->CSSetShader(nullptr, nullptr, 0);

	const auto DispatchEndTime = std::chrono::high_resolution_clock::now();
	if (GEngine)
	{
		FRenderInstrumentationStats& Stats = GEngine->GetMutableRenderInstrumentationStats();
		Stats.bOcclusionPassDispatched = true;
		Stats.OcclusionDispatchCpuMs += std::chrono::duration<double, std::milli>(DispatchEndTime - DispatchStartTime).count();
	}

	IssueStaticMeshOcclusionReadback();
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
	return FTextureLoader::CreateTextureFromSTB(Device, FilePath, OutSRV);
}

bool FRenderer::CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV)
{
	return FTextureLoader::CreateTextureFromSTB(Device, FilePath, OutSRV);
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

	TArray<uint32> ObjectAllocations;
	if (ObjectUniformStream)
	{
		ObjectAllocations.reserve(Items.size());
		for (const FOutlineRenderItem& Item : Items)
		{
			const uint32 AllocationIndex = ObjectUniformStream->AllocateWorldMatrix(Item.WorldMatrix);
			ObjectAllocations.push_back(AllocationIndex);
		}
		ObjectUniformStream->UploadFrame();
	}

	for (size_t ItemIndex = 0; ItemIndex < Items.size(); ++ItemIndex)
	{
		const FOutlineRenderItem& Item = Items[ItemIndex];
		if (!Item.Mesh)
		{
			continue;
		}

		if (Item.Mesh->NeedsBufferUpload() && !Item.Mesh->UpdateVertexAndIndexBuffer(Device, DeviceContext))
		{
			continue;
		}

		Item.Mesh->Bind(DeviceContext);
		if (Item.Mesh->Topology != EMeshTopology::EMT_Undefined)
		{
			DeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)Item.Mesh->Topology);
		}

		if (ObjectUniformStream && ItemIndex < ObjectAllocations.size())
		{
			ObjectUniformStream->BindAllocation(ObjectAllocations[ItemIndex]);
		}
		else
		{
			UpdateObjectConstantBuffer(Item.WorldMatrix);
		}

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
	if (ObjectUniformStream)
	{
		const uint32 AllocationIndex = ObjectUniformStream->AllocateWorldMatrix(FMatrix::Identity);
		ObjectUniformStream->UploadFrame();
		ObjectUniformStream->BindAllocation(AllocationIndex);
	}
	else
	{
		UpdateObjectConstantBuffer(FMatrix::Identity);
	}
	++FrameDrawCallCount;
	DeviceContext->Draw(static_cast<UINT>(LineVertices.size()), 0);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	LineVertices.clear();
}

void FRenderer::Release()
{
	ClearViewportCallbacks(); ClearSceneRenderTarget();
	CurrentRenderFrame.reset();
	SceneRenderer.reset();
	PassExecutor.reset();
	if (ObjectUniformStream)
	{
		ObjectUniformStream->Release();
		ObjectUniformStream.reset();
	}
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
	ReleaseHZBResources();
	ReleaseStaticMeshOcclusionResources();
	ReleaseStaticMeshOcclusionReadbackResources();
	DefaultMaterial.reset();
	if (FolderIconSRV)FolderIconSRV->Release();
	if (FileIconSRV)FileIconSRV->Release();
	if (LineVertexBuffer) LineVertexBuffer->Release();
	if (FrameConstantBuffer) FrameConstantBuffer->Release();
	if (ObjectConstantBuffer) ObjectConstantBuffer->Release();
	if (OutlinePostConstantBuffer) OutlinePostConstantBuffer->Release();
	if (HZBConstantBuffer) HZBConstantBuffer->Release();
	if (OcclusionConstantBuffer) OcclusionConstantBuffer->Release();
	ReleaseDepthStencilTextureResources(DepthTextureResources);
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
	ReleaseDepthStencilTextureResources(DepthTextureResources);
	ReleaseOutlineMaskResources();
	ReleaseHZBResources();
	ReleaseStaticMeshOcclusionResources();
	ReleaseStaticMeshOcclusionReadbackResources();
	const UINT ResizeFlags = bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	SwapChain->ResizeBuffers(0, W, H, DXGI_FORMAT_UNKNOWN, ResizeFlags);
	CreateRenderTargetAndDepthStencil(W, H);
	Viewport.Width = static_cast<float>(W); Viewport.Height = static_cast<float>(H);
}

