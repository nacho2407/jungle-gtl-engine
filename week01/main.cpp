#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"


struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
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

	float Dot2(const FVector& rhs)
	{
		return x * rhs.x + y * rhs.y;
	}
};

#include "Sphere.h"

class URenderer
{
public:
	struct FConstants
	{
		FVector Offset;
		float  Radius;
		FVector SinCos;
		float Padding;
	};

	struct FSphere
	{
		ID3D11Buffer* VertexBuffer = nullptr;
		UINT NumVertices;
	} sphere;

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

	// 화면 초기화 색상
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

public:
	void Create(HWND hWindow)
	{
		CreateDeviceAndSwapChain(hWindow);

		CreateFrameBuffer();

		CreateRasterizerState();

		CreateSphereMesh();
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

	void Release()
	{
		ReleaseRasterizerState();

		// 렌더 타겟 초기화
		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		ReleaseFrameBuffer();
		ReleaseDeviceAndSwapChain();

		ReleaseSphereMesh();
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

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
	{
		vertexBuffer->Release();
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

	void UpdateConstant(FVector Offset, FVector SinCos, float Radius)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU에 있는 상수 버퍼를 CPU가 접근할 수 있도록 매핑

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;

			constants->Offset = Offset;
			constants->SinCos = SinCos;
			constants->Radius = Radius;

			DeviceContext->Unmap(ConstantBuffer, 0); // CPU 작업이 끝난 뒤 언매핑
		}
	}

	void Prepare()
	{
		DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DeviceContext->RSSetViewports(1, &ViewportInfo);
		DeviceContext->RSSetState(RasterizerState);

		DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}

	void PrepareShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
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
		RenderPrimitive(sphere.VertexBuffer, sphere.NumVertices);
	}

	void SwapBuffer()
	{
		SwapChain->Present(1, 0); // VSync 활성화
	}
}; // class URenderer

class UPrimitive
{
public:
	UPrimitive() = default;
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

protected:
    // Border 는 map 데이터이므로 따로 빼서 전역 상수 등으로 관리하는 것이 좋을 듯함
    // 이외 데이터도 관련해서 수정해야할수도
    float LeftBorder = -1.0f;
    float RightBorder = 1.0f;
    float TopBorder = 1.0f;
    float BottomBorder = -1.0f;

    float MassFactor = 1.0f; // 질량 보정용

    const float MinSpeed = -2.0f;
    const float MaxSpeed = 2.0f;

    const float GravityAccel = 4.0f; // 중력 가속도
    const float RestitutionWall = 0.8f; // 중력 활성화 시 적용할 벽 반발 감쇠 계수(1.0 == 완전 반사

    bool bPendingDestroy = false; // 충돌 검사 중 삭제될 예정인 Primitive들을 표시하기 위한 플래그
};

class UBall : public UPrimitive
{
public:
	FVector Location; // 여기서는 Offset과 동일한 개념
	FVector Velocity;
	
	float Angle;
	float AngularVelocity;

	float Radius;
	float Mass;
	
public:
    UBall(): 
        Location(0.0f, 0.0f, 0.0f), 
        Velocity(0.0f, 0.0f, 0.0f), 
        Radius(0.1f), 
        Mass(0.0f),
		Angle(0.f),
		AngularVelocity(0.f)
    {
        // Radius = RandRange(0.1f, 0.2f);
		Mass = Radius * MassFactor;

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

	//TODO 아랫벽에 부딪혔을 때 속도 선형 감소
	void UpdatePhysics(float dt) override
	{

	}

	void Render(URenderer& renderer) override
	{
		FVector CosSin = {cosf(Angle), sinf(Angle), 0.f };
		renderer.UpdateConstant(Location, CosSin, Radius);
		renderer.RenderSphere();
	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FVector GetLocation() const { return Location; }

protected:
	float MassFactor = 1.0f; // 질량 보정용

	float LeftBorder = -1.0f;
	float RightBorder = 1.0f;
	float TopBorder = 1.0f;
	float BottomBorder = -1.0f;

	const float DefaultSpeed = 1.0f;

	const float GravityAccel = 4.f;    // 중력 가속도
	const float RestitutionWall = 0.2f;
	const float FloorFriction = 0.1f;

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


class UPariticle : public UBall
{
public:
	FVector Location; // 여기서는 Offset과 동일한 개념
	FVector Velocity;
	float Radius;
public:
	UPariticle() :
		Location(0.0f, 0.0f, 0.0f),
		Velocity(0.0f, 0.0f, 0.0f),
		Radius(0.1f)
	{
	}

	~UPariticle()
	{

	}

	//TODO 아랫벽에 부딪혔을 때 속도 선형 감소
	void UpdatePhysics(float dt) override
	{
		Location = Location + Velocity * dt;
	}

	void Render(URenderer& renderer) override
	{
		FVector CosSin = { cosf(Angle), sinf(Angle), 0.f };
		renderer.UpdateConstant(Location, CosSin, Radius);
		renderer.RenderSphere();
	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FVector GetLocation() const { return Location; }

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

const float SmallestFruitRadius = 0.05f; // 가장 작은 과일의 반지름
const float CoefRadiusGrowth = 1.25f; // 과일 레벨이 하나씩 올라갈 때마다 반지름이 커지는 정도

class UFruit : public UBall
{
public:
    UFruit()
    {

    }

    // 특정 EFruitType에 해당하는 과일 객체를 특정 X 위치에 스폰하는 생성자 필요
	// 과일 반지름: SmallestFruitRadius * pow(CoefRadiusGrowth, FruitType)

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

			//마찰로 인한 토크 적용

		}


		Velocity.y -= GravityAccel * dt;

		//각가속도는 0.98로 고정
		AngularVelocity *= 0.98f;

		//각속도 제한
		const float MaxAngularSpeed = 20.0f;
		if (AngularVelocity > MaxAngularSpeed) AngularVelocity = MaxAngularSpeed;
		else if (AngularVelocity < -MaxAngularSpeed) AngularVelocity = -MaxAngularSpeed;

		Angle += AngularVelocity * dt;
    }

    void SetFruitType(EFruitType InFruitType)
    {
        FruitType = InFruitType;
    }

    EFruitType GetFruitType() const { return FruitType; }

private:
    // Fruit Type
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

UFruit* CreateFruitAt(EFruitType FruitType, FVector Location)
{
    UFruit* Fruit = new UFruit();
    Fruit->SetFruitType(FruitType);
    Fruit->SetLocation(Location);

    // 이미 존재하는 과일들 사이에 새로운 과일이 합쳐지는 경우 처리 로직은 준혁님 코드에 구현된 로직으로 처리 예정(차후 코드 병합 과정에서 처리)
    // 아마 PrimitiveList 내의 모든 과일들 위치를 처리해주어야 할 듯

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

class UScoreManager
{
public:
    UScoreManager() : Score(0) {}

    /**
     * @brief 원본 게임의 점수 규칙에 따라 점수를 누적합니다
     * 
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
private:
    UINT Score;
};

/*class UParitlcePool
{
public:
	UParitlcePool()  = default;
	~UParitlcePool() = default;

public:
	void Initailize(int DefaultCapacity)
	{
		ParticlePool.Reserve(DefaultCapacity);
		for (int i = 0; i < ParticlePool.Num(); ++i)
		{

			ParticlePool.Push(new UBall());
		}
	}

	void SpawnParticles(int NumParticles)
	{

	}
	void UpdateParticles()
	{
		for (size_t i = 0; i < ParticlePool.Num(); ++i)
		{

		}
	}

private:
	TQueue<UPariticle*> ParticlePool;

};*/


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static void ResolveCollision(UBall* a, UBall* b);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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

	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

	UScoreManager ScoreManager;

    TArray<UPrimitive*> PrimitiveList;
    
	// Input 포함 Drop Position 관련 기능은 지윤님 코드에 구현된 로직으로 처리 예정(차후 코드 병합 과정에서 처리)
    PrimitiveList.PushBack(CreateFruit(EFruitType::FT_Apple));


	bool bIsExit = false;

	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);

		MSG msg;

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

		renderer.Prepare();
		renderer.PrepareShader();

		ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
		ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
		ImGui::NewFrame(); // ImGui Core 초기화

		ImGui::Begin("Jungle Property Window");
		ImGui::Text("Hello, jungle world!");

		static const char* FruitNames[] = {
			"Cherry", "Strawberry", "Grape", "Dekopon", "Orange",
			"Apple", "Pear", "Peach", "Pineapple", "Melon", "Watermelon"
		};
		static int SelectedFruitType = 0;
		static float SpawnX = 0.0f;

		ImGui::Separator();
		ImGui::Text("Add Ball");
		ImGui::Combo("Fruit Type", &SelectedFruitType, FruitNames, sizeof(FruitNames) / sizeof(*FruitNames));
		ImGui::SliderFloat("Spawn X", &SpawnX, -0.9f, 0.9f);
		if (ImGui::Button("Add Ball"))
		{
			EFruitType ft = (EFruitType)SelectedFruitType;
			PrimitiveList.PushBack(CreateFruit(ft));
		}
		ImGui::Separator();

        // 병합 예정 정보를 담은 큐
		TQueue<FPendingMerge> PendingMergeQueue;



		for (UINT i = 0; i < PrimitiveList.Num(); i++)
		{
			PrimitiveList[i]->UpdatePhysics(fixedDeltaTime);
		}

        /**************************
         * 과일들 간 충돌 / 병합 연산 *
         **************************/
        for (UINT i = 0; i < PrimitiveList.Num(); i++)
        {
            UFruit* A = (UFruit*) PrimitiveList[i];

			if (A->IsPendingDestroy())
            {
                continue;
            }

            for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
            {
                UFruit* B = (UFruit*) PrimitiveList[j];

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
                            PendingMergeQueue.Push({ 
                                (EFruitType) (A->GetFruitType() + 1), 
                                (A->Location + B->Location) * 0.5f
							});
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

            // 새 과일 생성
            PrimitiveList.PushBack(CreateFruitAt(MergeInfo.ResultFruitType, MergeInfo.NewLocation));
		}

		//새 과일이 생성된 후 마찰 적용
		const static int overlapIteration = 10;
		for (UINT i = 0; i < overlapIteration; ++i)
		{
			for (UINT i = 0; i < PrimitiveList.Num(); i++)
			{
				UFruit* A = static_cast<UFruit*>(PrimitiveList[i]);

				for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
				{
					UFruit* B = static_cast<UFruit*>(PrimitiveList[j]);

					ResolveCollision(A, B);
				}
			}
		}

		for (UINT i = 0; i < PrimitiveList.Num(); i++)
		{
			PrimitiveList[i]->Render(renderer);
		}

        ImGui::End();

        ImGui::Render(); // 이번 프레임에 그릴 UI 설정
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출

		renderer.SwapBuffer();

		// 모니터 주사율이 144Hz일 수도 있으니 VSync를 사용하더라도 60fps 고정 코드는 유지
		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);

			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
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
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) // 메시지가 자기 입력으로 처리되지 않았으면 false 반환
	{
		return true;
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
	float InverseDist = 1.0f / Dist;
	Normal = FVector(Delta.x * InverseDist, Delta.y * InverseDist, 0.0f);


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

	// 정지 접촉시에 각속도가 증폭됨.
	// 3개 이상 맞닿았을 때 각속도로 증폭되는 문제 방지

	const float tangThreshold = 2.f;
	float jT = 0.0f;
	if (fabsf(RelVelTang) >= tangThreshold)
	{
		jT = -RelVelTang / (1 / MA + 1 / MB + A->Radius * A->Radius * InvIA + B->Radius * B->Radius * InvIB);

		//마찰력 적용
		const float mu = 0.4f;
		float maxFriction = mu * fabsf(jN);
		if (fabsf(jT) > maxFriction)
			jT = (jT > 0.f ? 1.f : -1.f) * maxFriction;
	}

	A->Velocity = A->Velocity - (Normal * (jN * 1 / MA)) - (Tangent * (jT * 1 / MA));
	B->Velocity = B->Velocity + (Normal * (jN * 1 / MB)) + (Tangent * (jT * 1 / MB));

	A->AngularVelocity -= A->Radius * jT * InvIA;
	B->AngularVelocity += B->Radius * jT * InvIB;
}
