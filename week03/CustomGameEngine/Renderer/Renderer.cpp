#include "Renderer.h"
#include "EngineCore/ResourceManager.h"
void URenderer::Create(HWND hWindow)
{
	CreateDeviceAndSwapChain(hWindow);

	CreateFrameBuffer();

	CreateRasterizerState();
	CreateDepthStencilState();
	CreateBlendState();

	CreateConstantBuffer();
	CreateDynamicVertexBuffer();

	ShowFlagMap[EEngineShowFlags::SF_Primitives] = true;
	ShowFlagMap[EEngineShowFlags::SF_BillboardText] = true;
	ShowFlagMap[EEngineShowFlags::SF_EditorPrimitives] = true;
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
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

void URenderer::ReleaseDeviceAndSwapChain()
{
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}
	SwapChain.Reset();
	DeviceContext.Reset();
	Device.Reset();
}

void URenderer::CreateFrameBuffer()
{
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

void URenderer::ReleaseFrameBuffer()
{
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
	FrameBuffer.Reset();
	FrameBufferRTV.Reset();
}

void URenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerdesc = {};
	rasterizerdesc.FillMode = D3D11_FILL_SOLID;

	rasterizerdesc.CullMode = D3D11_CULL_BACK;
	Device->CreateRasterizerState(&rasterizerdesc, RasterizerState.GetAddressOf());

	rasterizerdesc.CullMode = D3D11_CULL_NONE;
	Device->CreateRasterizerState(&rasterizerdesc, RasterizerStateNoCull.GetAddressOf());

	rasterizerdesc.FillMode = D3D11_FILL_WIREFRAME;
	Device->CreateRasterizerState(&rasterizerdesc, RasterizerStateWireframe.GetAddressOf());
}

void URenderer::ReleaseRasterizerState()
{
	RasterizerState.Reset();
	RasterizerStateNoCull.Reset();
	RasterizerStateWireframe.Reset();
}

void URenderer::CreateDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER;
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

void URenderer::ReleaseDepthStencilState()
{
	DepthStencilState.Reset();
	DepthStencilStateNoWrite.Reset();
	DepthStencilStateNoDepth.Reset();
}

void URenderer::ReleaseBlendState()
{
	BlendState.Reset();
}

void URenderer::Release()
{
	ReleaseDynamicVertexBuffer();
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
	if (FAILED(hr))
	{
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

void URenderer::CreateDynamicVertexBuffer()
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = sizeof(FVertexSimple) * 20000;
	vertexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = Device->CreateBuffer(&vertexbufferdesc, nullptr, DynamicVertexBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		//ERROR HANDLE
	}
}

void URenderer::UpdateDynamicVertexBuffer(FVertexSimple* Vertices, UINT VertexCount)
{
	const int32 MaxVertexCount = 20000;
	if (VertexCount > MaxVertexCount)
	{
		//ERROR HANDLE
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	HRESULT hr = DeviceContext->Map(
		DynamicVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	memcpy(Mapped.pData, Vertices, sizeof(FVertexSimple) * VertexCount);
	DeviceContext->Unmap(DynamicVertexBuffer.Get(), 0);
}

void URenderer::ReleaseDynamicVertexBuffer()
{
	DynamicVertexBuffer.Reset();
}

void URenderer::Prepare()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
	//향후 레스터라이저 관련 모드 추가를 위해 siwtch문으로 만들어 두겠습니다.
	switch (RsSetMode)
	{
	case 0:
		DeviceContext->RSSetState(RasterizerState.Get());
		break;
	case 1:
		DeviceContext->RSSetState(RasterizerStateNoCull.Get());
		break;
	case 2:
		DeviceContext->RSSetState(RasterizerStateWireframe.Get());
		break;
	}
	DeviceContext->OMSetRenderTargets(1, FrameBufferRTV.GetAddressOf(), DepthStencilView.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	float blendFactor[4] = { 0,0,0,0 };
	DeviceContext->OMSetBlendState(BlendState.Get(), blendFactor, 0xffffffff);
}

void URenderer::CreateConstantBuffer()
{
	FrameConstantBuffer.Create(Device.Get(), sizeof(FFrameConstant), 0);
	FrameConstantBuffer.Bind(DeviceContext.Get());
}

void URenderer::ReleaseConstantBuffer()
{
	FrameConstantBuffer.Release();
}

void URenderer::SetRenderCamera(class UCameraComponent* camera)
{
	renderCamera = camera;
}

void URenderer::RenderNoDepthObjects(TArray<RenderObject*>& NoDepthRenderObjects)
{
	FObjectConstant ObjectConst;
	if (!NoDepthRenderObjects.empty())
	{
		DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);

		for (RenderObject* RenderObj : NoDepthRenderObjects)
		{
			ObjectConst.World = RenderObj->World;
			ObjectConst.Color = RenderObj->Color;
			ObjectConst.bIsSelected = RenderObj->bIsSelected ? 1 : 0;
			RenderObj->Material->ConstantBuffer.Update(DeviceContext.Get(), &ObjectConst, sizeof(FObjectConstant));
			RenderObj->Material->ConstantBuffer.Bind(DeviceContext.Get());
			RenderObj->Render(DeviceContext.Get());
		}
	}
}

void URenderer::Render(const TArray<FLevel*>& Levels)
{
	if (renderCamera == nullptr) return;

	FFrameConstant FrameConst;
	FrameConst.Projection = renderCamera->GetProjectionMatrix();
	FrameConst.View = renderCamera->GetViewMatrix();
	FrameConstantBuffer.Update(DeviceContext.Get(), &FrameConst, sizeof(FFrameConstant));
	FrameConstantBuffer.Bind(DeviceContext.Get());


	//Phase 1 : Distribute RenderObjects into 3 groups : Batch, Normal, NoDepth
	TArray<RenderObject*> NormalRenderObjects;
	TArray<RenderObject*> NoDepthRenderObjects;
	TMap<BatchKey, TArray<RenderObject*>> BatchGroups;
	DistributeRenderObjects(Levels, BatchGroups, NormalRenderObjects, NoDepthRenderObjects);
	RenderNormalObjects(NormalRenderObjects);
	BatchRender(BatchGroups);
	RenderNoDepthObjects(NoDepthRenderObjects);
}



void URenderer::BatchRenderDebugLine(const TArray<FVertexSimple>& Lines)
{
	ResourceManager::GetInstance()->GetMaterial(L"Shader/ShaderLine.hlsl").Bind(DeviceContext.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilStateNoDepth.Get(), 0);
	UpdateDynamicVertexBuffer(const_cast<FVertexSimple*>(Lines.data()), Lines.size());
	UINT stride = sizeof(FVertexSimple);
	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, DynamicVertexBuffer.GetAddressOf(), &stride, &offset);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	DeviceContext->Draw(Lines.size(), 0);
}

void URenderer::SetRasterizerMode(int32 mode)
{
	RsSetMode = mode;
}

void URenderer::DistributeRenderObjects(
	const TArray<FLevel*>& Levels,
	TMap<BatchKey, TArray<RenderObject*>>& BatchGroups,
	TArray<RenderObject*>& NormalRenderObjects,
	TArray<RenderObject*>& NoDepthRenderObjects)
{
	for (FLevel* Level : Levels)
	{
		if (Level == nullptr)
		{
			continue;
		}
		for (RenderObject* RenderObj : Level->RenderObjects)
		{
			if (!ShowFlagMap[RenderObj->ShowFlag])
				continue;
			if (RenderObj->bBatch)
			{
				BatchKey key{ RenderObj->GetBatchKey() };
				BatchGroups[key].push_back(RenderObj);
			}
			else
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
		}
	}
}

void URenderer::RenderNormalObjects(TArray<RenderObject*>& NormalRenderObjects)
{
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	FObjectConstant ObjectConst;

	for (RenderObject* RenderObj : NormalRenderObjects)
	{
		//Camera
		ObjectConst.World = RenderObj->World;
		ObjectConst.Color = RenderObj->Color;
		ObjectConst.bIsSelected = RenderObj->bIsSelected ? 1 : 0;
		RenderObj->Material->ConstantBuffer.Update(DeviceContext.Get(), &ObjectConst, sizeof(FObjectConstant));
		RenderObj->Material->ConstantBuffer.Bind(DeviceContext.Get());
		RenderObj->Render(DeviceContext.Get());
	}
}


void URenderer::BatchRender(TMap<BatchKey, TArray<RenderObject*>>& BatchGroups)
{
	//DeviceContext->OMSetDepthStencilState(DepthStencilStateNoDepth.Get(), 0);
	FObjectConstant ObjectConst;
	for (auto& [key, objects] : BatchGroups)
	{
		key.Material->Bind(DeviceContext.Get());
		DeviceContext->IASetPrimitiveTopology(key.PrimitiveTopology);
		TArray<FVertexSimple> Vertices;
		FConstantBuffer* ConstBuffer = &key.Material->ConstantBuffer;
		for (RenderObject* RenderObj : objects)
		{
			const bool bHasVertexColors = !RenderObj->Geometry->Colors.empty();
			for (size_t i = 0; i < RenderObj->Geometry->Vertices.size(); ++i)
			{
				const FVector& localVec = RenderObj->Geometry->Vertices[i];
				FVector worldVec = RenderObj->World.TransformPoint(localVec);
				const FColor& col = bHasVertexColors ? RenderObj->Geometry->Colors[i] : RenderObj->Color;
				Vertices.push_back({ worldVec.x, worldVec.y, worldVec.z, col.r, col.g, col.b, col.a });
			}
		}
		ObjectConst.World = FMatrix::IdentityMatrix();
		ObjectConst.Color = FColor(1, 1, 1, 1);
		ObjectConst.bIsSelected = 0;
		ConstBuffer->Bind(DeviceContext.Get());
		ConstBuffer->Update(DeviceContext.Get(), &ObjectConst, sizeof(FObjectConstant));
		UpdateDynamicVertexBuffer(Vertices.data(), Vertices.size());
		UINT stride = sizeof(FVertexSimple);
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, DynamicVertexBuffer.GetAddressOf(), &stride, &offset);
		DeviceContext->Draw(Vertices.size(), 0);
	}
}

