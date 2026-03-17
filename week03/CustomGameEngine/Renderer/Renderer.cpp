#include "Renderer.h"
#include "Component/CameraComponent.h"

void URenderer::Create(HWND hWindow) {
	CreateDeviceAndSwapChain(hWindow);

	CreateFrameBuffer();

	CreateRasterizerState();
	CreateDepthStencilState();
	CreateBlendState();

	CreateConstantBuffer();
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow) {
	D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
	swapchaindesc.BufferDesc.Width = 0;
	swapchaindesc.BufferDesc.Height = 0;
	swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapchaindesc.SampleDesc.Count = 1;
	swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchaindesc.BufferCount = 2;
	swapchaindesc.OutputWindow = hWindow;
	swapchaindesc.Windowed = TRUE;
	swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
		featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
		&swapchaindesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), nullptr, DeviceContext.GetAddressOf());

	SwapChain->GetDesc(&swapchaindesc);

	ViewportInfo = { 0.0f,
					0.0f,
					(float)swapchaindesc.BufferDesc.Width,
					(float)swapchaindesc.BufferDesc.Height,
					0.0f,
					1.0f };
}

void URenderer::CreateBlendState()
{
	D3D11_BLEND_DESC desc = {};
	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;

	D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];

	rt.BlendEnable = TRUE;

	rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	rt.BlendOp = D3D11_BLEND_OP_ADD;

	rt.SrcBlendAlpha = D3D11_BLEND_ONE;
	rt.DestBlendAlpha = D3D11_BLEND_ZERO;
	rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;

	rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	Device->CreateBlendState(&desc, BlendState.GetAddressOf());
}

void URenderer::ReleaseDeviceAndSwapChain() {
	if (DeviceContext) {
		DeviceContext->Flush();
	}
	SwapChain.Reset();
	DeviceContext.Reset();
	Device.Reset();
}

void URenderer::CreateFrameBuffer() {
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)FrameBuffer.GetAddressOf()); //백버퍼 가져오기

	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer.Get(), &framebufferRTVdesc,
		FrameBufferRTV.GetAddressOf());

	D3D11_TEXTURE2D_DESC depthBufferDesc = {};
	FrameBuffer->GetDesc(&depthBufferDesc);
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	Device->CreateTexture2D(&depthBufferDesc, nullptr, DepthStencilBuffer.GetAddressOf());
	if (DepthStencilBuffer)
	{
		Device->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr, DepthStencilView.GetAddressOf());
	}
}

void URenderer::ReleaseFrameBuffer() {
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
	FrameBuffer.Reset();
	FrameBufferRTV.Reset();
}

void URenderer::CreateRasterizerState() {
	D3D11_RASTERIZER_DESC rasterizerdesc = {};
	rasterizerdesc.FillMode = D3D11_FILL_SOLID;

	rasterizerdesc.CullMode = D3D11_CULL_BACK;
	Device->CreateRasterizerState(&rasterizerdesc, RasterizerState.GetAddressOf());

	rasterizerdesc.CullMode = D3D11_CULL_NONE;
	Device->CreateRasterizerState(&rasterizerdesc, RasterizerStateNoCull.GetAddressOf());
}

void URenderer::ReleaseRasterizerState() {
	RasterizerState.Reset();
	RasterizerStateNoCull.Reset();
}

void URenderer::CreateDepthStencilState() {
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = FALSE;

	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	Device->CreateDepthStencilState(&depthStencilDesc, DepthStencilState.GetAddressOf());

	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Device->CreateDepthStencilState(&depthStencilDesc, DepthStencilStateNoWrite.GetAddressOf());

	depthStencilDesc.DepthEnable = FALSE;
	Device->CreateDepthStencilState(&depthStencilDesc, DepthStencilStateNoDepth.GetAddressOf());
}

void URenderer::ReleaseDepthStencilState() {
	DepthStencilState.Reset();
	DepthStencilStateNoWrite.Reset();
	DepthStencilStateNoDepth.Reset();
}

void URenderer::ReleaseBlendState()
{
	BlendState.Reset();
}

void URenderer::Release() {
	ReleaseBlendState();
	ReleaseDepthStencilState();
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	ReleaseRasterizerState();
	ReleaseFrameBuffer();
	ReleaseDeviceAndSwapChain();
	ReleaseConstantBuffer();

}

void URenderer::SwapBuffer() { SwapChain->Present(0, 0); }

void URenderer::OnResize(UINT Width, UINT Height)
{
	if (!Device || !SwapChain) return;


	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	ReleaseFrameBuffer();

	HRESULT hr = SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) {
		return;
	}

	CreateFrameBuffer();

	ViewportInfo.Width = float(Width);
	ViewportInfo.Height = float(Height);
	ViewportInfo.MinDepth = 0.0f;
	ViewportInfo.MaxDepth = 1.0f;
	ViewportInfo.TopLeftX = 0.0f;
	ViewportInfo.TopLeftY = 0.0f;
	DeviceContext->RSSetViewports(1, &ViewportInfo);
}

void URenderer::Prepare() {
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState.Get());
	DeviceContext->OMSetRenderTargets(1, FrameBufferRTV.GetAddressOf(), DepthStencilView.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	float blendFactor[4] = { 0,0,0,0 };
	DeviceContext->OMSetBlendState(BlendState.Get(), blendFactor, 0xffffffff);
}

void URenderer::CreateConstantBuffer() {
	FrameConstantBuffer.Create(Device.Get(), sizeof(FFrameConstant), 0);
	FrameConstantBuffer.Bind(DeviceContext.Get());
	ObjectConstantBuffer.Create(Device.Get(), sizeof(FObjectConstant), 1);
	ObjectConstantBuffer.Bind(DeviceContext.Get());

	FGridConstantBuffer.Create(Device.Get(), sizeof(FGridConstant), 2);
	FGridConstantBuffer.Bind(DeviceContext.Get());
}

void URenderer::ReleaseConstantBuffer() {
	FrameConstantBuffer.Release();
	ObjectConstantBuffer.Release();
	FGridConstantBuffer.Release();
}

void URenderer::Render(UCameraComponent* Camera)
{
	FFrameConstant FrameConst;
	FrameConst.Projection = Camera->GetProjectionMatrix();
	FrameConst.View = Camera->GetViewMatrix();
	FrameConstantBuffer.Update(DeviceContext.Get(), &FrameConst, sizeof(FFrameConstant));
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	//FrameConstantBuffer.Bind(DeviceContext);

	TArray<RenderObject*> NormalRenderObjects;
	TArray<RenderObject*> NoDepthRenderObjects;

	FObjectConstant ObjectConst;
	auto renderObjects = &FLevel::GetInstance().RenderObjects;
	for (RenderObject* RenderObj : *renderObjects)
	{
		if (RenderObj->bDepthEnabled)
		{
			NormalRenderObjects.push_back(RenderObj);
		}
		else
		{
			NoDepthRenderObjects.push_back(RenderObj);
		}
	}

	for (RenderObject* RenderObj : NormalRenderObjects)
	{
		//Camera
		ObjectConst.World = RenderObj->World;
		ObjectConst.bIsSelected = RenderObj->bIsSelected ? 1 : 0;
		ObjectConstantBuffer.Update(DeviceContext.Get(), &ObjectConst, sizeof(FObjectConstant));

		//GPU에 ConstantBuffer가 오브젝트마다 생기는 것을 방지하기 위해 ObjectConstantBuffer를 하나만 만들어서 매 오브젝트마다 업데이트해서 사용.
		RenderObj->Render(DeviceContext.Get());
	}

	if (!NoDepthRenderObjects.empty())
	{
		DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		for (RenderObject* RenderObj : NoDepthRenderObjects)
		{
			ObjectConst.World = RenderObj->World;
			ObjectConst.bIsSelected = RenderObj->bIsSelected ? 1 : 0;
			ObjectConstantBuffer.Update(DeviceContext.Get(), &ObjectConst, sizeof(FObjectConstant));
			RenderObj->Render(DeviceContext.Get());
		}
	}
	// ClearRenderObjects();
	// RenderObjects.clear();
}

void URenderer::RenderGrid(UCameraComponent* Camera, ID3D11VertexShader* VertexShader, ID3D11PixelShader* PixelShader)
{
	FFrameConstant FrameConst;
	FGridConstant GridConst;
	FrameConst.Projection = Camera->GetProjectionMatrix();
	FrameConst.View = Camera->GetViewMatrix();
	FrameConstantBuffer.Update(DeviceContext.Get(), &FrameConst, sizeof(FFrameConstant));

	GridConst.CameraPos[0] = Camera->Position.x;
	GridConst.CameraPos[1] = Camera->Position.y;
	GridConst.CameraPos[2] = Camera->Position.z;
	GridConst.GridSize = 10.0f;
	GridConst.LineThickness = 0.8f;
	FGridConstantBuffer.Update(DeviceContext.Get(), &GridConst, sizeof(FGridConstant));

	DeviceContext->OMSetDepthStencilState(DepthStencilStateNoWrite.Get(), 0);
	DeviceContext->RSSetState(RasterizerStateNoCull.Get());

	DeviceContext->VSSetShader(VertexShader, nullptr, 0);
	DeviceContext->PSSetShader(PixelShader, nullptr, 0);
	DeviceContext->IASetInputLayout(nullptr);

	FGridConstantBuffer.Bind(DeviceContext.Get());
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DeviceContext->Draw(4, 0);



	DeviceContext->RSSetState(RasterizerState.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
