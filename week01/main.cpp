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

struct InputState
{
    float MouseX;
    float MouseY;

    bool MouseLeftDown;
    bool MouseLeftClicked;
};
InputState gInput;

struct SceneFrame
{
    ImVec2 Pos;
    ImVec2 Size;
};

#include "Sphere.h"
#include <windowsx.h>

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
        float Radius;
    };

    struct FSphere
    {
		ID3D11Buffer* VertexBuffer = nullptr;
		UINT NumVertices;
    } sphere;

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

    ID3D11Texture2D* SceneTexture = nullptr;
    ID3D11RenderTargetView* SceneRTV = nullptr;
    ID3D11ShaderResourceView* SceneSRV = nullptr;

    // 화면 초기화 색상
    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

public:
    void Create(HWND hWindow)
    {
        CreateDeviceAndSwapChain(hWindow);

        CreateSceneRenderTarget(
            (UINT)ViewportInfo.Width,
            (UINT)ViewportInfo.Height
        );

        CreateFrameBuffer();

        CreateRasterizerState();

        CreateSphereMesh();

        CreateLine();
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

        Device->CreateTexture2D(&desc, nullptr, &SceneTexture);

        Device->CreateRenderTargetView(SceneTexture, nullptr, &SceneRTV);

        Device->CreateShaderResourceView(SceneTexture, nullptr, &SceneSRV);
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

    void PrepareScene()
    {
        DeviceContext->ClearRenderTargetView(SceneRTV, ClearColor);

        DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        DeviceContext->RSSetViewports(1, &ViewportInfo);
        DeviceContext->RSSetState(RasterizerState);

        DeviceContext->OMSetRenderTargets(1, &SceneRTV, nullptr);
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

    void RenderLine()
    {
        UINT offset = 0;

        DeviceContext->IASetVertexBuffers(
            0,
            1,
            &LineVertexBuffer,
            &Stride,
            &offset
        );

        DeviceContext->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        );

        DeviceContext->Draw(2, 0);
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
        Radius(0.1f), 
        Mass(0.0f)
    {
        Radius = RandRange(0.1f, 0.2f);

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

    bool CheckLineCollision(float lineY)
    {
        float distance = fabs(Location.y - lineY);
        return distance <= Radius;
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

    HWND hWnd = CreateWindowExW(
        0,
        WindowClass,
        Title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        1100, // 윈도우 사이즈 고정
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

    float sceneWidth = 850;
    float sceneHeight = 850;
    SceneFrame gameScene;

    UINT PrimitiveListCapacity = 64;
	UINT PrimitiveListSize = UBall::TotalNumBalls; // 컨테이너 크기 자체는 컨테이너가 직접 파악
    UPrimitive** PrimitiveList = new UPrimitive*[PrimitiveListCapacity];

	PrimitiveList[PrimitiveListSize++] = new UBall();

    const float LineY = 0.8f;

	bool bIsExit = false;

	// 게임 오버 상태와 이전 프레임의 선 충돌 여부
    bool gameOver = false;
    bool prevCollision = false;

    LARGE_INTEGER collisionStartCounter;

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
        bool mouseInScene =
            gInput.MouseX >= gameScene.Pos.x &&
            gInput.MouseX <= gameScene.Pos.x + gameScene.Size.x &&
            gInput.MouseY >= gameScene.Pos.y &&
            gInput.MouseY <= gameScene.Pos.y + gameScene.Size.y;

        bool spawnBall = false;
        float spawnX = 0;

        if (mouseInScene)
        {
            float localX = gInput.MouseX - gameScene.Pos.x;
            spawnX = (localX / gameScene.Size.x) * 2.0f - 1.0f;

            if (gInput.MouseLeftClicked)
            {
                spawnBall = true;
            }
        }

        renderer.PrepareScene();
        renderer.PrepareShader();

        static int TargetCount = 1;
        static bool bGravity = false;
        static bool bAntigravity = false;
        static bool bAirDrag = false;
        static float SlowMotionCoef = 1.0f;
        bool bLineCollision = false;

        if (!gameOver)
        {
            // ----- logic -----

            // 공 생성
            if (spawnBall)
            {
                UBall* newBall = new UBall();
                newBall->Location = FVector(spawnX, 0.9f, 0);

                PrimitiveList[PrimitiveListSize++] = newBall;
            }

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
                UBall* A = (UBall*)PrimitiveList[i];

                for (UINT j = i + 1; j < PrimitiveListSize; j++)
                {
                    UBall* B = (UBall*)PrimitiveList[j];

                    ResolveCollision(A, B);
                }
            }

            // 공과 선의 충돌 계산
            for (UINT i = 0; i < PrimitiveListSize; i++)
            {
                UBall* ball = (UBall*)PrimitiveList[i];

                if (ball->CheckLineCollision(LineY))
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

                if (elapsed >= 1.0)
                {
                    gameOver = true;
                }
            }

            prevCollision = bLineCollision;
        }
        else
        {
            // 기존 데이터 초기화
            for (UINT i = 0; i < PrimitiveListSize; i++)
            {
                delete PrimitiveList[i];
            }

            PrimitiveListSize = 0;
        }
        // ----- render -----



        ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
        ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
        ImGui::NewFrame(); // ImGui Core 초기화

        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin(
            "Jungle Scene Window",
            nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar
        );

        ImGui::Text("GameOver: %d", gameOver);
        ImGui::Text("Score:");

        ImGui::Spacing();
        ImGui::Spacing();

        if (!gameOver)
        {
            renderer.UpdateConstant(FVector(0, 0, 0), 1.0f);
            renderer.UpdateLineColor(bLineCollision);
            renderer.RenderLine();

            if (mouseInScene)
            {
                renderer.UpdateConstant(FVector(spawnX, 0.9f, 0), 0.1f);
                renderer.RenderSphere();
            }
            // 가운데 정렬
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float posX = (avail.x - sceneWidth) * 0.5f;
            float posY = (avail.y - sceneHeight) * 0.9f;

            if (posX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + posX);
            if (posY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + posY);

            // 내부 프레임
            ImGui::BeginChild(
                "SceneFrame",
                ImVec2(sceneWidth, sceneHeight),   // 프레임 크기
                false,               // border 표시
                ImGuiWindowFlags_NoScrollbar
            );

            gameScene.Pos = ImGui::GetCursorScreenPos();
            gameScene.Size = ImVec2(sceneWidth, sceneHeight);

            ImGui::Image(
                (ImTextureID)renderer.SceneSRV,
                ImVec2(sceneWidth, sceneHeight)
            );
            ImGui::EndChild();
        }
        else
        {
            const char* text = "Game Over";
            ImVec2 textSize = ImGui::CalcTextSize(text);

            float centerX = (io.DisplaySize.x - textSize.x) * 0.5f;
            float centerY = (io.DisplaySize.y - textSize.y) * 0.5f;

            ImGui::SetCursorPos(ImVec2(centerX, centerY));
            ImGui::Text("%s", text);

            ImGui::Spacing();

            const char* buttonText = "Restart";
            ImVec2 buttonSize = ImGui::CalcTextSize(buttonText);

            ImGui::SetCursorPosX((io.DisplaySize.x - buttonSize.x - 20) * 0.5f);

            if (ImGui::Button(buttonText))
            {
                gameOver = false;
                PrimitiveList[PrimitiveListSize++] = new UBall();
            }
        }
        ImGui::End();

        renderer.Prepare();

        ImGui::Render(); // 이번 프레임에 그릴 UI 설정
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출

        renderer.SwapBuffer();

        gInput.MouseLeftClicked = false;
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
    case WM_MOUSEMOVE:
        gInput.MouseX = (float)GET_X_LPARAM(lParam);
        gInput.MouseY = (float)GET_Y_LPARAM(lParam);
        break;

    case WM_LBUTTONDOWN:
        gInput.MouseLeftDown = true;
        gInput.MouseLeftClicked = true;
        break;

    case WM_LBUTTONUP:
        gInput.MouseLeftDown = false;
        break;

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
