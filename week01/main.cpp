#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <unordered_map>

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

// From DirectXTK
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "SimpleMath.h"

#include <fmod.hpp>
#pragma comment (lib, "fmod_vc.lib")

#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

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


class UInputManager
{
public:
	UInputManager() :
		bIsPrevMouseDown(false),
		bIsMouseDown(false),
		bIsMousePressed(false),
		bIsMouseReleased(false),
		bIsXPadConnected(false),
		MousePosition(0, 0),
		XPadStickX(0),
		XPadStickY(0),
		bIsXPadADown(false),
		bIsXPadTriggerDown(false)
	{
	}

	void Update(HWND hWnd, float dt)
	{
		bool bCurrent = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

		bIsMousePressed = (!bIsPrevMouseDown && bCurrent);
		bIsMouseReleased = (bIsPrevMouseDown && !bCurrent);
		bIsMouseDown = bCurrent;
		bIsPrevMouseDown = bCurrent;

		GetCursorPos(&MousePosition);
		ScreenToClient(hWnd, &MousePosition);

		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		DWORD result = XInputGetState(0, &state);
		bIsXPadConnected = (result == ERROR_SUCCESS);

		if (bIsXPadConnected)
		{
			// 패드 연결됨

			// [-32768, 32767]
			SHORT lx = state.Gamepad.sThumbLX;
			SHORT ly = state.Gamepad.sThumbLY;

			float Magnitude = sqrt(lx * lx + ly * ly);
			const float DEADZONE = 8689;

			// HW Noise 배제
			if (Magnitude < DEADZONE)
			{
				// Not Holding
				lx = 0;
				ly = 0;

				if (bIsHoldingRepeatStarted)
				{
					bIsHoldingRepeatStarted = false;
					bIsHoldingRepeatTriggered = false;
					HoldRepeatTime = 0;
				}
			}
			else
			{
				// Holding
				if (bIsHoldingRepeatStarted)
				{
					HoldRepeatTime += dt;
					if (HoldRepeatTime >= 0.3f)
					{
						bIsHoldingRepeatTriggered = true;
						// dt 가 큰 경우 0.3f 를 크게 벗어날 수 있으므로 이를 고려해서 HoldRepeatTime 수정
						HoldRepeatTime -= 0.3f;
					}
					else
					{
						bIsHoldingRepeatTriggered = false;
					}
				}
				else
				{
					// 첫 시작시엔 바로 Trigger
					bIsHoldingRepeatStarted = true;
					bIsHoldingRepeatTriggered = true;
					HoldRepeatTime = 0;
				}
			}


			// lx, ly => [-1, 1] 정규화
			XPadStickX = lx / 32768.f;
			XPadStickY = ly / 32768.f;

			bIsXPadADown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
			bIsXPadAPressed = bIsXPadADown && !bIsPrevXPadADown;
			bIsXPadAReleased = !bIsXPadADown && bIsPrevXPadADown;
			bIsPrevXPadADown = bIsXPadADown;

			bIsXPadTriggerDown = (state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD || state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		}
	}

	bool IsMousePressed() const { return bIsMousePressed; }
	bool IsMouseReleased() const { return bIsMouseReleased; }
	bool IsMouseDown() const { return bIsMouseDown; }

	bool IsXPadConnected() const { return bIsXPadConnected; }
	POINT GetMousePosition() const { return MousePosition; }

	float GetXPadStickX() const { return XPadStickX; }
	float GetXPadStickY() const { return XPadStickY; }

	bool IsXPadADown() const { return bIsXPadADown; }
	bool IsXPadAPressed() const { return bIsXPadAPressed; }
	bool IsXPadAReleased() const { return bIsXPadAReleased; }

	bool IsXPadTriggerDown() const { return bIsXPadTriggerDown; }
	bool IsXPadHoldingRepeatTriggered() const { return bIsHoldingRepeatTriggered; }

private:
	bool bIsPrevMouseDown = false;
	bool bIsMouseDown = false;
	bool bIsMousePressed = false;
	bool bIsMouseReleased = false;

	bool bIsPrevXPadADown = false;
	bool bIsXPadADown = false;
	bool bIsXPadAPressed = false;
	bool bIsXPadAReleased = false;

	bool bIsXPadConnected;
	POINT MousePosition;

	// [-1, 1] Normalized
	float XPadStickX;
	float XPadStickY;

	bool bIsXPadTriggerDown;

	bool bIsHoldingRepeatStarted;
	bool bIsHoldingRepeatTriggered;
	float HoldRepeatTime;
};

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

	~UTexture()
	{
		// Release와 동시에 객체 자체의 메모리도 해제되도록
		Release();
	}

	bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName)
	{
		D3D11_TEXTURE2D_DESC TextureDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		UINT RowPitch;
		HRESULT Result;
		unsigned char* Data;

		Data = stbi_load(FileName, &Width, &Height, &Channels, 4);

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

		stbi_image_free(Data);
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

	static FVector Splat(float Scalar)
	{
		return FVector(Scalar, Scalar, Scalar);
	}
};

struct SceneFrame
{
	ImVec2 Pos;
	ImVec2 Size;
};

#include "Sphere.h"
#include "Rectangle.h"

const float LineY = 0.7f;

FVertexSimple line_vertices[] =
{
	{-1.0f, LineY, 0.0f, 1,1,1,1},
	{ 1.0f, LineY, 0.0f, 1,1,1,1}
};

class URenderer
{
public:
	struct FConstants
	{
		// 16B
		FVector Offset;
		float Padding1;

		// 16B
		FVector Scale;
		float Padding2;

		// 16B
		FVector SinCos;
		float UseTexture;   // 1 = texture, 0 = color

		FVector TintColor;
		float Padding3;
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

	void UpdateConstant(FVector Offset, float Angle, FVector Scale, float UseTexture, FVector Tint)
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

	void SwapBuffer()
	{
		SwapChain->Present(1, 0); // VSync 활성화
	}
};

const float MapBoarder = 0.76f;

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
	float LeftBorder = -0.76f;
	float RightBorder = 0.76f;
	float TopBorder = 1.0f;
	float BottomBorder = -0.9f;

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

		LeftBorder += Radius * 0.2f;
		RightBorder -= Radius * 0.2f;
		TopBorder -= Radius * 0.2f;
		BottomBorder += Radius * 0.2f;

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
		renderer.UpdateConstant(Location, Angle, FVector::Splat(Radius), 1.0f, FVector(1, 1, 1));
		renderer.UpdateTexture(GetTexture());
		renderer.RenderRectangle();
	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FVector GetLocation() const { return Location; }

	void BorderClamp()
	{
		if (Location.x < LeftBorder)
		{
			Location.x = LeftBorder;
		}
		if (Location.x > RightBorder)
		{
			Location.x = RightBorder;
		}
		if (Location.y < BottomBorder)
		{
			Location.y = BottomBorder;
		}
	}


protected:
	float MassFactor = 1.0f; // 질량 보정용

	const float GravityAccel = 2.f;    // 중력 가속도
	const float RestitutionWall = 0.1f;
	const float FloorFriction = 0.6f;

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
		// Screen Size 가 (1024, ) 고정 상황 가정
		return FVector((CartesianCoord.x + 1.0f) * 0.5f * 1024, (1 - CartesianCoord.y) * 0.5f * 1024);
	}

private:
	UWidget* Parent;
	float Scale;  // 실제 Offset 에 영향을 줌

	// Parent == nullptr 인 경우 Offset 이 곧 Location 이 됨
	// Parent != nullptr 인 경우 Parent 로부터의 상대적 거리를 나타내는 Offset (Scale = 1 기준)
	FVector Offset;
};

/*
 * UTextWidget Render 후 UImageWidget Render 가 제대로 작동하지 않거나
 * Parent-Child 위치 관계가 보존이 안되는 등의 문제가 있어 현재로썬 단일 객체 + Rendering 순서
 */
class UTextWidget : public UWidget
{
public:
	UTextWidget() :
		TextColor(DirectX::Colors::White)
	{
	}

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


		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			TextColor,
			0.0f,
			origin,                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);

		/*
		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			DirectX::Colors::White,
			0.0f,
			DirectX::XMFLOAT2(0, 0),                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);
		*/

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

	void SetTextColor(DirectX::XMVECTORF32 InTextColor)
	{
		TextColor = InTextColor;
	}

private:
	std::wstring Text;

	DirectX::XMVECTORF32 TextColor;
};

class UImageWidget : public UWidget
{
public:
	UImageWidget() :
		Texture(nullptr),
		AxisScale(FVector::Splat(1)),
		TintColor(FVector(1, 1, 1))
	{
	}

	void Render(URenderer& renderer) override
	{
		FVector Location = GetLocation(true);
		renderer.UpdateConstant(Location, 0.f, AxisScale * GetScale() * GetParentScale(), 1.0f, TintColor);
		renderer.UpdateTexture(Texture);
		renderer.RenderRectangle();
	}

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }
	void SetAxisScale(FVector InAxisScale) { AxisScale = InAxisScale; }
	void SetTintColor(FVector InTintColor) { TintColor = InTintColor; }
	FVector GetTintColor() { return TintColor; }

protected:
	UTexture* Texture;
	FVector AxisScale;
	FVector TintColor;
};

/*
 * 이미지가 없는 버튼도 있겠지만 이 게임 프로토타입에선 배제하여 이미지를 상속받음
 * 버튼의 가운데에 해당 텍스트를 넣는다면 렌더 순서가 꼬여 소유하지 않고 일단 외부 텍스트를 받도록 함
 */
class UButtonWidget : public UImageWidget
{
public:
	UButtonWidget() = default;

public:
	bool IsPressed(HWND hWnd, const UInputManager& InputManager, USoundManager& soundManger)
	{
		if (IsOnBound(hWnd) && InputManager.IsMouseReleased())
		{
			soundManger.PlaySFX(TEXT("./Sound/SFX/Click.wav"));
			return true;
		}
		return false;
	}

public:
	/*
	* 스크린 상에서 버튼 클릭 체크
	*/
	bool IsOnBound(HWND hWnd)
	{
		//NDC 좌표
		POINT lpPoint = {};
		GetCursorPos(&lpPoint);
		ScreenToClient(hWnd, &lpPoint);

		float MouseX = lpPoint.x;
		float MouseY = lpPoint.y;

		FVector CartesianCoord = GetLocation(true);

		FVector ScaledAxisScale = AxisScale * GetScale() * GetParentScale();
		FVector LeftTop = { CartesianCoord.x - ScaledAxisScale.x, CartesianCoord.y + ScaledAxisScale.y, 0.f };
		FVector RightBottom = { CartesianCoord.x + ScaledAxisScale.x, CartesianCoord.y - ScaledAxisScale.y, 0.f };

		LeftTop = { (LeftTop.x + 1.0f) * 0.5f * 1024, (1.f - LeftTop.y) * 0.5f * 1024 , 0 };
		RightBottom = { (RightBottom.x + 1.0f) * 0.5f * 1024,  (1.f - RightBottom.y) * 0.5f * 1024 , 0 };

		return (MouseX > LeftTop.x && MouseX < RightBottom.x && MouseY > LeftTop.y && MouseY < RightBottom.y);

	}
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
		renderer.UpdateConstant(Location, Angle, FVector::Splat(Radius), 1.0f, FVector(1, 1, 1));
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
		if (Location.y <= BottomBorder)
		{
			Location.y = BottomBorder;

			if (fabsf(Velocity.y) < 0.05f)
				Velocity.y = 0.0f;
			else
				Velocity.y *= -RestitutionWall;
		}

		ResolveWallCollision(dt);

		Velocity.y -= GravityAccel * dt;
		const float SleepThreshold = 0.01f;
		if (fabsf(Velocity.x) < SleepThreshold) Velocity.x = 0.0f;
		if (fabsf(Velocity.y) < SleepThreshold) Velocity.y = 0.0f;

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
		AngularVelocity -= Radius * 2;
		if (AngularVelocity < 0.f)AngularVelocity = 0.f;

		//최소 포도의 질량을 가지도록 설정
		float GrapeRadius = SmallestFruitRadius * pow(CoefRadiusGrowth, 2);

		Mass = max(GrapeRadius * GrapeRadius, Radius * Radius);

		LeftBorder += Radius;
		RightBorder -= Radius;
		TopBorder -= Radius;
		BottomBorder += Radius;
	}

	EFruitType GetFruitType() const { return FruitType; }

private:
	/** 
	* 옆과 아래 벽과의 마찰
	*/
	void ResolveWallCollision(float dt)
	{
		float normalImpulse = Mass * GravityAccel;
		float frictionImpulse = FloorFriction * normalImpulse;

		if (Location.y <= LeftBorder)
		{
			if (fabsf(Velocity.x) <= frictionImpulse)
				Velocity.x = 0.0f;
			else
				Velocity.x -= (Velocity.x > 0.0f ? 1.0f : -1.0f) * frictionImpulse;
		}
		// 옆벽 마찰 
		if (Location.x <= LeftBorder || Location.x >= RightBorder)
		{
			float frictionImpulse = FloorFriction * normalImpulse;
			if (fabsf(Velocity.y) <= frictionImpulse)
				Velocity.y = 0.0f;
			else
				Velocity.y -= (Velocity.y > 0.0f ? 1.0f : -1.0f) * frictionImpulse;
		}
	}

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
 * 생성되는데 비용도 고려하여 풀을 만듦
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
		ParticleTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/Star.png");

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


const UINT RANK_NUM = 5;

class FScoreManager
{
public:
	FScoreManager() : Score(0)
	{
		ReadHighScoreData();
	}

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

	int UpdateHighScore()
	{
		for (int i = 0; i < RANK_NUM; i++)
		{
			if (Score > HighScores[i])
			{
				for (int j = RANK_NUM - 1; j > i; j--)
				{
					HighScores[j] = HighScores[j - 1];
				}

				HighScores[i] = Score;

				WriteHighScoreData();

				return i;
			}
		}
		return -1; // 랭킹 진입 실패
	}

	UINT* GetHighScores() {
		return HighScores;
	}
private:
	void ReadHighScoreData()
	{
		std::ifstream HighScoreData("highscore_data", std::ios::binary);

		if (!HighScoreData)
		{
			return;
		}

		HighScoreData.read(reinterpret_cast<char*>(HighScores), sizeof(HighScores));
	}

	void WriteHighScoreData()
	{
		std::ofstream HighScoreData("highscore_data", std::ios::binary); // 덮어쓰기

		HighScoreData.write(reinterpret_cast<const char*>(HighScores), sizeof(HighScores));
	}
private:
	UINT Score;
	UINT HighScores[5] = { 0 }; // 0:최고기록
};

// main 브랜치가 바뀌는 부분을 모두 추적하기 어려워서 게임 매니저는 개발이 어느정도 끝난 후 리팩토링 하면서 추가하겠숩니다..
//class FGameManager;

// 우선 게임 상태는 간단하게 enum으로 관리
enum EGameState
{
	GS_Title,
	GS_Credits,
	GS_Playing,
	GS_GameOver,
	GS_HowToPlay,
	GS_LeaderBoard
};
EGameState GameState = GS_Title;

const float DangerLimit = 0.6f;
const float GameOverLimit = 2.5f;
const double BlinkInterval = 0.2f;

static void UpdateTitle(URenderer& renderer)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

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
		(ImTextureID)TitleBackgroundSRV,
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
	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

static void UpdateCredits(URenderer& renderer)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

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

	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

static void UpdateGameOver(URenderer& renderer, FScoreManager& ScoreManager)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

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
		ScoreManager.UpdateHighScore();
		ScoreManager.Reset();

		GameState = GS_Playing;

		GameOverBackgroundSRV->Release();
		GameOverBackgroundSRV = nullptr;
	}

	ImGui::PopStyleColor();

	ImGui::End();

	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static void ResolveCollision(UBall* a, UBall* b);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static inline EFruitType GetRandomFruitType()
{
	return static_cast<EFruitType>(rand() % (EFruitType::FT_Orange + 1));
}

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

	RECT rect = { 0, 0, 1024, 1024 };
	AdjustWindowRect(&rect, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindowExW(0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,   // 타이틀바 포함한 전체 크기
		rect.bottom - rect.top,
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

	LARGE_INTEGER startTime, endTime, lastBlinkTime;
	double elapsedTime = 0.0;

	static bool BlinkInitialized = false;

	if (!BlinkInitialized)
	{
		QueryPerformanceCounter(&lastBlinkTime);
		BlinkInitialized = true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

	FScoreManager ScoreManager;
	UInputManager InputManager;
	USoundManager SoundManager;
	UParticlePool ParticlePool;

	SoundManager.Initialize();

	//BGM은 mp3파일
	SoundManager.LoadSound(TEXT("./Sound/BGM/Menu.mp3"), true);

	//SFX는 wav파일
	SoundManager.LoadSound(TEXT("./Sound/SFX/Drop.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/Merge.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/Click.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/GameWin.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/GameOver.wav"), false);


	SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));

	//연쇄적인 12개의 충돌을 고려해 100개
	ParticlePool.Initialize(100, renderer);


	TArray<UPrimitive*> PrimitiveList;

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

	// 여기 데이터 해제 필요함!!!
	UTexture* EmptyTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/preview_empty.png");
	UTexture* BubbleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/bubble.png");
	UTexture* ButtonBGTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Normal.png");
	UTexture* ButtonBGHoverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Hover.png");
	UTexture* NextTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/NextText.png");
	UTexture* ScoreTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/ScoreText.png");
	UTexture* Map1Texture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1.png");
	UTexture* Map1RoofTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1_Roof.png");
	UTexture* TitleBackgroundTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/title_bg.png");
	UTexture* RuleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rule_bg.png");
	UTexture* CreditTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/credits.png");
	UTexture* GameOverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/gameover_bg.png");
	UTexture* LeaderBoardBgTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/leaderboard_bg.png");

	UImageWidget ScoreWidget, TestWidget, NextTextWidget;
	UTextWidget TestTextWidget;

	ScoreWidget.SetOffset(FVector(0.7, 0.7, 0));
	ScoreWidget.SetTexture(BubbleTexture);
	ScoreWidget.SetScale(1);
	ScoreWidget.SetAxisScale(FVector::Splat(0.1));

	TestWidget.SetScale(1);
	TestWidget.SetAxisScale(FVector::Splat(0.05));
	TestWidget.SetParent(&ScoreWidget);
	TestWidget.SetOffset(FVector(0, 0, 0));

	NextTextWidget.SetTexture(NextTextTexture);
	NextTextWidget.SetScale(1);
	NextTextWidget.SetAxisScale(FVector::Splat(0.1));
	NextTextWidget.SetParent(&ScoreWidget);
	NextTextWidget.SetOffset(FVector(0, 0.1, 0));

	UImageWidget MapWidget;

	MapWidget.SetTexture(Map1Texture);
	MapWidget.SetAxisScale(FVector::Splat(1));
	MapWidget.SetOffset(FVector(0, -0.15, 0));

	UImageWidget MapRoofWidget;

	MapRoofWidget.SetTexture(Map1RoofTexture);
	MapRoofWidget.SetAxisScale(FVector(1, 0.1, 0));
	MapRoofWidget.SetOffset(FVector(0, 0.6, 0));

	UTexture* LeaderBoardTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/leaderboard.png");
	UTexture* RankBgTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rankbg.png");

	UImageWidget LeaderBoardWidget;
	UImageWidget* RankBg = new UImageWidget[RANK_NUM];
	UTextWidget* RankNum = new UTextWidget[RANK_NUM];
	UTextWidget* RankScoreIngame = new UTextWidget[RANK_NUM];
	UTextWidget* RankScore = new UTextWidget[RANK_NUM];

	LeaderBoardWidget.SetOffset(FVector(-0.8, 0.3, 0));
	LeaderBoardWidget.SetTexture(LeaderBoardTexture);
	LeaderBoardWidget.SetScale(1);
	LeaderBoardWidget.SetAxisScale(FVector::Splat(0.2));


	for (int i = 0; i < RANK_NUM; i++)
	{

		RankBg[i].SetTexture(RankBgTexture);
		RankBg[i].SetScale(0.5);
		RankBg[i].SetAxisScale(FVector::Splat(0.28));
		RankBg[i].SetParent(&LeaderBoardWidget);
		RankBg[i].SetOffset(FVector(0, 0.08 - (0.06 * i), 0));

		RankNum[i].SetNumber(i + 1);
		RankNum[i].SetScale(1);
		RankNum[i].SetParent(&RankBg[i]);
		RankNum[i].SetOffset(FVector(-0.19, 0.015, 0));

		RankScoreIngame[i].SetScale(1);
		RankScoreIngame[i].SetParent(&RankBg[i]);
		RankScoreIngame[i].SetOffset(FVector(0, 0.015, 0));

		RankScore[i].SetScale(2);
		RankScore[i].SetTextColor(DirectX::Colors::Black);
		RankScore[i].SetOffset(FVector(0.1, 0.29 - (0.215 * i), 0));
	}
	float spawnX = 0;
	float VibrationTime = 0.f;
	const float MaxVibrationTime = 0.3f;
	bool bIsVibrating = false;

	//왼쪽 점수
	UImageWidget LeftScoreWidget, ScoreTextWidget;
	UTextWidget  CurrentScoreWidget;

	LeftScoreWidget.SetOffset(FVector(-0.7f, 0.7f, 0.f));
	LeftScoreWidget.SetTexture(BubbleTexture);
	LeftScoreWidget.SetScale(1.f);
	LeftScoreWidget.SetAxisScale({ 0.1, 0.1f, 1.f });

	ScoreTextWidget.SetTexture(ScoreTextTexture);
	ScoreTextWidget.SetScale(1);
	ScoreTextWidget.SetParent(&LeftScoreWidget);

	ScoreTextWidget.SetAxisScale(FVector::Splat(0.1));
	ScoreTextWidget.SetOffset(FVector(0, 0.1, 0));

	CurrentScoreWidget.SetText(TEXT("0"));
	CurrentScoreWidget.SetParent(&LeftScoreWidget);
	CurrentScoreWidget.SetScale(1.f);
	CurrentScoreWidget.SetOffset(FVector(0.f, 0.0f, 0.f));

	// 게임 오버 점수 UI
	UImageWidget GameOverScoreImage, GameOverScoreTextImage;
	UTextWidget  GameOverScoreText;

	GameOverScoreImage.SetOffset(FVector(0.f, -0.2f, 0.f));
	GameOverScoreImage.SetTexture(FruitTextures[10]);
	GameOverScoreImage.SetScale(1.5f);
	GameOverScoreImage.SetAxisScale({ 0.1, 0.1f, 1.f });

	GameOverScoreTextImage.SetTexture(ScoreTextTexture);
	GameOverScoreTextImage.SetScale(1);
	GameOverScoreTextImage.SetParent(&GameOverScoreImage);
	GameOverScoreTextImage.SetAxisScale(FVector::Splat(0.1));
	GameOverScoreTextImage.SetOffset(FVector(0, 0.1, 0));

	GameOverScoreText.SetText(TEXT("0"));
	GameOverScoreText.SetParent(&GameOverScoreImage);
	GameOverScoreText.SetScale(1.f);
	GameOverScoreText.SetTextColor(DirectX::Colors::White);
	GameOverScoreText.SetOffset(FVector(0.f, 0.0f, 0.f));


	UButtonWidget StartButton, CreditButton, RuleButton, LeaderBoardButton, BackButton, RestartButton, BackToTitleButton;
	UTextWidget StartText, CreditText, RuleText, LeaderBoardText, BackText, RestartText, BackToTitleText;
	UImageWidget StartScreenImage, HowToPlayImage, CreditImage, GameOverImage, LeaderBoardImage;

	RestartButton.SetOffset(FVector(-0.3f, -0.5f, 0.f));
	RestartButton.SetTexture(ButtonBGTexture);
	RestartButton.SetAxisScale({ 0.2, 0.1f, 1.f });

	RestartText.SetParent(&RestartButton);
	RestartText.SetOffset({ 0, 0, 0 });
	RestartText.SetScale(1);
	RestartText.SetText(L"Restart");

	BackToTitleButton.SetOffset(FVector(0.3f, -0.5f, 0.f));
	BackToTitleButton.SetTexture(ButtonBGTexture);
	BackToTitleButton.SetAxisScale({ 0.2, 0.1f, 1.f });

	BackToTitleText.SetParent(&BackToTitleButton);
	BackToTitleText.SetOffset({ 0, 0, 0 });
	BackToTitleText.SetScale(1);
	BackToTitleText.SetText(L"Back To Title");

	StartButton.SetOffset(FVector(0.f, 0.f, 0.f));
	StartButton.SetTexture(ButtonBGTexture);
	StartButton.SetAxisScale({ 0.2, 0.07f, 1.f });

	StartText.SetParent(&StartButton);
	StartText.SetScale(1);
	StartText.SetText(L"Game Start");

	CreditButton.SetParent(&StartButton);
	CreditButton.SetTexture(ButtonBGTexture);
	CreditButton.SetAxisScale({ 0.2, 0.07, 1.f });
	CreditButton.SetOffset({ 0.f, -0.2f, 0.f });

	CreditText.SetParent(&CreditButton);
	CreditText.SetScale(1);
	CreditText.SetText(L"Credits");

	RuleButton.SetParent(&StartButton);
	RuleButton.SetTexture(ButtonBGTexture);
	RuleButton.SetAxisScale({ 0.2, 0.07, 1.f });
	RuleButton.SetOffset({ 0.f, -0.4f, 0.f });

	RuleText.SetParent(&RuleButton);
	RuleText.SetScale(1);
	RuleText.SetText(L"How To Play");

	LeaderBoardButton.SetParent(&StartButton);
	LeaderBoardButton.SetTexture(ButtonBGTexture);
	LeaderBoardButton.SetAxisScale({ 0.2, 0.07, 1.f });
	LeaderBoardButton.SetOffset({ 0.f, -0.6f, 0.f });

	LeaderBoardText.SetParent(&LeaderBoardButton);
	LeaderBoardText.SetScale(1);
	LeaderBoardText.SetText(L"Leader Board");

	StartScreenImage.SetTexture(TitleBackgroundTexture);
	StartScreenImage.SetOffset({ 0, 0, 0 });
	StartScreenImage.SetAxisScale({ 1, 1, 1 });
	StartScreenImage.SetScale(1);

	HowToPlayImage.SetTexture(RuleTexture);
	HowToPlayImage.SetOffset({ 0, 0, 0 });
	HowToPlayImage.SetAxisScale({ 1, 1, 1 });
	HowToPlayImage.SetScale(1);

	CreditImage.SetTexture(CreditTexture);
	CreditImage.SetOffset({ 0, 0, 0 });
	CreditImage.SetAxisScale({ 1, 1, 1 });
	CreditImage.SetScale(1);

	GameOverImage.SetTexture(GameOverTexture);
	GameOverImage.SetOffset({ 0, 0, 0 });
	GameOverImage.SetAxisScale({ 1, 1, 1 });
	GameOverImage.SetScale(1);

	LeaderBoardImage.SetTexture(LeaderBoardBgTexture);
	LeaderBoardImage.SetOffset({ 0, 0, 0 });
	LeaderBoardImage.SetAxisScale({ 1, 1, 1 });
	LeaderBoardImage.SetScale(1);

	BackButton.SetOffset(FVector(-0.8f, 0.8f, 0.f));
	BackButton.SetTexture(ButtonBGTexture);
	BackButton.SetAxisScale({ 0.1, 0.05f, 1.f });

	BackText.SetParent(&BackButton);
	BackText.SetOffset({ 0, 0, 0 });
	BackText.SetScale(1);
	BackText.SetText(L"Back");

	const float SpawnCooldown = 0.5f; // 과일 생성 간 최소 시간 간격 (초)
	LARGE_INTEGER LastSpawnTime;
	QueryPerformanceCounter(&LastSpawnTime);

	// GameState 변화 간 Index 를 초기화해줘야 하는데 지금 구조 상으로 아직 안돼있어서 나중에 고쳐야할듯함
	int XPadButtonIndex = 0;

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

		InputManager.Update(hWnd, fixedDeltaTime);

		// 임시 게임 상태 저장
		EGameState FrameState = GameState;

		switch (FrameState)
		{
		case GS_Title:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				if (InputManager.IsXPadHoldingRepeatTriggered())
				{
					if (InputManager.GetXPadStickY() < 0)
					{
						XPadButtonIndex = (XPadButtonIndex + 1) % 3;
					}
					else
					{
						if (XPadButtonIndex == 0) XPadButtonIndex = 2;
						else XPadButtonIndex -= 1;
					}
				}

				// 모든 버튼 Hover -> Normal
				// 비효율적이긴한데 시간이 없어서 우선 이렇게 처리
				StartButton.SetTexture(ButtonBGTexture);
				CreditButton.SetTexture(ButtonBGTexture);
				RuleButton.SetTexture(ButtonBGTexture);
				LeaderBoardButton.SetTexture(ButtonBGTexture);

				switch (XPadButtonIndex)
				{
				case 0:
					StartButton.SetTexture(ButtonBGHoverTexture);
					break;

				case 1:
					CreditButton.SetTexture(ButtonBGHoverTexture);
					break;

				case 2:
					RuleButton.SetTexture(ButtonBGHoverTexture);
					break;

				case 3:
					LeaderBoardButton.SetTexture(ButtonBGHoverTexture);
					break;
				}

				if (InputManager.IsXPadAReleased())
				{
					switch (XPadButtonIndex)
					{
					case 0:
						GameState = GS_Playing;
						break;

					case 1:
						GameState = GS_Credits;
						break;

					case 2:
						GameState = GS_HowToPlay;
						break;

					case 3:
						GameState = GS_LeaderBoard;
						break;
					}
				}
			}
			else
			{
				if (StartButton.IsOnBound(hWnd))
					StartButton.SetTexture(ButtonBGHoverTexture);
				else
					StartButton.SetTexture(ButtonBGTexture);

				if (CreditButton.IsOnBound(hWnd))
					CreditButton.SetTexture(ButtonBGHoverTexture);
				else
					CreditButton.SetTexture(ButtonBGTexture);

				if (RuleButton.IsOnBound(hWnd))
					RuleButton.SetTexture(ButtonBGHoverTexture);
				else
					RuleButton.SetTexture(ButtonBGTexture);

				if (LeaderBoardButton.IsOnBound(hWnd))
					LeaderBoardButton.SetTexture(ButtonBGHoverTexture);
				else
					LeaderBoardButton.SetTexture(ButtonBGTexture);

				if (StartButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_Playing;
				}
				else if (CreditButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_Credits;
				}
				else if (RuleButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_HowToPlay;
				}
				else if (LeaderBoardButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_LeaderBoard;
				}
			}

			StartScreenImage.Render(renderer);
			StartButton.Render(renderer);
			CreditButton.Render(renderer);
			RuleButton.Render(renderer);
			LeaderBoardButton.Render(renderer);

			StartText.Render(renderer);
			CreditText.Render(renderer);
			RuleText.Render(renderer);
			LeaderBoardText.Render(renderer);

			break;

		case GS_Credits:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				BackButton.SetTexture(ButtonBGHoverTexture);
				if (InputManager.IsXPadAReleased())
				{
					GameState = GS_Title;
				}
			}
			else
			{
				if (BackButton.IsOnBound(hWnd))
					BackButton.SetTexture(ButtonBGHoverTexture);
				else
					BackButton.SetTexture(ButtonBGTexture);

				if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_Title;
				}
			}

			CreditImage.Render(renderer);

			BackButton.Render(renderer);
			BackText.Render(renderer);

			break;

		case GS_HowToPlay:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				BackButton.SetTexture(ButtonBGHoverTexture);
				if (InputManager.IsXPadAReleased())
				{
					GameState = GS_Title;
				}
			}
			else
			{
				if (BackButton.IsOnBound(hWnd))
					BackButton.SetTexture(ButtonBGHoverTexture);
				else
					BackButton.SetTexture(ButtonBGTexture);

				if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_Title;
				}
			}

			HowToPlayImage.Render(renderer);
			BackButton.Render(renderer);
			BackText.Render(renderer);

			break;

		case GS_LeaderBoard:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				BackButton.SetTexture(ButtonBGHoverTexture);
				if (InputManager.IsXPadAReleased())
				{
					GameState = GS_Title;
				}
			}
			else
			{
				if (BackButton.IsOnBound(hWnd))
					BackButton.SetTexture(ButtonBGHoverTexture);
				else
					BackButton.SetTexture(ButtonBGTexture);

				if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					GameState = GS_Title;
				}
			}

			LeaderBoardImage.Render(renderer);
			BackButton.Render(renderer);
			BackText.Render(renderer);

			for (int i = 0; i < RANK_NUM; i++)
			{
				RankScore[i].SetNumber(ScoreManager.GetHighScores()[i]);
				RankScore[i].Render(renderer);
			}

			break;
		case GS_Playing:
		{
			static bool prevMouseDown = false;

			bool spawnFruit = false;

			if (InputManager.IsXPadConnected())
			{
				if (InputManager.IsXPadTriggerDown())
					spawnX += InputManager.GetXPadStickX() * 0.01f;
				else
					spawnX += InputManager.GetXPadStickX() * 0.03f;

				spawnX = max(min(spawnX, 1), -1);

				if (bIsVibrating)
				{
					VibrationTime += fixedDeltaTime;
					if (VibrationTime >= MaxVibrationTime)
					{
						// 진동 종료
						XINPUT_VIBRATION vibration{};
						vibration.wLeftMotorSpeed = 0;
						vibration.wRightMotorSpeed = 0;
						XInputSetState(0, &vibration);
						bIsVibrating = false;
						VibrationTime = 0;
					}
				}

				bool curMouseDown = InputManager.IsXPadADown();

				if (curMouseDown && !prevMouseDown)
				{
					LARGE_INTEGER currentTime;
					QueryPerformanceCounter(&currentTime);

					float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
					if (timeSinceLastSpawn >= SpawnCooldown)
					{
						spawnFruit = true;
						LastSpawnTime = currentTime;
					}
				}

				prevMouseDown = curMouseDown;
			}
			else
			{
				POINT mousePos = InputManager.GetMousePosition();

				spawnX = mousePos.x / renderer.ViewportInfo.Width;
				spawnX = max(0.0f, min(1.0f, spawnX));
				spawnX = spawnX * 2.0f - 1.0f;

				// 화면 바깥을 클릭해도 과일이 생성되는 문제 해결
				bool isMouseInScene = (mousePos.y >= renderer.ViewportInfo.TopLeftY) && (mousePos.y <= renderer.ViewportInfo.TopLeftY + renderer.ViewportInfo.Height) &&
					(mousePos.x >= renderer.ViewportInfo.TopLeftX) && (mousePos.x <= renderer.ViewportInfo.TopLeftX + renderer.ViewportInfo.Width);

				bool curMouseDown = InputManager.IsMouseDown();

				if (curMouseDown && !prevMouseDown && isMouseInScene)
				{
					LARGE_INTEGER currentTime;
					QueryPerformanceCounter(&currentTime);

					float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
					if (timeSinceLastSpawn >= SpawnCooldown)
					{
						spawnFruit = true;
						LastSpawnTime = currentTime;
					}
				}

				prevMouseDown = curMouseDown;
			}

			bool bLineCollision = false;

			static int CurrentFruitType = GetRandomFruitType();
			static int NextFruitType = GetRandomFruitType();

			renderer.Prepare();
			renderer.PrepareShader();

			SoundManager.Update();

			MapWidget.Render(renderer);

			EFruitType ft = (EFruitType)CurrentFruitType;

			// 새 과일 생성
			if (spawnFruit)
			{
				EFruitType ft = (EFruitType)CurrentFruitType;

				float offsetAngle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
				float offsetDist = 0.01f;
				spawnX += cosf(offsetAngle) * offsetDist;

				FVector spawnPos = FVector(spawnX, 0.9f, 0);

				SoundManager.PlaySFX(TEXT("./Sound/SFX/Drop.wav"), 1.f);
				PrimitiveList.PushBack(CreateFruitAt(ft, spawnPos, FruitTextures[ft]));

				// 다음 과일
				CurrentFruitType = NextFruitType;
				NextFruitType = GetRandomFruitType();
			}
			TestWidget.SetTexture(FruitTextures[NextFruitType]);
			// 병합 예정 정보를 담은 큐
			TQueue<FPendingMerge> PendingMergeQueue;

			for (UINT i = 0; i < PrimitiveList.Num(); i++)
			{
				PrimitiveList[i]->UpdatePhysics(fixedDeltaTime);
			}
			ParticlePool.Update(fixedDeltaTime);

			// 과일 간 충돌 계산 시 기준이 너무 빡빡하면 가끔씩 충돌이 감지되지 않는 경우가 있어서 약간의 여유를 둠
			const float MergeSlop = 0.005f;

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

					if (Dist2 <= r * r + MergeSlop)
					{
						if (A->GetFruitType() == B->GetFruitType())
						{
							if (InputManager.IsXPadConnected())
							{
								// 패드 연결 상태
								XINPUT_VIBRATION vibration{};
								vibration.wLeftMotorSpeed = 500 * (A->GetFruitType() * A->GetFruitType() + 1);
								vibration.wRightMotorSpeed = 500 * (A->GetFruitType() * A->GetFruitType() + 1);

								XInputSetState(0, &vibration);

								bIsVibrating = true;
								VibrationTime = 0.f;
							}

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
								ParticlePool.Aquire(A->Location);
								SoundManager.PlaySFX(TEXT("./Sound/SFX/Merge.wav"));
								SoundManager.PlaySFX(TEXT("./Sound/SFX/GameWin.wav"));
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

				PrimitiveList.PushBack(CreateFruitAt(MergeInfo.ResultFruitType, MergeInfo.NewLocation, FruitTextures[MergeInfo.ResultFruitType]));

				// 점수 획득
				ScoreManager.AddScore(MergeInfo.ResultFruitType);
			}

			CurrentScoreWidget.SetText(std::to_wstring(ScoreManager.GetScore()).c_str());

			//새 과일이 생성된 후 마찰 적용
			const static int overlapIteration = 100;
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
			// Danger 여부 계산
			if (bLineCollision)
			{
				LARGE_INTEGER now;
				QueryPerformanceCounter(&now);

				double elapsed = (double)(now.QuadPart - collisionStartCounter.QuadPart) / frequency.QuadPart;

				if (elapsed >= DangerLimit)
				{
					LARGE_INTEGER now;
					QueryPerformanceCounter(&now);

					double blinkElapsed = (double)(now.QuadPart - lastBlinkTime.QuadPart) / frequency.QuadPart;

					if (blinkElapsed >= BlinkInterval)
					{
						if (MapRoofWidget.GetTintColor().y == 0 &&
							MapRoofWidget.GetTintColor().z == 0)
						{
							MapRoofWidget.SetTintColor(FVector(1, 1, 1));
						}
						else
						{
							MapRoofWidget.SetTintColor(FVector(1, 0, 0));
						}

						lastBlinkTime = now;
					}
				}
			}
			else
			{
				MapRoofWidget.SetTintColor(FVector(1, 1, 1));
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

					CurrentFruitType = GetRandomFruitType();
					NextFruitType = GetRandomFruitType();

					SoundManager.StopAll();
					SoundManager.PlaySFX(TEXT("./Sound/SFX/GameOver.wav"));

					GameState = GS_GameOver;

					break;
				}
			}

			prevCollision = bLineCollision;

			previewFruit.SetFruitType((EFruitType)CurrentFruitType);
			if (spawnX < ((-1) * MapBoarder) + previewFruit.Radius) { spawnX = ((-1) * MapBoarder) + previewFruit.Radius; }
			else if (spawnX > MapBoarder - previewFruit.Radius) { spawnX = MapBoarder - previewFruit.Radius; }
			previewFruit.SetLocation(FVector(spawnX, 0.9f, 0));

			// 과일이 생성 가능한 상태인지에 따라 프리뷰 이미지 변경
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
			if (timeSinceLastSpawn >= SpawnCooldown)
			{
				previewFruit.SetTexture(FruitTextures[CurrentFruitType]);
			}
			else
			{
				previewFruit.SetTexture(EmptyTexture);
			}

			previewFruit.Render(renderer);

			ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
			ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
			ImGui::NewFrame(); // ImGui Core 초기화

			ImGui::Begin("Team4 Game");

			ImGui::Combo("Fruit Type", &CurrentFruitType, FruitNames, sizeof(FruitNames) / sizeof(*FruitNames));
			ImGui::Text("Score: %d", ScoreManager.GetScore());
			if (ImGui::Button("Give Up"))
			{
				SoundManager.StopAll();
				SoundManager.PlayBGM(TEXT("./Sound/SFX/GameOver.wav"));
				GameState = GS_GameOver;
			}

			ImGui::End();
			ImGui::Render(); // 이번 프레임에 그릴 UI 설정
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출

			break;
		}
		case GS_GameOver:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				if (InputManager.IsXPadHoldingRepeatTriggered())
				{
					if (InputManager.GetXPadStickX() > 0)
					{
						XPadButtonIndex = (XPadButtonIndex + 1) % 2;
					}
					else
					{
						if (XPadButtonIndex == 0) XPadButtonIndex = 1;
						else XPadButtonIndex -= 1;
					}
				}

				// 모든 버튼 Hover -> Normal
				// 비효율적이긴한데 시간 문제로...
				RestartButton.SetTexture(ButtonBGTexture);
				BackToTitleButton.SetTexture(ButtonBGTexture);

				switch (XPadButtonIndex)
				{
				case 0:
					RestartButton.SetTexture(ButtonBGHoverTexture);
					break;

				case 1:
					BackToTitleButton.SetTexture(ButtonBGHoverTexture);
					break;
				}

				if (InputManager.IsXPadAReleased())
				{
					switch (XPadButtonIndex)
					{
					case 0:
						GameState = GS_Playing;
						ScoreManager.UpdateHighScore();
						ScoreManager.Reset();
						break;

					case 1:
						GameState = GS_Title;
						ScoreManager.UpdateHighScore();
						ScoreManager.Reset();
						XPadButtonIndex = 0;
						break;
					}
				}
			}
			else
			{
				if (RestartButton.IsOnBound(hWnd))
					RestartButton.SetTexture(ButtonBGHoverTexture);
				else
					RestartButton.SetTexture(ButtonBGTexture);

				if (BackToTitleButton.IsOnBound(hWnd))
					BackToTitleButton.SetTexture(ButtonBGHoverTexture);
				else
					BackToTitleButton.SetTexture(ButtonBGTexture);

				if (RestartButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
					GameState = GS_Playing;
					ScoreManager.UpdateHighScore();
					ScoreManager.Reset();
				}
				else if (BackToTitleButton.IsPressed(hWnd, InputManager, SoundManager))
				{
					SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
					GameState = GS_Title;
					ScoreManager.UpdateHighScore();
					ScoreManager.Reset();
				}
			}

			GameOverImage.Render(renderer);
			RestartButton.Render(renderer);
			BackToTitleButton.Render(renderer);

			GameOverScoreImage.Render(renderer);
			GameOverScoreTextImage.Render(renderer);

			RestartText.Render(renderer);
			BackToTitleText.Render(renderer);

			GameOverScoreText.SetText(std::to_wstring(ScoreManager.GetScore()).c_str());
			GameOverScoreText.Render(renderer);

			// 종료 시 진동 종료
			if (bIsVibrating)
			{
				// 진동 종료
				XINPUT_VIBRATION vibration{};
				vibration.wLeftMotorSpeed = 0;
				vibration.wRightMotorSpeed = 0;
				XInputSetState(0, &vibration);
				bIsVibrating = false;
				VibrationTime = 0;
			}

			break;
		}

		if (FrameState == GS_Playing)
		{
			MapRoofWidget.Render(renderer);

			LeftScoreWidget.Render(renderer);
			ScoreTextWidget.Render(renderer);

			ScoreWidget.Render(renderer);
			TestWidget.Render(renderer);
			NextTextWidget.Render(renderer);

			LeaderBoardWidget.Render(renderer);

			for (int i = 0; i < RANK_NUM; i++)
			{
				RankBg[i].Render(renderer);
			}
			for (int i = 0; i < RANK_NUM; i++)
			{
				RankNum[i].Render(renderer);
				RankScoreIngame[i].SetNumber(ScoreManager.GetHighScores()[i]);
				RankScoreIngame[i].Render(renderer);
			}

			NextTextWidget.Render(renderer);
			CurrentScoreWidget.Render(renderer);
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
		// Release와 동시에 FruitTextures[i] 자체도 해제
		delete FruitTextures[i];
	}

	for (UINT i = 0; i < PrimitiveList.Num(); i++) {
		delete PrimitiveList[i];
		PrimitiveList[i] = nullptr;
	}
	PrimitiveList.Clear();

	// 임시 메모리 해제
	delete LeaderBoardTexture;
	delete RankBgTexture;
	delete[] RankBg;
	delete[] RankNum;
	delete[] RankScoreIngame;
	delete[] RankScore;

	// 텍스처 해제
	delete EmptyTexture;
	delete BubbleTexture;
	delete ButtonBGTexture;
	delete NextTextTexture;
	delete ScoreTextTexture;
	delete Map1Texture;
	delete Map1RoofTexture;
	delete LeaderBoardBgTexture;


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

	const float Slop = 0.001f;
	const float Percent = 0.8f;
	float Correction = fmaxf(Penetration - Slop, 0.0f) / (1.0f / MA + 1.0f / MB) * Percent;
	float MoveA = Correction / MA;
	float MoveB = Correction / MB;

	A->Location = A->Location - (Normal * MoveA);
	B->Location = B->Location + (Normal * MoveB);

	A->BorderClamp();
	B->BorderClamp();

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

	const float e = 0.2f;
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
	if (jN > 0.01f)
	{
		A->AngularVelocity -= A->Radius * jT * InvIA;
		B->AngularVelocity += B->Radius * jT * InvIB;
	}
}
