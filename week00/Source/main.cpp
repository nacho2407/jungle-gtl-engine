#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "../ThirdParty/imgui/imgui.h"
#include "../ThirdParty/imgui/imgui_internal.h"
#include "../ThirdParty/imgui/imgui_impl_dx11.h"
#include "../ThirdParty/imgui/imgui_impl_win32.h"


struct FVertexSimple
{
    float x, y, z;
    float r, g, b, a;
};

struct FVector
{
    float x, y, z;
    FVector(float _x = 0, float _y = 0, float _z = 0): x(_x), y(_y), z(_z) {}

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
        float Radius;
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
            (float) swapchaindesc.BufferDesc.Width,
            (float) swapchaindesc.BufferDesc.Height, 
            0.0f, 
            1.0f
        };
    }

    void CreateFrameBuffer()
    {
        // 스왑 체인으로부터 백 버퍼 텍스처 가져오기
        // 특정 물리 메모리를 계속 가리키는 것이 아니며, 스왑 체인 내 버퍼는 DXGI가 내부적으로 버퍼 회전을 하면서도 앱이 계속 같은 RTV로 현재의 백 버퍼를 그릴 수 있게 추상화되어 있음
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &FrameBuffer);

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

    void UpdateConstant(FVector Offset, float Radius)
    {
        if (ConstantBuffer)
        {
            D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU에 있는 상수 버퍼를 CPU가 접근할 수 있도록 매핑

            DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
            FConstants* constants = (FConstants*)constantbufferMSR.pData;

            constants->Offset = Offset;
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

    virtual void UpdatePhysics(
        float dt, 
        bool bGravity, 
        bool bAntigravity, 
        bool bAirDrag,
        float SlowMotionCoef
    ) = 0;
	virtual void Render(URenderer& renderer) = 0;
};

static UINT test = 0;
class UBall: public UPrimitive
{
public:
    FVector Location; // 여기서는 Offset과 동일한 개념
    FVector Velocity;
    float Radius;
    float Mass;
    
    static int TotalNumBalls;

public:
    UBall(): 
        Location(0.0f, 0.0f, 0.0f), 
        Velocity(0.0f, 0.0f, 0.0f), 
        Radius(0.075f), 
        Mass(0.0f)
    {
        //Radius = RandRange(0.1f, 0.2f);
        Radius = 0.05f * pow(1.25f, test);
        test++;

        Mass = Radius * MassFactor;

        LeftBorder = -1.0f + Radius;
        RightBorder = 1.0f - Radius;
        TopBorder = 1.0f - Radius;
        BottomBorder = -1.0f + Radius;

        Location.x = RandRange(LeftBorder, RightBorder);
        Location.y = RandRange(BottomBorder, TopBorder);
        Location.z = 0.0f;

        Velocity.x = RandRange(MinSpeed, MaxSpeed);
        Velocity.y = RandRange(MinSpeed, MaxSpeed);
        Velocity.z = 0.0f;

        TotalNumBalls++;
	}

    ~UBall()
    {
		TotalNumBalls--;
    }

    void UpdatePhysics(
        float dt,
        bool bGravity,
        bool bAntigravity,
        bool bAirDrag,
        float SlowMotionCoef
    ) override
    {
        dt *= SlowMotionCoef;

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
            Velocity.y *= bAntigravity ? -RestitutionWall: -1.0f;
        }
        if (Location.y < BottomBorder)
        {
			Location.y = BottomBorder;
            Velocity.y *= bGravity ? -RestitutionWall : -1.0f;
        }

        if (bGravity)
        {
            float Accel = bAntigravity ? -GravityAccel : GravityAccel;

            Velocity.y -= Accel * dt;
        }

        if (bAirDrag)
        {
            Velocity = Velocity * (1.0f - AirDragCoef / Mass * dt);
        }
	}

    void Render(URenderer& renderer) override
    {
        renderer.UpdateConstant(Location, Radius);
        renderer.RenderSphere();
	}

private:
	float MassFactor = 1.0f; // 질량 보정용

	float LeftBorder = -1.0f;
	float RightBorder = 1.0f;
	float TopBorder = 1.0f;
	float BottomBorder = -1.0f;

    const float MinSpeed = -2.0f;
	const float MaxSpeed = 2.0f;

    const float GravityAccel = 4.0f; // 중력 가속도
    const float RestitutionWall = 0.8f; // 중력 활성화 시 적용할 벽 반발 감쇠 계수(1.0 == 완전 반사)
    const float AirDragCoef = 0.2f; // 공기 저항 활성화 시 적용할 공기 저항 계수

private:
    float Random()
    {
        return (float) rand() / (float) RAND_MAX; // [0, 1]
    }

    float RandRange(float a, float b)
    {
        return a + (b - a) * Random();
    }
};


int UBall::TotalNumBalls = 0;

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
	const float fixedDeltaTime = (float) targetFrameTime / 1000; // 60fps 고정 상황 가정

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER startTime, endTime;
    double elapsedTime = 0.0;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
    ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

    UINT PrimitiveListCapacity = 64;
	UINT PrimitiveListSize = UBall::TotalNumBalls; // 컨테이너 크기 자체는 컨테이너가 직접 파악
    UPrimitive** PrimitiveList = new UPrimitive*[PrimitiveListCapacity];

	PrimitiveList[PrimitiveListSize++] = new UBall();

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

        static int TargetCount = 1;
        static bool bGravity = false;
        static bool bAntigravity = false;
        static bool bAirDrag = false;
        static float SlowMotionCoef = 1.0f;

        // 이동 및 벽 충돌 계산
		for (UINT i = 0; i < PrimitiveListSize; i++)
        {
            PrimitiveList[i]->UpdatePhysics(
                fixedDeltaTime, 
                bGravity, 
                bAntigravity, 
                bAirDrag,
                SlowMotionCoef
            );
			PrimitiveList[i]->Render(renderer);
        }

        // 공들 간 충돌 계산
        for (UINT i = 0; i < PrimitiveListSize; i++)
        {
            UBall* A = (UBall*) PrimitiveList[i];

            for (UINT j = i + 1; j < PrimitiveListSize; j++)
            {
                UBall* B = (UBall*) PrimitiveList[j];

                ResolveCollision(A, B);
            }
        }

        ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
        ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
        ImGui::NewFrame(); // ImGui Core 초기화

        ImGui::Begin("Jungle Property Window");

        ImGui::Text("Hello, jungle world!");

        ImGui::InputInt("Number of balls", &TargetCount);

        if (TargetCount < 1)
        {
            TargetCount = 1;
        }

        while (PrimitiveListSize < (UINT) TargetCount)
        {
            if (PrimitiveListSize == PrimitiveListCapacity)
            {
                UINT NewCapacity = PrimitiveListCapacity * 2;

                UPrimitive** NewList = new UPrimitive * [NewCapacity];

                for (UINT i = 0; i < PrimitiveListSize; i++)
                {
                    NewList[i] = PrimitiveList[i];
                }

                delete[] PrimitiveList;

                PrimitiveList = NewList;
                PrimitiveListCapacity = NewCapacity;
            }

            PrimitiveList[PrimitiveListSize++] = new UBall();
        }

        while (PrimitiveListSize > (UINT) TargetCount)
        {
            UINT RandomIndex = (UINT) (rand() % PrimitiveListSize);
            delete PrimitiveList[RandomIndex];

            PrimitiveList[RandomIndex] = PrimitiveList[PrimitiveListSize - 1]; // 순서가 중요하지 않으므로...
            PrimitiveListSize--;
        }

        ImGui::Checkbox("Gravity", &bGravity);

        if (!bGravity)
        {
            bAntigravity = false;
        }

        ImGui::BeginDisabled(!bGravity);
        ImGui::Checkbox("Antigravity", &bAntigravity);
        ImGui::EndDisabled();

        ImGui::Checkbox("Air Drag", &bAirDrag);

        ImGui::SliderFloat("Slow Motion", &SlowMotionCoef, 0.1f, 1.0f);

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

    for (UINT i = 0; i < PrimitiveListSize; i++) {
        delete PrimitiveList[i];
		PrimitiveList[i] = nullptr;
    }

	delete[] PrimitiveList;

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

    FVector RelVel =  B->Velocity - A->Velocity;
    float RelVelNorm = RelVel.Dot2(Normal);

    if (RelVelNorm > 0.0f)
    {
        return;
    }

    const float e = 1.0f; // 탄성계수(완전 탄성)
    float j = -(1.0f + e) * RelVelNorm / (1.0f / MA + 1.0f / MB); // 충격량

    A->Velocity = A->Velocity - (Normal * (j / MA));
    B->Velocity = B->Velocity + (Normal * (j / MB));
}
