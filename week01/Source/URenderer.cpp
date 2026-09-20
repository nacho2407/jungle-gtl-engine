#include "URenderer.hpp"

#include "Rectangle.h"
#include "Sphere.h"

static FVertexSimple line_vertices[] =
{
	{-1.0f, LineY, 0.0f, 1,1,1,1},
	{ 1.0f, LineY, 0.0f, 1,1,1,1}
};

void URenderer::Create(HWND hWindow)
{
	CreateDeviceAndSwapChain(hWindow);

	CreateFrameBuffer();

	CreateRasterizerState();

	CreateSphereMesh();

	CreateRectangleMesh();

	CreateLine();

	CreateFont();
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
	// Direct3D 기능 레벨(GPU가 하드웨어적으로 보장하는 최소 기능 집합) 정의
	D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
	swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
	swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
	swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapchaindesc.SampleDesc.Count = 1;
	swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchaindesc.BufferCount = 2; // 더블 버퍼링
	swapchaindesc.OutputWindow = hWindow;
	swapchaindesc.Windowed = TRUE; // 창 모드
	swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	D3D11CreateDeviceAndSwapChain(nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
		featurelevels,
		ARRAYSIZE(featurelevels),
		D3D11_SDK_VERSION,
		&swapchaindesc,
		&SwapChain,
		&Device,
		nullptr,
		&DeviceContext
	);

	// 생성된 실제 스왑 체인의 정보 가져오기
	SwapChain->GetDesc(&swapchaindesc);

	ViewportInfo = {
		0.0f,
		0.0f,
		(float)swapchaindesc.BufferDesc.Width,
		(float)swapchaindesc.BufferDesc.Height,
		0.0f,
		1.0f
	};
}

void URenderer::CreateSceneRenderTarget(UINT width, UINT height)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
}

void URenderer::CreateFrameBuffer()
{
	// 스왑 체인으로부터 백 버퍼 텍스처 가져오기
	// 특정 물리 메모리를 계속 가리키는 것이 아니며, 스왑 체인 내 버퍼는 DXGI가 내부적으로 버퍼 회전을 하면서도 앱이 계속 같은 RTV로 현재의 백 버퍼를 그릴 수 있게 추상화되어 있음
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void URenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerdesc = {};
	rasterizerdesc.FillMode = D3D11_FILL_SOLID;
	rasterizerdesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void URenderer::CreateSphereMesh()
{
	sphere.NumVertices = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	sphere.VertexBuffer = CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
}

void URenderer::CreateRectangleMesh()
{
	rectangle.NumVertices = sizeof(rectangle_vertices) / sizeof(FVertexSimple);
	rectangle.VertexBuffer = CreateVertexBuffer(rectangle_vertices, sizeof(rectangle_vertices));
}

void URenderer::CreateLine()
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(line_vertices);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = line_vertices;

	Device->CreateBuffer(&desc, &data, &LineVertexBuffer);
}

void URenderer::CreateFont()
{
	SpriteBatch = std::make_unique<DirectX::SpriteBatch>(DeviceContext);
	Font = std::make_unique<DirectX::SpriteFont>(
		Device,
		L"./Content/Resource/comic_sans_ms.spritefont"
	);
}

void URenderer::Release()
{
	ReleaseRasterizerState();

	// 렌더 타겟 초기화
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	ReleaseDeviceAndSwapChain();

	ReleaseSphereMesh();
	ReleaseRectangleMesh();
	ReleaseLine();
}

void URenderer::ReleaseLine()
{
	if (LineVertexBuffer) { LineVertexBuffer->Release(); LineVertexBuffer = nullptr; }
}

void URenderer::ReleaseFrameBuffer()
{
	if (FrameBuffer)
	{
		FrameBuffer->Release();
		FrameBuffer = nullptr;
	}

	if (FrameBufferRTV)
	{
		FrameBufferRTV->Release();
		FrameBufferRTV = nullptr;
	}
}

void URenderer::ReleaseDeviceAndSwapChain()
{
	if (DeviceContext)
	{
		DeviceContext->Flush(); // 남은 GPU 명령 실행
	}

	if (SwapChain)
	{
		SwapChain->Release();
		SwapChain = nullptr;
	}

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}

	if (DeviceContext)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}
}

void URenderer::ReleaseRasterizerState()
{
	if (RasterizerState)
	{
		RasterizerState->Release();
		RasterizerState = nullptr;
	}
}

void URenderer::ReleaseSphereMesh()
{
	ReleaseVertexBuffer(sphere.VertexBuffer);
}

void URenderer::ReleaseRectangleMesh()
{
	ReleaseVertexBuffer(rectangle.VertexBuffer);
}

void URenderer::CreateShader()
{
	ID3DBlob* vertexshaderCSO;

	D3DCompileFromFile(
		L"Shaders/ShaderW0.hlsl",
		nullptr,
		nullptr,
		"mainVS",
		"vs_5_0", // Shader Model 5.0
		0,
		0,
		&vertexshaderCSO,
		nullptr
	);

	Device->CreateVertexShader(
		vertexshaderCSO->GetBufferPointer(),
		vertexshaderCSO->GetBufferSize(),
		nullptr,
		&SimpleVertexShader
	);

	ID3DBlob* pixelshaderCSO;

	D3DCompileFromFile(
		L"Shaders/ShaderW0.hlsl",
		nullptr,
		nullptr,
		"mainPS",
		"ps_5_0",
		0,
		0,
		&pixelshaderCSO,
		nullptr
	);

	Device->CreatePixelShader(
		pixelshaderCSO->GetBufferPointer(),
		pixelshaderCSO->GetBufferSize(),
		nullptr,
		&SimplePixelShader
	);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			12,
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			28,
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		}
	};

	Device->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		vertexshaderCSO->GetBufferPointer(),
		vertexshaderCSO->GetBufferSize(),
		&SimpleInputLayout
	);

	Stride = sizeof(FVertexSimple);

	vertexshaderCSO->Release();
	pixelshaderCSO->Release();
}

void URenderer::ReleaseShader()
{
	if (SimpleInputLayout)
	{
		SimpleInputLayout->Release();
		SimpleInputLayout = nullptr;
	}

	if (SimplePixelShader)
	{
		SimplePixelShader->Release();
		SimplePixelShader = nullptr;
	}

	if (SimpleVertexShader)
	{
		SimpleVertexShader->Release();
		SimpleVertexShader = nullptr;
	}
}

ID3D11Buffer* URenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = byteWidth;
	vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // 현재는 정점이 변경되지 않을 것이므로
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

	ID3D11Buffer* vertexBuffer;

	Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

	return vertexBuffer;
}

void URenderer::ReleaseVertexBuffer(ID3D11Buffer*& vertexBuffer)
{
	if (vertexBuffer)
	{
		vertexBuffer->Release();
		vertexBuffer = nullptr;
	}
}

void URenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};
	constantbufferdesc.ByteWidth = (sizeof(FConstants) + 0xf) & 0xfffffff0; // 16바이트 정렬
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // 프레임마다 CPU가 갱신해주어야 하므로
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Usage는 사용 패턴만 정의. CPU가 접근할 수 있도록 플래그도 설정해주어야 함
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
}

void URenderer::ReleaseConstantBuffer()
{
	if (ConstantBuffer)
	{
		ConstantBuffer->Release();
		ConstantBuffer = nullptr;
	}
}

void URenderer::UpdateConstant(FVector Offset, float Angle, FVector Scale, float UseTexture, FVector Tint)
{
	FVector CosSin = { cosf(Angle), sinf(Angle), 0.f };

	if (ConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU에 있는 상수 버퍼를 CPU가 접근할 수 있도록 매핑

		DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
		FConstants* constants = (FConstants*)constantbufferMSR.pData;

		constants->Offset = Offset;
		constants->SinCos = CosSin;
		constants->Scale = Scale;
		constants->UseTexture = UseTexture;
		constants->TintColor = Tint;

		DeviceContext->Unmap(ConstantBuffer, 0); // CPU 작업이 끝난 뒤 언매핑
	}
}

void URenderer::UpdateTexture(UTexture* InTexture)
{
	if (InTexture)
	{
		ID3D11ShaderResourceView* SRV = InTexture->GetTextureSRV();
		DeviceContext->PSSetShaderResources(0, 1, &SRV);
	}
	else
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
	}
}

void URenderer::Prepare()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState);

	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
	// alpha blending 설정
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;

	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	ID3D11BlendState* AlphaBlendState;
	Device->CreateBlendState(&blendDesc, &AlphaBlendState);

	float blendFactor[4] = { 0.f,0.f,0.f,0.f };

	DeviceContext->OMSetBlendState(
		AlphaBlendState,
		blendFactor,
		0xffffffff);

	AlphaBlendState->Release();
}

void URenderer::PrepareShader()
{
	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
	DeviceContext->IASetInputLayout(SimpleInputLayout);

	if (ConstantBuffer)
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
	}
}

void URenderer::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
{
	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

	DeviceContext->Draw(numVertices, 0);
}

void URenderer::RenderSphere()
{
	DeviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);
	RenderPrimitive(sphere.VertexBuffer, sphere.NumVertices);
}

void URenderer::RenderRectangle()
{
	DeviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);
	RenderPrimitive(rectangle.VertexBuffer, rectangle.NumVertices);
}

void URenderer::RenderLine()
{
	DeviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_LINELIST
	);
	RenderPrimitive(LineVertexBuffer, 2);
}

void URenderer::UpdateLineColor(bool collision)
{
	FVertexSimple vertices[2];

	float y = LineY;

	if (collision)
	{
		vertices[0] = { -1.0f, y, 0.0f, 1,0,0,1 };
		vertices[1] = { 1.0f, y, 0.0f, 1,0,0,1 };
	}
	else
	{
		vertices[0] = { -1.0f, y, 0.0f, 1,1,1,1 };
		vertices[1] = { 1.0f, y, 0.0f, 1,1,1,1 };
	}

	DeviceContext->UpdateSubresource(LineVertexBuffer, 0, nullptr, vertices, 0, 0);
}

void URenderer::SwapBuffer()
{
	SwapChain->Present(1, 0); // VSync 활성화
}
