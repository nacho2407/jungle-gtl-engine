#include <cstdlib>
#include <ctime>
#include <string>

#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fmod.hpp>
#pragma comment (lib, "fmod_vc.lib")

// From DirectXTK
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "SimpleMath.h"


#include <unordered_map>
#include <string>

class UTexture
{
public:
	UTexture()
	{

	}

	UTexture(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName)
	{
		bool bIsOK = Initialize(Device, DeviceContext, FileName);
		assert(bIsOK);
	}

	bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName)
	{
		D3D11_TEXTURE2D_DESC TextureDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		UINT RowPitch;
		HRESULT Result;
		unsigned char* Data;

		Data = stbi_load(FileName, &Width, &Height, &Channels, 0);

		assert(Data);

		TextureDesc.Height = Height;
		TextureDesc.Width = Width;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.SampleDesc.Quality = 0;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		TextureDesc.CPUAccessFlags = 0;
		TextureDesc.MiscFlags = 0;

		RowPitch = (Width * 4) * sizeof(unsigned char);
		D3D11_SUBRESOURCE_DATA InitData = { Data, RowPitch, 0 };

		Result = Device->CreateTexture2D(&TextureDesc, &InitData, &Texture);
		if (FAILED(Result))
		{
			return false;
		}

		SRVDesc.Format = TextureDesc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = 1;

		Result = Device->CreateShaderResourceView(Texture, &SRVDesc, &TextureSRV);
		if (FAILED(Result))
		{
			return false;
		}

		delete[] Data;
		return true;
	}

	void Release()
	{
		if (TextureSRV)
		{
			TextureSRV->Release();
			TextureSRV = nullptr;
		}
		if (Texture)
		{
			Texture->Release();
			Texture = nullptr;
		}
	}

	ID3D11ShaderResourceView* GetTextureSRV() { return TextureSRV; }

private:
	ID3D11Texture2D* Texture;
	ID3D11ShaderResourceView* TextureSRV;

	int Width;
	int Height;
	int Channels;
};

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
	float u, v;
};

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

	FVector operator +(const FVector& rhs) const
	{
		return FVector(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	FVector operator -(const FVector& rhs) const
	{
		return FVector(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	FVector operator *(float scala) const
	{
		return FVector(x * scala, y * scala, z * scala);
	}

	float Dot2(const FVector& rhs) const
	{
		return x * rhs.x + y * rhs.y;
	}
};

struct SceneFrame
{
	ImVec2 Pos;
	ImVec2 Size;
};

#include "Sphere.h"
#include "Rectangle.h"

FVertexSimple line_vertices[] =
{
	{-1.0f, 0.8f, 0.0f, 1,1,1,1},
	{ 1.0f, 0.8f, 0.0f, 1,1,1,1}
};

class URenderer
{
public:
	struct FConstants
	{
		FVector Offset;
		float  Radius;
		FVector SinCos;
		float Padding;
		float UseTexture;   // 1 = texture, 0 = color
		float Padding2[3];
	};

	struct FMesh
	{
		ID3D11Buffer* VertexBuffer = nullptr;
		UINT NumVertices;
	};

	FMesh sphere;
	FMesh rectangle;

	ID3D11Buffer* LineVertexBuffer = nullptr;

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11RasterizerState* RasterizerState = nullptr;

	D3D11_VIEWPORT ViewportInfo;

	ID3D11InputLayout* SimpleInputLayout = nullptr;
	unsigned int Stride;

	ID3D11VertexShader* SimpleVertexShader = nullptr;

	ID3D11PixelShader* SimplePixelShader = nullptr;

	ID3D11Buffer* ConstantBuffer = nullptr;

	ID3D11BlendState* AlphaBlendState = nullptr;
	// 화면 초기화 색상
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

	std::unique_ptr<DirectX::SpriteBatch> SpriteBatch;
	std::unique_ptr<DirectX::SpriteFont> Font;

public:
	void Create(HWND hWindow)
	{
		CreateDeviceAndSwapChain(hWindow);

		CreateFrameBuffer();

		CreateRasterizerState();

		CreateSphereMesh();

		CreateRectangleMesh();

		CreateLine();

		CreateFont();
	}

	void CreateDeviceAndSwapChain(HWND hWindow)
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

	void CreateSceneRenderTarget(UINT width, UINT height)
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

	void CreateFrameBuffer()
	{
		// 스왑 체인으로부터 백 버퍼 텍스처 가져오기
		// 특정 물리 메모리를 계속 가리키는 것이 아니며, 스왑 체인 내 버퍼는 DXGI가 내부적으로 버퍼 회전을 하면서도 앱이 계속 같은 RTV로 현재의 백 버퍼를 그릴 수 있게 추상화되어 있음
		SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

		D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
		framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
	}

	void CreateRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerdesc = {};
		rasterizerdesc.FillMode = D3D11_FILL_SOLID;
		rasterizerdesc.CullMode = D3D11_CULL_BACK;

		Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
	}

	void CreateSphereMesh()
	{
		sphere.NumVertices = sizeof(sphere_vertices) / sizeof(FVertexSimple);
		sphere.VertexBuffer = CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	}

	void CreateRectangleMesh()
	{
		rectangle.NumVertices = sizeof(rectangle_vertices) / sizeof(FVertexSimple);
		rectangle.VertexBuffer = CreateVertexBuffer(rectangle_vertices, sizeof(rectangle_vertices));
	}

	void CreateLine()
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(line_vertices);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = line_vertices;

		Device->CreateBuffer(&desc, &data, &LineVertexBuffer);
	}

	void CreateFont()
	{
		SpriteBatch = std::make_unique<DirectX::SpriteBatch>(DeviceContext);
		Font = std::make_unique<DirectX::SpriteFont>(
			Device,
			L"./Resource/comic_sans_ms.spritefont"
		);
	}

	void Release()
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

	void ReleaseLine()
	{
		if (LineVertexBuffer) { LineVertexBuffer->Release(); LineVertexBuffer = nullptr; }
	}
	void ReleaseFrameBuffer()
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

	void ReleaseDeviceAndSwapChain()
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

	void ReleaseRasterizerState()
	{
		if (RasterizerState)
		{
			RasterizerState->Release();
			RasterizerState = nullptr;
		}
	}

	void ReleaseSphereMesh()
	{
		ReleaseVertexBuffer(sphere.VertexBuffer);
	}

	void ReleaseRectangleMesh()
	{
		ReleaseVertexBuffer(rectangle.VertexBuffer);
	}

	void CreateShader()
	{
		ID3DBlob* vertexshaderCSO;

		D3DCompileFromFile(
			L"ShaderW0.hlsl",
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
			L"ShaderW0.hlsl",
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

	void ReleaseShader()
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

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
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

	void ReleaseVertexBuffer(ID3D11Buffer*& vertexBuffer)
	{
		if (vertexBuffer)
		{
			vertexBuffer->Release();
			vertexBuffer = nullptr;
		}
	}

	void CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = (sizeof(FConstants) + 0xf) & 0xfffffff0; // 16바이트 정렬
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // 프레임마다 CPU가 갱신해주어야 하므로
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Usage는 사용 패턴만 정의. CPU가 접근할 수 있도록 플래그도 설정해주어야 함
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
	}

	void ReleaseConstantBuffer()
	{
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}

	void UpdateConstant(FVector Offset, float Angle, float Radius, float UseTexture)
	{
		FVector CosSin = { cosf(Angle), sinf(Angle), 0.f };

		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU에 있는 상수 버퍼를 CPU가 접근할 수 있도록 매핑

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;

			constants->Offset = Offset;
			constants->SinCos = CosSin;
			constants->Radius = Radius;
			constants->UseTexture = UseTexture;

			DeviceContext->Unmap(ConstantBuffer, 0); // CPU 작업이 끝난 뒤 언매핑
		}
	}

	void UpdateTexture(UTexture* InTexture)
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

	void Prepare()
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

	void PrepareShader()
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

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

		DeviceContext->Draw(numVertices, 0);
	}

	void RenderSphere()
	{
		DeviceContext->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);
		RenderPrimitive(sphere.VertexBuffer, sphere.NumVertices);
	}

	void RenderRectangle()
	{
		DeviceContext->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);
		RenderPrimitive(rectangle.VertexBuffer, rectangle.NumVertices);
	}

	void RenderLine()
	{
		DeviceContext->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_LINELIST
		);
		RenderPrimitive(LineVertexBuffer, 2);
	}

	void UpdateLineColor(bool collision)
	{
		FVertexSimple vertices[2];

		float y = 0.8f;

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

	void SwapBuffer()
	{
		SwapChain->Present(1, 0); // VSync 활성화
	}
};

class UPrimitive
{
public:
	UPrimitive(UTexture* InTexture = nullptr) : Texture(InTexture)
	{
	}
	virtual ~UPrimitive() = default; // 생성과 해제를 UPrimitive*로 할 예정이므로

	virtual void UpdatePhysics(float dt) = 0;
	virtual void Render(URenderer& renderer) = 0;

	// 삭제 예정 플래그 조작
	void MarkPendingDestroy()
	{
		bPendingDestroy = true;
	}

	bool IsPendingDestroy() const
	{
		return bPendingDestroy;
	}

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }
	UTexture* GetTexture() const { return Texture; }

protected:
	// Border 는 map 데이터이므로 따로 빼서 전역 상수 등으로 관리하는 것이 좋을 듯함
	// 이외 데이터도 관련해서 수정해야할수도
	float LeftBorder = -1.0f;
	float RightBorder = 1.0f;
	float TopBorder = 1.0f;
	float BottomBorder = -1.0f;

	bool bPendingDestroy = false; // 충돌 검사 중 삭제될 예정인 Primitive들을 표시하기 위한 플래그

private:
	UTexture* Texture;
};

class UBall : public UPrimitive
{
public:
	FVector Location; // 여기서는 Offset과 동일한 개념
	FVector Velocity;

	float Angle;
	float AngularVelocity;

	float Radius;

	// 무게 비례 공 밀어내기에 사용(ResolveCollision 참조)
	float Mass;

public:
	UBall(UTexture* InTexture = nullptr) :
		UPrimitive(InTexture),
		Location(0.0f, 0.0f, 0.0f),
		Velocity(0.0f, 0.0f, 0.0f),
		Radius(0.1f),
		Mass(0.0f),
		Angle(0.f),
		AngularVelocity(1.f)
	{
		Mass = Radius * Radius * MassFactor;

		LeftBorder = -1.0f + Radius;
		RightBorder = 1.0f - Radius;
		TopBorder = 1.0f - Radius;
		BottomBorder = -1.0f + Radius;

		Location.x = RandRange(LeftBorder, RightBorder);
		Location.y = 1.f;
		Location.z = 0.0f;

		Velocity.x = 0.f;
		Velocity.y = -1.f;
		Velocity.z = 0.0f;
	}

	~UBall()
	{

	}

	void Render(URenderer& renderer) override
	{
		renderer.UpdateConstant(Location, Angle, Radius, 1.0f);
		renderer.UpdateTexture(GetTexture());
		renderer.RenderRectangle();
	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FVector GetLocation() const { return Location; }

protected:
	float MassFactor = 1.0f; // 질량 보정용

	const float GravityAccel = 2.f;    // 중력 가속도
	const float RestitutionWall = 0.1f;
	const float FloorFriction = 0.4f;

private:
	float Random()
	{
		return (float)rand() / (float)RAND_MAX; // [0, 1]
	}

	float RandRange(float a, float b)
	{
		return a + (b - a) * Random();
	}
};

class UWidget
{
public:
	UWidget() :
		Parent(nullptr),
		Scale(1)
	{

	}

	void SetParent(UWidget* InWidget)
	{
		// Offset 은 Scale = 1 기준이므로 Scale 을 배제한 GetLocation 사용
		if (InWidget)
		{
			FVector PrevLocation = GetLocation(false);
			Parent = InWidget;
			Offset = PrevLocation - Parent->GetLocation(false);
		}
		else
		{
			if (Parent)
			{
				// parent 초기화하기 전에 현재 위치를 Offset 으로 지정 (parent == nullptr 일 때 Offset 은 location 임을 기억)
				Offset = GetLocation(false);
				Parent = nullptr;
			}
			else
			{
				// do nothing
			}
		}
	}

	void SetScale(float InScale) { Scale = InScale; }

	// Scale = 1 기준, Parent 와의 Offset 설정
	// Parent == nullptr 인 경우 Location 설정과 같음
	// Offset = Self.Loc - Parent.Loc
	// Self.Loc = Parent.Loc + Offset
	void SetOffset(FVector InOffset) { Offset = InOffset; }

	FVector GetLocation(bool bApplyScale) const
	{
		if (Parent)
		{
			FVector Location = Parent->GetLocation(bApplyScale);
			if (bApplyScale) Location = Location + Offset * GetParentScale();
			return Location;
		}
		else
		{
			return Offset;
		}
	}

	float GetParentScale() const
	{
		if (Parent)
		{
			return Parent->GetScale() * Parent->GetParentScale();
		}
		else
		{
			return 1;
		}
	}

	float GetScale() const
	{
		return Scale;
	}

	virtual void Render(URenderer& renderer)
	{

	}

protected:
	// Cartesian <-> Screen 변환 과정을 어느 객체에서 또는 함수에서 할 것인가는 논의 필요
	FVector CartesianToScreenCoord(FVector CartesianCoord)
	{
		// Screen Size 가 (900, 1100) 고정 상황 가정
		return FVector(round((CartesianCoord.x + 1.0f) * 0.5f * 900), round((1 - (CartesianCoord.y + 1.0f) * 0.5f) * 1100));
	}

private:
	UWidget* Parent;
	float Scale;  // 실제 Offset 에 영향을 줌

	// Parent == nullptr 인 경우 Offset 이 곧 Location 이 됨
	// Parent != nullptr 인 경우 Parent 로부터의 상대적 거리를 나타내는 Offset (Scale = 1 기준)
	FVector Offset;
};

/*
UTextWidget Render 후 UImageWidget Render 가 제대로 작동하지 않거나
Parent-Child 위치 관계가 보존이 안되는 등의 문제가 있어 현재로썬 단일 객체 + Rendering 순

*/
class UTextWidget : public UWidget
{
public:
	void Render(URenderer& renderer) override
	{
		/*
			UI
		*/
		DirectX::SimpleMath::Vector2 textSize = renderer.Font->MeasureString(Text.c_str());

		renderer.SpriteBatch->Begin();

		// Screen 좌표 계산
		FVector ScreenCoord = CartesianToScreenCoord(GetLocation(true));

		// DrawString 중앙 정렬: Origin = 텍스트 중앙
		DirectX::XMFLOAT2 origin = { textSize.x / 2.f, textSize.y / 2.f };

		/*
		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			DirectX::Colors::White,
			0.0f,
			origin,                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);
		*/

		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			DirectX::Colors::White,
			0.0f,
			DirectX::XMFLOAT2(0, 0),                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);

		renderer.SpriteBatch->End();
	}

	void SetNumber(int Num)
	{
		Text = std::to_wstring(Num);
	}

	void SetText(const wchar_t* InText)
	{
		Text = InText;
	}

private:
	std::wstring Text;
};

class UImageWidget : public UWidget
{
public:
	UImageWidget() :
		Texture(nullptr),
		Radius(1)
	{
	}

	void Render(URenderer& renderer) override
	{
		FVector Location = GetLocation(true);
		renderer.UpdateConstant(Location, 0, Radius * GetScale() * GetParentScale(), true);
		renderer.UpdateTexture(Texture);
		renderer.RenderRectangle();
	}

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }
	void SetRadius(float InRadius) { Radius = InRadius; }

private:
	UTexture* Texture;
	float Radius;
};

enum EFruitType
{
	FT_Cherry = 0,
	FT_Strawberry,
	FT_Grape,
	FT_Dekopon,      // 귤/데코폰
	FT_Orange,
	FT_Apple,
	FT_Pear,
	FT_Peach,
	FT_Pineapple,
	FT_Melon,
	FT_Watermelon,

	FT_Count  // Last Type Check 용도
};


class UParticle : public UBall
{
public:
	FVector Location;
	FVector Velocity;
	float Radius;
public:
	UParticle() :
		Location(0.0f, 0.0f, 0.0f),
		Velocity(0.0f, 0.0f, 0.0f),
		Radius(0.1f)
	{
	}

	~UParticle()
	{

	}

	//TODO 아랫벽에 부딪혔을 때 속도 선형 감소
	void UpdatePhysics(float dt) override
	{
		Velocity = Velocity + dt;

		Location = Location + Velocity * EaseOut(LifeTime / MaxLifeTime) * dt;
		LifeTime += dt;

		if (LifeTime >= MaxLifeTime) bActive = false;
	}

	void Render(URenderer& renderer) override
	{
		renderer.UpdateConstant(Location, Angle, Radius, 1.0f);
		renderer.UpdateTexture(GetTexture());
		renderer.RenderRectangle();
	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FVector GetLocation() const { return Location; }

	void SetActive(bool bIsActive, float angleRand) {
		Angle = angleRand;
		Angle += RandRange(-10.f, 10.f);
		float radian = Angle * 3.141592f / 180.f;
		MaxLifeTime = RandRange(0.3f, 0.5f);
		LifeTime = 0.f;
		Velocity.x = cos(radian);
		Velocity.y = sin(radian);

		bActive = bIsActive;
	};
	bool IsActive() { return bActive; };

	void SetAngle(float _Angle)
	{
		Angle = _Angle;
	}
	void SetMaxLifeTime(float time)
	{
		MaxLifeTime = time;
	}

private:
	float Random()
	{
		return (float)rand() / (float)RAND_MAX; // [0, 1]
	}

	float RandRange(float a, float b)
	{
		return a + (b - a) * Random();
	}
	float EaseOut(float t)
	{
		return 1.0f - (1.0f - t) * (1.0f - t);
	}
private:
	bool bActive = { false };
	float LifeTime = { 0.f };
	float MaxLifeTime = { 0.f };
};

class UFruit : public UBall
{
public:
	UFruit()
	{
		FruitType = EFruitType::FT_Cherry;
	}

	void UpdatePhysics(
		float dt
	) override
	{
		Location = Location + Velocity * dt;

		if (Location.x < LeftBorder)
		{
			Location.x = LeftBorder;
			Velocity.x *= -1.0f;
		}
		if (Location.x > RightBorder)
		{
			Location.x = RightBorder;
			Velocity.x *= -1.0f;
		}
		if (Location.y > TopBorder)
		{
			Location.y = TopBorder;
			Velocity.y *= -RestitutionWall;
		}
		if (Location.y < BottomBorder)
		{
			Location.y = BottomBorder;

			//바닥 마찰력 적용
			float normalImpulse = fabsf(Velocity.y) * (1.0f + RestitutionWall);

			Velocity.y *= -RestitutionWall;

			float frictionImpulse = FloorFriction * normalImpulse;
			if (fabsf(Velocity.x) <= frictionImpulse)
				Velocity.x = 0.0f;
			else
				Velocity.x -= (Velocity.x > 0.0f ? 1.0f : -1.0f) * frictionImpulse;

		}

		Velocity.y -= GravityAccel * dt;

		float speed = sqrtf(Velocity.x * Velocity.x + Velocity.y * Velocity.y);

		//각속도를 현재 속도의 크기를 이용하여 조절
		AngularVelocity *= 0.98f;

		const float MaxAngularSpeed = 5.0f;
		if (AngularVelocity > MaxAngularSpeed) AngularVelocity = MaxAngularSpeed;
		else if (AngularVelocity < -MaxAngularSpeed) AngularVelocity = -MaxAngularSpeed;

		Angle += AngularVelocity * dt;
	}

	bool CheckLineCollision(float lineY)
	{
		float distance = fabs(Location.y - lineY);
		return distance <= Radius;
	}

	void SetFruitType(EFruitType InFruitType)
	{
		FruitType = InFruitType;
		Radius = SmallestFruitRadius * pow(CoefRadiusGrowth, InFruitType);

		Mass = Radius * Radius * MassFactor;

		LeftBorder = -1.0f + Radius;
		RightBorder = 1.0f - Radius;
		TopBorder = 1.0f - Radius;
		BottomBorder = -1.0f + Radius;
	}

	EFruitType GetFruitType() const { return FruitType; }

private:
	const float SmallestFruitRadius = 0.06f; // 가장 작은 과일의 반지름
	const float CoefRadiusGrowth = 1.25f; // 과일 레벨이 하나씩 올라갈 때마다 반지름이 커지는 정도

	EFruitType FruitType;
};

// RAII 고려 필요 (자원 해제가 지금은 제대로 안됨)
// 스마트포인터 사용 필요
UFruit* CreateFruit(EFruitType FruitType)
{
	UFruit* Fruit = new UFruit();
	Fruit->SetFruitType(FruitType);
	return Fruit;
}

UFruit* CreateFruitAt(EFruitType FruitType, FVector Location, UTexture* InTexture = nullptr)
{
	UFruit* Fruit = new UFruit();
	Fruit->SetFruitType(FruitType);
	Fruit->SetLocation(Location);
	Fruit->SetTexture(InTexture);

	return Fruit;
}

template<typename DataType>
class TQueue
{
public:
	TQueue()
	{
		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
		Head = 0;
		Tail = 0;
	}

	~TQueue()
	{
		delete[] Data;
	}

	// Push, Pop, Reserve
	bool Push(const DataType& InData)
	{
		// Array is full
		if (DataNum == Capacity)
		{
			bool bIsReserveOK = Reserve(Capacity * 2);
			if (!bIsReserveOK) return false;
		}

		Data[Tail] = InData;
		Tail = (Tail + 1) % Capacity;
		DataNum++;
		return true;
	}

	void Clear()
	{
		delete[] Data;

		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
		Head = 0;
		Tail = 0;
	}

	void Pop()
	{
		if (IsEmpty()) return;
		Head = (Head + 1) % Capacity;
		DataNum--;
	}

	DataType& Front()
	{
		// Empty 체크 나중에 넣어야할듯
		return Data[Head];
	}

	const DataType& Front() const
	{
		// Empty 체크 나중에 넣어야할듯
		return Data[Head];
	}

	bool Reserve(UINT NewCapacity)
	{
		// 더 작아지는 경우는 고려하지 않음
		if (Capacity >= NewCapacity) return false;

		DataType* NewData = new DataType[NewCapacity];

		// 객체가 특수한 복사, 이동 연산을 가지는 것은 아직 고려하지 않음
		for (UINT i = 0; i < DataNum; i++)
		{
			NewData[i] = Data[(Head + i) % Capacity];
		}

		delete[] Data;
		Data = NewData;
		// DataNum 유지
		Capacity = NewCapacity;
		Head = 0;
		Tail = DataNum;
		return true;
	}

	bool IsEmpty() const
	{
		return DataNum == 0;
	}

	UINT Num() const
	{
		return DataNum;
	}

private:
	DataType* Data;
	UINT Head;
	UINT Tail;
	UINT DataNum;
	UINT Capacity;
};

template<typename DataType>
class TArray
{
public:
	TArray()
	{
		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
	}

	~TArray()
	{
		delete[] Data;
	}

	// PushBack, PopBack
	bool PushBack(const DataType& InData)
	{
		if (DataNum == Capacity)
		{
			bool bIsReserveOK = Reserve(Capacity * 2);
			if (!bIsReserveOK) return false;
		}

		Data[DataNum] = InData;
		DataNum++;
		return true;
	}

	void Clear()
	{
		delete[] Data;

		Data = new DataType[1];
		DataNum = 0;
		Capacity = 1;
	}

	void PopBack()
	{
		if (IsEmpty()) return;

		DataNum--;
	}

	DataType& Back()
	{
		// Empty 처리 나중에 해야됨
		return Data[DataNum - 1];
	}

	DataType& operator[](UINT Idx)
	{
		// 일단 Out of index 는 체크 안함
		return Data[Idx];
	}

	const DataType& operator[](UINT Idx) const
	{
		return Data[Idx];
	}

	bool IsEmpty() const
	{
		return DataNum == 0;
	}

	UINT Num() const
	{
		return DataNum;
	}

	bool Reserve(UINT NewCapacity)
	{
		// 더 작아지는 경우는 허용 안함
		if (Capacity >= NewCapacity) return false;

		DataType* NewData = new DataType[NewCapacity];
		for (UINT i = 0; i < DataNum; i++)
		{
			NewData[i] = Data[i];
		}

		delete[] Data;
		Data = NewData;
		// DataNum 유지
		Capacity = NewCapacity;
		return true;
	}

	void RemoveAt(UINT Idx)
	{
		if (Idx >= DataNum)
		{
			return;
		}

		if (Idx != DataNum - 1)
		{
			Data[Idx] = Data[DataNum - 1];
		}

		DataNum--;

		// 원소의 소멸자를 별도로 호출하지 않음에 유의
	}

private:
	DataType* Data;
	UINT DataNum;
	UINT Capacity;
};

struct FPendingMerge
{
	EFruitType ResultFruitType;
	FVector NewLocation;
};


/*
* 파티클 특성상 합쳐질때마다 생성을 할 경우 생명주기 관리가 어렵고
  생성되는데 비용도 고려하여 풀을 만듦
*/
class UParticlePool
{
public:
	UParticlePool() = default;
	~UParticlePool() = default;

public:
	/*머지를 위해 임시적으로 Texture를 Pool에 둠*/
	void Initialize(int DefaultCapacity, URenderer& renderer)
	{
		ParticleTexture = new UTexture;
		ParticleTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Star.png");

		ParticlePool.Reserve(DefaultCapacity);
		for (int i = 0; i < DefaultCapacity; ++i)
		{
			UParticle* particle = new UParticle();
			particle->SetActive(false, 0);
			particle->SetTexture(ParticleTexture);
			ParticlePool.Push(particle);
		}
	}
	void ReturnAll()
	{
		for (UINT i = 0; i < ParticleList.Num(); ++i)
		{
			//강제로 풀에 반납
			ParticleList[i]->SetActive(true, 0);
			Return(i);
		}
	}
	void ReleasePool()
	{
		while (!ParticlePool.IsEmpty())
		{
			UParticle* particle = ParticlePool.Front();
			ParticlePool.Pop();
			delete particle;
		}
		for (int i = 0; i < ParticleList.Num(); ++i)
		{
			delete ParticleList[i];
			ParticleList[i] = nullptr;
		}
		if (ParticleTexture)
		{
			ParticleTexture->Release();
			delete ParticleTexture;
			ParticleTexture = nullptr;
		}
	}
	void Update(float dt)
	{
		for (UINT i = 0; i < ParticleList.Num(); ++i)
		{
			ParticleList[i]->UpdatePhysics(dt);
		}

		for (UINT i = 0; i < ParticleList.Num(); ++i)
		{
			if (!ParticleList[i]->IsActive())
			{
				Return(i);
			}
		}
	}

	void Render(URenderer& renderer)
	{
		for (int i = 0; i < ParticleList.Num(); ++i)
		{
			ParticleList[i]->Render(renderer);

		}
	}
	void Aquire(FVector Location)
	{
		//소유권 이동
		if (ParticlePool.IsEmpty()) return;

		UINT pairticleNum = 5;
		if (pairticleNum % 2 == 0) ++pairticleNum;

		for (UINT i = 1; i <= pairticleNum; ++i)
		{
			UParticle* particle = ParticlePool.Front();
			particle->SetLocation(Location);
			particle->SetActive(true, 360 * i / pairticleNum);

			//각도 설정
			ParticlePool.Pop();
			ParticleList.PushBack(particle);
		}
	}

	void Reset()
	{
		for (UINT i = 0; i < ParticleList.Num(); ++i)
		{
			if (ParticleList[i])
			{
				ParticleList[i]->SetActive(false, 0);
				ParticlePool.Push(ParticleList[i]);
				ParticleList[i] = nullptr;
			}
		}

		ParticleList.Clear();
	}

private:
	void Return(int idx)
	{
		ParticleList[idx]->SetActive(false, 0);

		ParticlePool.Push(ParticleList[idx]);

		ParticleList.RemoveAt(idx);
	}


private:
	TQueue<UParticle*> ParticlePool;
	TArray<UParticle*> ParticleList;
	UTexture* ParticleTexture;
};


class USoundManager
{
public:
	USoundManager() = default;
	~USoundManager() = default;
public:
	void Initialize()
	{
		FMOD::System_Create(&System);
		System->init(512, FMOD_INIT_NORMAL, nullptr);
		System->createChannelGroup("BGM", &BGMGroup);
		System->createChannelGroup("SFX", &SFXGroup);
	}

	void Release()
	{
		for (auto MyPair : Sounds)
		{
			MyPair.second->release();
		}

		if (BGMGroup) BGMGroup->release();
		if (SFXGroup) SFXGroup->release();
		System->close();
		System->release();
	}

	void LoadSound(std::wstring Path, bool bIsLoop)
	{
		FMOD_MODE Mode = (bIsLoop) ? (FMOD_LOOP_NORMAL) : (FMOD_DEFAULT);

		if (Sounds.find(Path) == Sounds.end())
		{
			System->createSound(std::string(Path.begin(), Path.end()).c_str(), Mode, nullptr, &Sounds[Path]);
		}
	}

	// BGM: 하나만 재생 (이전 BGM 정지 후 교체)
	void PlayBGM(std::wstring Path, float Volume = 1.f)
	{
		if (Sounds.find(Path) == Sounds.end()) return;

		if (BGMChannel) BGMChannel->stop();

		System->playSound(Sounds[Path], BGMGroup, false, &BGMChannel);
		if (BGMChannel) BGMChannel->setVolume(Volume);
	}

	void PlaySFX(std::wstring Path, float Volume = 1.f)
	{
		if (Sounds.find(Path) == Sounds.end()) return;

		FMOD::Channel* Ch = nullptr;
		System->playSound(Sounds[Path], SFXGroup, false, &Ch);
		if (Ch) Ch->setVolume(Volume);
	}

	void StopAll()
	{
		if (BGMGroup) BGMGroup->stop();
		if (SFXGroup) SFXGroup->stop();
	}

	void Update()
	{
		System->update();
	}


private:
	FMOD::System* System = nullptr;
	FMOD::ChannelGroup* BGMGroup = nullptr;
	FMOD::ChannelGroup* SFXGroup = nullptr;
	FMOD::Channel* BGMChannel = nullptr;

	std::unordered_map<std::wstring, FMOD::Sound*> Sounds;

};

class FScoreManager
{
public:
	FScoreManager() : Score(0) {}

	/**
	 * @param FruitType 병합된 후의 과일 타입
	 */
	void AddScore(EFruitType FruitType)
	{
		Score += (UINT)((FruitType * (FruitType + 1)) / 2);
	}
	UINT GetScore() const
	{
		return Score;
	}

	void Reset()
	{
		Score = 0;
	}
private:
	UINT Score;
};

// main 브랜치가 바뀌는 부분을 모두 추적하기 어려워서 게임 매니저는 개발이 어느정도 끝난 후 리팩토링 하면서 추가하겠숩니다..
//class FGameManager;

// 우선 게임 상태는 간단하게 enum으로 관리
enum EGameState
{
	GS_Title,
	GS_Credits,
	GS_Playing,
	GS_GameOver
};

EGameState GameState = GS_Title;

const float GameOverLimit = 1.5f;

static void UpdateTitle(URenderer& renderer)
{
	static ID3D11ShaderResourceView* TitleBackgroundSRV = nullptr;

	if (TitleBackgroundSRV == nullptr)
	{
		UTexture* TitleBackgroundTexture = new UTexture();
		TitleBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/title_bg.png");
		TitleBackgroundSRV = TitleBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID) TitleBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* StartButtonText = "START";
	ImVec2 buttonSize = ImGui::CalcTextSize(StartButtonText);

	ImGui::SetCursorPos(ImVec2((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f, (ImGui::GetIO().DisplaySize.y - buttonSize.y) * 0.5f));

	if (ImGui::Button(StartButtonText))
	{
		GameState = GS_Playing;

		TitleBackgroundSRV->Release();
		TitleBackgroundSRV = nullptr;
	}

	const char* CreditsButtonText = "CREDITS";
	buttonSize = ImGui::CalcTextSize(CreditsButtonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);
	if (ImGui::Button(CreditsButtonText))
	{
		GameState = GS_Credits;
	}

	ImGui::PopStyleColor();

	ImGui::End();
}

static void UpdateCredits(URenderer& renderer)
{
	static ID3D11ShaderResourceView* CreditsBackgroundSRV = nullptr;

	if (CreditsBackgroundSRV == nullptr)
	{
		UTexture* CreditsBackgroundTexture = new UTexture();
		CreditsBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/credits_bg.png");
		CreditsBackgroundSRV = CreditsBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID)CreditsBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* text = "Amazing Developers!!!";
	ImVec2 textSize = ImGui::CalcTextSize(text);

	float centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	float centerY = (ImGui::GetIO().DisplaySize.y - textSize.y) * 0.5f;

	ImGui::SetCursorPos(ImVec2(centerX, centerY));
	ImGui::Text("%s", text);

	textSize = ImGui::CalcTextSize("KJS");
	centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("KJS");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("PJH");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("JMJ");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("JJY");

	ImGui::Spacing();

	const char* buttonText = "BACK";
	ImVec2 buttonSize = ImGui::CalcTextSize(buttonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);

	if (ImGui::Button(buttonText))
	{
		GameState = GS_Title;

		CreditsBackgroundSRV->Release();
		CreditsBackgroundSRV = nullptr;
	}

	ImGui::PopStyleColor();

	ImGui::End();
}

static void UpdateGameOver(URenderer& renderer,FScoreManager& ScoreManager)
{
	static ID3D11ShaderResourceView* GameOverBackgroundSRV = nullptr;

	if (GameOverBackgroundSRV == nullptr)
	{
		UTexture* GameOverBackgroundTexture = new UTexture();
		GameOverBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/gameover_bg.png");
		GameOverBackgroundSRV = GameOverBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID)GameOverBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* text = "Game Over";
	ImVec2 textSize = ImGui::CalcTextSize(text);

	float centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	float centerY = (ImGui::GetIO().DisplaySize.y - textSize.y) * 0.5f;

	ImGui::SetCursorPos(ImVec2(centerX, centerY));
	ImGui::Text("%s", text);

	ImGui::SetCursorPosX(centerX);
	ImGui::Text("Score: %d", ScoreManager.GetScore());

	ImGui::Spacing();

	const char* buttonText = "Restart";
	ImVec2 buttonSize = ImGui::CalcTextSize(buttonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);

	if (ImGui::Button(buttonText))
	{
		//Reset();
		ScoreManager.Reset();

		GameState = GS_Playing;

		GameOverBackgroundSRV->Release();
		GameOverBackgroundSRV = nullptr;
	}

	ImGui::PopStyleColor();

	ImGui::End();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static void ResolveCollision(UBall* a, UBall* b);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	std::srand(std::time(0));

	WCHAR WindowClass[] = L"GTLWindowClass";

	WCHAR Title[] = L"Game Tech Lab";

	WNDCLASSW wndclass = {
		0,
		WndProc,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		WindowClass
	};

	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1024,
		1024,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	URenderer renderer;
	renderer.Create(hWnd); // 현재는 공 모델만 사용하기 때문에 정점 버퍼는 Create() 내에서 한 번 생성
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;
	const float fixedDeltaTime = (float)targetFrameTime / 1000; // 60fps 고정 상황 가정

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

	FScoreManager ScoreManager;
	USoundManager SoundManager;
	UParticlePool ParticlePool;

	SoundManager.Initialize();

	//BGM은 mp3파일
	SoundManager.LoadSound(TEXT("./Sound/BGM/Menu.mp3"), true);

	//SFX는 wav파일
	SoundManager.LoadSound(TEXT("./Sound/SFX/Drop.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/Merge.wav"), false);

	SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));

	//연쇄적인 12개의 충돌을 고려해 100개
	ParticlePool.Initialize(100, renderer);


	TArray<UPrimitive*> PrimitiveList;

	const float LineY = 0.8f;

	bool bIsExit = false;

	// 이전 프레임의 선 충돌 여부
	bool prevCollision = false;

	UFruit previewFruit;

	LARGE_INTEGER collisionStartCounter;

	static const char* FruitNames[] = {
		"Cherry", "Strawberry", "Grape", "Dekopon", "Orange",
		"Apple", "Pear", "Peach", "Pineapple", "Melon", "Watermelon"
	};

	TArray<UTexture*> FruitTextures;
	for (int i = 0; i < ARRAYSIZE(FruitNames); i++)
	{
		std::string FruitPath = "./Resource/";
		FruitPath += FruitNames[i];
		FruitPath += ".png";
		FruitTextures.PushBack(new UTexture(renderer.Device, renderer.DeviceContext, FruitPath.c_str()));
	}

	UTexture* BubbleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/bubble.png");
	UTexture* NextTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/NextText.png");

	UImageWidget ScoreWidget, TestWidget, NextTextWidget;
	UTextWidget TestTextWidget;

	ScoreWidget.SetOffset(FVector(0.7, 0.7, 0));
	ScoreWidget.SetTexture(BubbleTexture);
	ScoreWidget.SetScale(1);
	ScoreWidget.SetRadius(0.1);

	TestWidget.SetTexture(FruitTextures[0]);
	TestWidget.SetScale(1);
	TestWidget.SetRadius(0.05);
	TestWidget.SetParent(&ScoreWidget);
	TestWidget.SetOffset(FVector(0, 0, 0));

	NextTextWidget.SetTexture(NextTextTexture);
	NextTextWidget.SetScale(1);
	NextTextWidget.SetRadius(0.1);
	NextTextWidget.SetParent(&ScoreWidget);
	NextTextWidget.SetOffset(FVector(0, 0.1, 0));

	

	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);

		MSG msg;

		// ----- input ------

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);

			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}
		
		// 일단 임시로 게임 상태 저장
		EGameState FrameState = GameState;

		switch (FrameState)
		{
			case GS_Title:
				renderer.Prepare();
				UpdateTitle(renderer);
				break;
			case GS_Credits:
				renderer.Prepare();
				UpdateCredits(renderer);
				break;
			case GS_Playing:
			{
				POINT mousePos;
				GetCursorPos(&mousePos);
				ScreenToClient(hWnd, &mousePos);

				float spawnX = mousePos.x / renderer.ViewportInfo.Width;
				spawnX = max(0.0f, min(1.0f, spawnX));
				spawnX = spawnX * 2.0f - 1.0f;

				static bool prevMouseDown = false;
				bool spawnFruit = false;

				bool curMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

				if (curMouseDown && !prevMouseDown)
				{
					spawnFruit = true;
				}

				prevMouseDown = curMouseDown;

				bool bLineCollision = false;


				static int SelectedFruitType = 0;

				renderer.Prepare();
				renderer.PrepareShader();

				SoundManager.Update();

				EFruitType ft = (EFruitType)SelectedFruitType;

				// 생성
				if (spawnFruit)
				{
					EFruitType ft = (EFruitType)SelectedFruitType;

					// 새 과일 생성
					float offsetAngle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
					float offsetDist = 0.01f;
					spawnX += cosf(offsetAngle) * offsetDist;

					FVector spawnPos = FVector(spawnX, 0.9f, 0);

					PrimitiveList.PushBack(CreateFruitAt(ft, spawnPos, FruitTextures[ft]));

					SelectedFruitType = rand() % (EFruitType::FT_Orange + 1);
				}

				// 병합 예정 정보를 담은 큐
				TQueue<FPendingMerge> PendingMergeQueue;

				for (UINT i = 0; i < PrimitiveList.Num(); i++)
				{
					PrimitiveList[i]->UpdatePhysics(fixedDeltaTime);
				}
				ParticlePool.Update(fixedDeltaTime);

				// 과일들 간 충돌 / 병합 연산
				for (UINT i = 0; i < PrimitiveList.Num(); i++)
				{
					UFruit* A = (UFruit*)PrimitiveList[i];

					if (A->IsPendingDestroy())
					{
						continue;
					}

					for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
					{
						UFruit* B = (UFruit*)PrimitiveList[j];

						if (B->IsPendingDestroy())
						{
							continue;
						}

						FVector Delta = B->Location - A->Location;
						float Dist2 = Delta.Dot2(Delta);
						float r = A->Radius + B->Radius;

						if (Dist2 <= r * r)
						{
							if (A->GetFruitType() == B->GetFruitType())
							{
								A->MarkPendingDestroy();
								B->MarkPendingDestroy();

								// 수박끼리 병합 시 두 수박만 사라지고 병합 X
								if (A->GetFruitType() != EFruitType::FT_Watermelon) {
									//FVector TargetLocation = (A->Location.y < B->Location.y) ? A->Location : B->Location;
									FVector TargetLocation = A->Location;

									PendingMergeQueue.Push({
										(EFruitType)(A->GetFruitType() + 1),
										TargetLocation
										});

									//병합된 곳에서 파티클 생성
									ParticlePool.Aquire(TargetLocation);
									SoundManager.PlaySFX(TEXT("./Sound/SFX/Merge.wav"));
								}
								else
								{
									ScoreManager.AddScore(EFruitType::FT_Watermelon);
								}
							}
						}
					}
				}

				// 과일 삭제 로직
				for (UINT i = 0; i < PrimitiveList.Num();)
				{
					if (PrimitiveList[i]->IsPendingDestroy())
					{
						delete PrimitiveList[i];
						PrimitiveList[i] = nullptr;
						PrimitiveList.RemoveAt(i);
					}
					else
					{
						i++;
					}
				}

				// 병합 처리 로직
				while (!PendingMergeQueue.IsEmpty())
				{
					FPendingMerge MergeInfo = PendingMergeQueue.Front();
					PendingMergeQueue.Pop();

					// 점수 획득
					ScoreManager.AddScore(MergeInfo.ResultFruitType);
					PrimitiveList.PushBack(CreateFruitAt(MergeInfo.ResultFruitType, MergeInfo.NewLocation, FruitTextures[EFruitType::FT_Peach]));
				}

				//새 과일이 생성된 후 마찰 적용
				const static int overlapIteration = 10;
				for (UINT i = 0; i < overlapIteration; ++i)
				{
					for (UINT i = 0; i < PrimitiveList.Num(); i++)
					{
						UFruit* A = (UFruit*)PrimitiveList[i];

						for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
						{
							UFruit* B = (UFruit*)PrimitiveList[j];

							ResolveCollision(A, B);
						}
					}
				}

				for (UINT i = 0; i < PrimitiveList.Num(); i++)
				{
					PrimitiveList[i]->Render(renderer);
				}

				ParticlePool.Render(renderer);

				for (UINT i = 0; i < PrimitiveList.Num(); i++)
				{
					UFruit* fruit = (UFruit*)PrimitiveList[i];

					if (fruit->CheckLineCollision(LineY))
					{
						bLineCollision = true;

						if (!prevCollision)
						{
							QueryPerformanceCounter(&collisionStartCounter);
						}

						break;
					}
				}
				// Game Over 여부 계산
				if (bLineCollision)
				{
					LARGE_INTEGER now;
					QueryPerformanceCounter(&now);

					double elapsed = (double)(now.QuadPart - collisionStartCounter.QuadPart) / frequency.QuadPart;

					if (elapsed >= GameOverLimit)
					{
						// 데이터 초기화. 게임 매니저가 있으면 UpdateGameOver 함수에서 GameState가 GS_Playing으로 바뀔 때 초기화하는 쪽으로 리팩토링
						for (UINT i = 0; i < PrimitiveList.Num(); i++)
						{
							delete PrimitiveList[i];
							PrimitiveList[i] = nullptr;
						}
						PrimitiveList.Clear();

						ParticlePool.Reset();

						prevCollision = false;

						prevMouseDown = false;
						SelectedFruitType = 0;

						GameState = GS_GameOver;

						break;
					}
				}

				prevCollision = bLineCollision;

				previewFruit.SetFruitType((EFruitType)SelectedFruitType);
				previewFruit.SetLocation(FVector(spawnX, 0.9f, 0));
				previewFruit.SetTexture(FruitTextures[SelectedFruitType]);
				previewFruit.Render(renderer);

				renderer.UpdateConstant(FVector(0, 0, 0), 0.f, 1.0f, 0.0f);
				renderer.UpdateTexture(nullptr);
				renderer.UpdateLineColor(bLineCollision);
				renderer.RenderLine();

				ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
				ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
				ImGui::NewFrame(); // ImGui Core 초기화

				ImGui::Begin("Team4 Game");

				ImGui::Combo("Fruit Type", &SelectedFruitType, FruitNames, sizeof(FruitNames) / sizeof(*FruitNames));
				ImGui::Text("Score: %d", ScoreManager.GetScore());

				ImGui::End();

				break;
			}
			case GS_GameOver:
				renderer.Prepare();
				UpdateGameOver(renderer, ScoreManager);
				break;
		}

		ImGui::Render(); // 이번 프레임에 그릴 UI 설정
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출

		if (FrameState == GS_Playing)
		{
			ScoreWidget.Render(renderer);
			TestWidget.Render(renderer);
			NextTextWidget.Render(renderer);
		}

		renderer.SwapBuffer();

		// 모니터 주사율이 144Hz일 수도 있으니 VSync를 사용하더라도 60fps 고정 코드는 유지
		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);

			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
	}

	SoundManager.StopAll();
	SoundManager.Release();

	//파티클 텍스쳐도 삭제
	ParticlePool.ReleasePool();

	for (int i = 0; i < FruitTextures.Num(); i++)
	{
		FruitTextures[i]->Release();
	}

	for (UINT i = 0; i < PrimitiveList.Num(); i++) {
		delete PrimitiveList[i];
		PrimitiveList[i] = nullptr;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		{
			return true;
		}
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

static void ResolveCollision(UBall* A, UBall* B)
{
	FVector Delta = B->Location - A->Location;
	float Dist2 = Delta.Dot2(Delta);
	float r = A->Radius + B->Radius;

	if (Dist2 >= r * r)
	{
		return;
	}

	float Dist = sqrtf(Dist2);

	FVector Normal;
	if (Dist < 0.0001f)
	{
		float angle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
		Normal = FVector(cosf(angle), sinf(angle), 0.f);
	}
	else
	{
		float InverseDist = 1.0f / Dist;
		Normal = FVector(Delta.x * InverseDist, Delta.y * InverseDist, 0.0f);
	}


	// 안정성을 위해 공들이 겹치는 경우 처리
	float Penetration = r - Dist;
	float MA = A->Mass;
	float MB = B->Mass;
	float MoveA = Penetration * (MB / (MA + MB));
	float MoveB = Penetration * (MA / (MA + MB));

	A->Location = A->Location - (Normal * MoveA);
	B->Location = B->Location + (Normal * MoveB);

	// 관성 모멘트 
	float IA = 0.5f * MA * A->Radius * A->Radius;
	float IB = 0.5f * MB * B->Radius * B->Radius;
	float InvIA = 1.0f / IA, InvIB = 1.0f / IB;

	//접선의 방향
	FVector Tangent = FVector(-Normal.y, Normal.x, 0.f);

	// 충격량 적용 전 접촉점 속도 v + w corss r
	FVector vA_contact = FVector(
		A->Velocity.x - A->AngularVelocity * Normal.y * A->Radius,
		A->Velocity.y + A->AngularVelocity * Normal.x * A->Radius,
		0.f);

	FVector vB_contact = FVector(
		B->Velocity.x + B->AngularVelocity * Normal.y * B->Radius,
		B->Velocity.y - B->AngularVelocity * Normal.x * B->Radius,
		0.f);

	FVector RelVel = vB_contact - vA_contact;
	float RelVelNorm = RelVel.Dot2(Normal);

	// 이미 멀어지는 중이면 무시
	if (RelVelNorm > 0.0f)
		return;

	float RelVelTang = RelVel.Dot2(Tangent);

	const float e = 0.5f;
	float jN = -(1.0f + e) * RelVelNorm / (1 / MA + 1 / MB);

	float jT = 0.f;
	if (fabsf(RelVelTang) > 0.1f)
		jT = -RelVelTang / (1 / MA + 1 / MB + A->Radius * A->Radius * InvIA + B->Radius * B->Radius * InvIB);

	//마찰력 적용
	const float mu = 0.6f;
	float maxFriction = mu * fabsf(jN);
	if (fabsf(jT) > maxFriction)
		jT = (jT > 0.f ? 1.f : -1.f) * maxFriction;

	A->Velocity = A->Velocity - (Normal * (jN * 1 / MA)) - (Tangent * (jT * 1 / MA));
	B->Velocity = B->Velocity + (Normal * (jN * 1 / MB)) + (Tangent * (jT * 1 / MB));

	//충격량이 존재할 때만 각속도를 변화
	if (jN > 0.05f)
	{
		A->AngularVelocity -= A->Radius * jT * InvIA;
		B->AngularVelocity += B->Radius * jT * InvIB;
	}

}
