#include "D3DDevice.h"

//	Safe Release Macro
#define SAFE_RELEASE(Obj) if (Obj) { Obj->Release(); Obj = nullptr; }


void FD3DDevice::Create(HWND InHWindow)
{
	CreateDeviceAndSwapChain(InHWindow);
	CreateFrameBuffer();
	CreateRasterizerState();
	CreateDepthStencilBuffer();
	CreateBlendState();
}

void FD3DDevice::Release()
{
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseBlendState();
	ReleaseDepthStencilBuffer();
	ReleaseRasterizerState();
	ReleaseFrameBuffer();

	ReleaseDeviceAndSwapChain();
}

void FD3DDevice::BeginFrame()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);

	SetRasterizerState(ERasterizerState::SolidBackCull);
	SetDepthStencilState(EDepthStencilState::Default);
	SetBlendState(EBlendState::Opaque);

	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
}


void FD3DDevice::EndFrame()
{
	UINT PresentFlags = bTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
	SwapChain->Present(0, PresentFlags);
}

void FD3DDevice::OnResizeViewport(int Width, int Height)
{
	if (!SwapChain) return;

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	ReleaseDepthStencilBuffer();

	SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainFlags);

	ViewportInfo.Width = static_cast<float>(Width);
	ViewportInfo.Height = static_cast<float>(Height);

	CreateFrameBuffer();
	CreateDepthStencilBuffer();

	// 상태 캐시 초기화 — 새로 생성된 state 객체가 BeginFrame에서 재적용되도록
	CurrentRasterizerState = static_cast<ERasterizerState>(-1);
	CurrentDepthStencilState = static_cast<EDepthStencilState>(-1);
	CurrentBlendState = static_cast<EBlendState>(-1);
}


ID3D11Device* FD3DDevice::GetDevice() const
{
	return Device;
}

ID3D11DeviceContext* FD3DDevice::GetDeviceContext() const
{
	return DeviceContext;
}

void FD3DDevice::SetDepthStencilState(EDepthStencilState InState)
{
	if (CurrentDepthStencilState == InState) return;

	switch (InState)
	{
	case EDepthStencilState::Default:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateDefault, 0);
		break;
	case EDepthStencilState::DepthGreater:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateDepthGreater, 0);
		break;
	case EDepthStencilState::DepthReadOnly:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateDepthReadOnly, 0);
		break;
	case EDepthStencilState::StencilWrite:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilWrite, 1);
		break;
	case EDepthStencilState::StencilOutline:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilOutline, 1);
		break;
	case EDepthStencilState::StencilWriteOnlyEqual:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilMaskEqual, 1);
		break;
	case EDepthStencilState::GizmoInside:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateGizmoInside, 1);
		break;
	case EDepthStencilState::GizmoOutside:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateGizmoOutside, 1);
		break;
	}

	CurrentDepthStencilState = InState;
}

void FD3DDevice::SetBlendState(EBlendState InState)
{
	if (CurrentBlendState == InState) return;
	const float BlendFactor[4] = { 0, 0, 0, 0 };
	CurrentBlendState = InState;

	switch (CurrentBlendState)
	{
	case EBlendState::Opaque:
		DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
		break;

	case EBlendState::AlphaBlend:
		DeviceContext->OMSetBlendState(BlendStateAlpha, BlendFactor, 0xffffffff);
		break;

	case EBlendState::NoColor:
		DeviceContext->OMSetBlendState(BlendStateNoColorWrite, BlendFactor, 0xFFFFFFFF);
		break;
	}
}

void FD3DDevice::SetRasterizerState(ERasterizerState InState)
{
	if (CurrentRasterizerState == InState) return;

	switch (InState)
	{
	case ERasterizerState::SolidBackCull:
		DeviceContext->RSSetState(RasterizerStateBackCull);
		break;
	case ERasterizerState::SolidFrontCull:
		DeviceContext->RSSetState(RasterizerStateFrontCull);
		break;
	case ERasterizerState::WireFrame:
		DeviceContext->RSSetState(RasterizerStateWireFrame);
		break;
	}

	CurrentRasterizerState = InState;
}

void FD3DDevice::CreateDeviceAndSwapChain(HWND InHWindow)
{
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.OutputWindow = InHWindow;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// Check tearing support for no-vsync with flip model
	IDXGIFactory5* Factory5 = nullptr;
	{
		IDXGIFactory1* Factory1 = nullptr;
		if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&Factory1)))
		{
			if (SUCCEEDED(Factory1->QueryInterface(__uuidof(IDXGIFactory5), (void**)&Factory5)))
			{
				Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
					&bTearingSupported, sizeof(bTearingSupported));
			}
			Factory1->Release();
		}
	}

	if (bTearingSupported)
	{
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

	SwapChainFlags = swapChainDesc.Flags;

	UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
	CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		CreateDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
		&swapChainDesc, &SwapChain, &Device, nullptr, &DeviceContext);

	if (Factory5) Factory5->Release();

	SwapChain->GetDesc(&swapChainDesc);

	ViewportInfo = { 0, 0, float(swapChainDesc.BufferDesc.Width), float(swapChainDesc.BufferDesc.Height), 0, 1 };
}

void FD3DDevice::ReleaseDeviceAndSwapChain()
{
	//	Flush first
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}

	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE(Device);
	SAFE_RELEASE(DeviceContext);
}

void FD3DDevice::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	CD3D11_RENDER_TARGET_VIEW_DESC frameBufferRTVDesc = {};
	frameBufferRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	frameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &frameBufferRTVDesc, &FrameBufferRTV);
}

void FD3DDevice::ReleaseFrameBuffer()
{
	SAFE_RELEASE(FrameBufferRTV);
	SAFE_RELEASE(FrameBuffer);
}

void FD3DDevice::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&rasterizerDesc, &RasterizerStateBackCull);

	D3D11_RASTERIZER_DESC frontCullDesc = {};
	frontCullDesc.FillMode = D3D11_FILL_SOLID;
	frontCullDesc.CullMode = D3D11_CULL_FRONT;

	Device->CreateRasterizerState(&frontCullDesc, &RasterizerStateFrontCull);

	D3D11_RASTERIZER_DESC wireFrameDesc = {};
	wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
	wireFrameDesc.CullMode = D3D11_CULL_NONE;

	Device->CreateRasterizerState(&wireFrameDesc, &RasterizerStateWireFrame);

	CurrentRasterizerState = ERasterizerState::SolidBackCull;
}

void FD3DDevice::ReleaseRasterizerState()
{
	SAFE_RELEASE(RasterizerStateBackCull);
	SAFE_RELEASE(RasterizerStateFrontCull);
	SAFE_RELEASE(RasterizerStateWireFrame);
}

void FD3DDevice::CreateDepthStencilBuffer()
{
	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
	depthStencilDesc.Width = static_cast<uint32>(ViewportInfo.Width);
	depthStencilDesc.Height = static_cast<uint32>(ViewportInfo.Height);
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//	Depth 24bit + Stenmcil 8bit
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Device->CreateTexture2D(&depthStencilDesc, nullptr, &DepthStencilBuffer);
	Device->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);

	//	Default
	D3D11_DEPTH_STENCIL_DESC depthStencilStateDefaultDesc = {};
	depthStencilStateDefaultDesc.DepthEnable = TRUE;
	depthStencilStateDefaultDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilStateDefaultDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilStateDefaultDesc.StencilEnable = FALSE;

	Device->CreateDepthStencilState(&depthStencilStateDefaultDesc, &DepthStencilStateDefault);

	// Depth Greater
	D3D11_DEPTH_STENCIL_DESC depthGreaterDesc = {};
	depthGreaterDesc.DepthEnable = TRUE;
	depthGreaterDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthGreaterDesc.DepthFunc = D3D11_COMPARISON_GREATER;
	depthGreaterDesc.StencilEnable = FALSE;

	Device->CreateDepthStencilState(&depthGreaterDesc, &DepthStencilStateDepthGreater);

	//	Depth Read Only
	D3D11_DEPTH_STENCIL_DESC depthStencilStateDepthReadOnlyDesc = {};
	depthStencilStateDepthReadOnlyDesc.DepthEnable = TRUE;
	depthStencilStateDepthReadOnlyDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilStateDepthReadOnlyDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilStateDepthReadOnlyDesc.StencilEnable = FALSE;

	Device->CreateDepthStencilState(&depthStencilStateDepthReadOnlyDesc, &DepthStencilStateDepthReadOnly);

	// Stencil Write
	D3D11_DEPTH_STENCIL_DESC depthStencilStateStencilWriteDesc = {};
	depthStencilStateStencilWriteDesc.DepthEnable = TRUE;
	depthStencilStateStencilWriteDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilStateStencilWriteDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilStateStencilWriteDesc.StencilEnable = TRUE;
	depthStencilStateStencilWriteDesc.StencilReadMask = 0xFF;
	depthStencilStateStencilWriteDesc.StencilWriteMask = 0xFF;

	depthStencilStateStencilWriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilStateStencilWriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	depthStencilStateStencilWriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilStateStencilWriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	depthStencilStateStencilWriteDesc.BackFace = depthStencilStateStencilWriteDesc.FrontFace;

	Device->CreateDepthStencilState(&depthStencilStateStencilWriteDesc, &DepthStencilStateStencilWrite);

	{//Gizmo DepthStencil State
		D3D11_DEPTH_STENCIL_DESC gizmoInsideDesc = {};
		gizmoInsideDesc.DepthEnable = TRUE;
		gizmoInsideDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		gizmoInsideDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

		gizmoInsideDesc.StencilEnable = TRUE;
		gizmoInsideDesc.StencilReadMask = 0xFF;
		gizmoInsideDesc.StencilWriteMask = 0x00;

		gizmoInsideDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		gizmoInsideDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		gizmoInsideDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		gizmoInsideDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		gizmoInsideDesc.BackFace = gizmoInsideDesc.FrontFace;

		Device->CreateDepthStencilState(&gizmoInsideDesc, &DepthStencilStateGizmoInside);


		D3D11_DEPTH_STENCIL_DESC gizmoOutsideDesc = {};
		gizmoOutsideDesc.DepthEnable = TRUE;
		gizmoOutsideDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		gizmoOutsideDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

		gizmoOutsideDesc.StencilEnable = TRUE;
		gizmoOutsideDesc.StencilReadMask = 0xFF;
		gizmoOutsideDesc.StencilWriteMask = 0x00;

		gizmoOutsideDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		gizmoOutsideDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		gizmoOutsideDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		gizmoOutsideDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		gizmoOutsideDesc.BackFace = gizmoOutsideDesc.FrontFace;

		Device->CreateDepthStencilState(&gizmoOutsideDesc, &DepthStencilStateGizmoOutside);
	}



	//Stencil Mask Equal
	D3D11_DEPTH_STENCIL_DESC maskDesc = {};
	maskDesc.DepthEnable = TRUE;
	maskDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	maskDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	maskDesc.StencilEnable = TRUE;
	maskDesc.StencilReadMask = 0xFF;
	maskDesc.StencilWriteMask = 0x00;

	maskDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	maskDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	maskDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	maskDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	maskDesc.BackFace = maskDesc.FrontFace;

	Device->CreateDepthStencilState(&maskDesc, &DepthStencilStateStencilMaskEqual);

	// Stencil Test (Not Equal)
	D3D11_DEPTH_STENCIL_DESC depthStencilStateStencilOutlineDesc = {};
	depthStencilStateStencilOutlineDesc.DepthEnable = TRUE;
	// 또는 TRUE + ZERO로 테스트 가능, 지금은 먼저 단순하게
	depthStencilStateStencilOutlineDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilStateStencilOutlineDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilStateStencilOutlineDesc.StencilEnable = TRUE;
	depthStencilStateStencilOutlineDesc.StencilReadMask = 0xFF;
	depthStencilStateStencilOutlineDesc.StencilWriteMask = 0x00;

	depthStencilStateStencilOutlineDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	depthStencilStateStencilOutlineDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilStateStencilOutlineDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilStateStencilOutlineDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	depthStencilStateStencilOutlineDesc.BackFace = depthStencilStateStencilOutlineDesc.FrontFace;

	Device->CreateDepthStencilState(&depthStencilStateStencilOutlineDesc, &DepthStencilStateStencilOutline);
}

void FD3DDevice::CreateBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	Device->CreateBlendState(&blendDesc, &BlendStateAlpha);

	//No Color
	D3D11_BLEND_DESC noColorWriteDesc = {};
	noColorWriteDesc.AlphaToCoverageEnable = FALSE;
	noColorWriteDesc.IndependentBlendEnable = FALSE;
	noColorWriteDesc.RenderTarget[0].BlendEnable = FALSE;
	noColorWriteDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	noColorWriteDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	noColorWriteDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	noColorWriteDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	noColorWriteDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	noColorWriteDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	noColorWriteDesc.RenderTarget[0].RenderTargetWriteMask = 0;

	Device->CreateBlendState(&noColorWriteDesc, &BlendStateNoColorWrite);
}

void FD3DDevice::ReleaseDepthStencilBuffer()
{
	SAFE_RELEASE(DepthStencilStateDefault);
	SAFE_RELEASE(DepthStencilStateDepthGreater);
	SAFE_RELEASE(DepthStencilStateDepthReadOnly);
	SAFE_RELEASE(DepthStencilStateStencilWrite);
	SAFE_RELEASE(DepthStencilStateStencilOutline);
	SAFE_RELEASE(DepthStencilView);
	SAFE_RELEASE(DepthStencilBuffer);
	SAFE_RELEASE(DepthStencilStateStencilMaskEqual);
}

void FD3DDevice::ReleaseBlendState()
{
	SAFE_RELEASE(BlendStateAlpha);
	SAFE_RELEASE(BlendStateNoColorWrite);
}
