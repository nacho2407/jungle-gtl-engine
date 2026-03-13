#include <Windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "FVector.h"
#include "FVertexSimple.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "Sphere.h"

static constexpr float PI{3.1415927f};
static constexpr float G{-0.98f};
static constexpr float leftBorder{-1.0f};
static constexpr float rightBorder{1.0f};
static constexpr float topBorder{1.0f};
static constexpr float bottomBorder{-1.0f};

float GetRandomFloat(float min, float max) {
  float random{(float)rand() / (float)RAND_MAX};
  return min + random * (max - min);
}

struct FMatrix4x4 {
  float m[4][4];

  FMatrix4x4() {
    for (int i{0}; i < 4; i++)
      for (int j{0}; j < 4; j++) m[i][j] = (i == j) ? 1.0f : 0.0f;
  }

  FMatrix4x4 operator*(const FMatrix4x4& other) const {
    FMatrix4x4 result;
    for (int i{0}; i < 4; i++) {
      for (int j{0}; j < 4; j++) {
        result.m[i][j] = m[i][0] * other.m[0][j] + m[i][1] * other.m[1][j] +
                         m[i][2] * other.m[2][j] + m[i][3] * other.m[3][j];
      }
    }
    return result;
  }
};

class URenderer {
 public:
  ID3D11Device* Device = nullptr;
  ID3D11DeviceContext* DeviceContext = nullptr;
  IDXGISwapChain* SwapChain = nullptr;

  ID3D11Texture2D* FrameBuffer = nullptr;
  ID3D11RenderTargetView* FrameBufferRTV = nullptr;
  ID3D11RasterizerState* RasterizerState = nullptr;

  FLOAT ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};
  D3D11_VIEWPORT ViewportInfo;

 public:
  void Create(HWND hWindow) {
    CreateDeviceAndSwapChain(hWindow);

    CreateFrameBuffer();

    CreateRasterizerState();
  }

  void CreateDeviceAndSwapChain(HWND hWindow) {
    D3D_FEATURE_LEVEL featurelevels[] = {D3D_FEATURE_LEVEL_11_0};

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
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

    SwapChain->GetDesc(&swapchaindesc);

    ViewportInfo = {0.0f,
                    0.0f,
                    (float)swapchaindesc.BufferDesc.Width,
                    (float)swapchaindesc.BufferDesc.Height,
                    0.0f,
                    1.0f};
  }

  void ReleaseDeviceAndSwapChain() {
    if (DeviceContext) {
      DeviceContext->Flush();
    }
    if (SwapChain) {
      SwapChain->Release();
      SwapChain = nullptr;
    }
    if (Device) {
      Device->Release();
      Device = nullptr;
    }
    if (DeviceContext) {
      DeviceContext->Release();
      DeviceContext = nullptr;
    }
  }

  void CreateFrameBuffer() {
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc,
                                   &FrameBufferRTV);
  }

  void ReleaseFrameBuffer() {
    if (FrameBuffer) {
      FrameBuffer->Release();
      FrameBuffer = nullptr;
    }
    if (FrameBufferRTV) {
      FrameBufferRTV->Release();
      FrameBufferRTV = nullptr;
    }
  }

  void CreateRasterizerState() {
    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID;
    rasterizerdesc.CullMode = D3D11_CULL_BACK;

    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
  }

  void ReleaseRasterizerState() {
    if (RasterizerState) {
      RasterizerState->Release();
      RasterizerState = nullptr;
    }
  }

  void Release() {
    RasterizerState->Release();
    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();
  }

  void SwapBuffer() { SwapChain->Present(1, 0); }

 public:
  ID3D11VertexShader* SimpleVertexShader;
  ID3D11PixelShader* SimplePixelShader;
  ID3D11InputLayout* SimpleInputLayout;
  unsigned int Stride;

  void CreateShader() {
    ID3DBlob* vertexshaderCSO;
    ID3DBlob* pixelshaderCSO;

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0",
                       0, 0, &vertexshaderCSO, nullptr);

    Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
                               vertexshaderCSO->GetBufferSize(), nullptr,
                               &SimpleVertexShader);

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0",
                       0, 0, &pixelshaderCSO, nullptr);

    Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
                              pixelshaderCSO->GetBufferSize(), nullptr,
                              &SimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Device->CreateInputLayout(
        layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(),
        vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

    Stride = sizeof(FVertexSimple);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
  }

  void ReleaseShader() {
    if (SimpleInputLayout) {
      SimpleInputLayout->Release();
      SimpleInputLayout = nullptr;
    }
    if (SimplePixelShader) {
      SimplePixelShader->Release();
      SimplePixelShader = nullptr;
    }
    if (SimpleVertexShader) {
      SimpleVertexShader->Release();
      SimpleVertexShader = nullptr;
    }
  }

  void Prepare() {
    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    DeviceContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
    DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
  }
  void PrepareShader() {
    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
    DeviceContext->IASetInputLayout(SimpleInputLayout);

    if (ConstantBuffer) {
      DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
  }

  void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices) {
    UINT offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
    DeviceContext->Draw(numVertices, 0);
  }

 public:
  ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth) {
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};
    ID3D11Buffer* vertexBuffer;
    Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
  }

  void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer) {
    vertexBuffer->Release();
  }

 public:
  struct FConstantBufferData {
    FMatrix4x4 World;
  };
  ID3D11Buffer* ConstantBuffer = nullptr;

  void CreateConstantBuffer() {
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth =
        sizeof(FConstantBufferData) + 0xf & 0xfffffff0;
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
  }

  void ReleaseConstantBuffer() {
    if (ConstantBuffer) {
      ConstantBuffer->Release();
      ConstantBuffer = nullptr;
    }
  }

  void UpdateConstant(const FMatrix4x4& World) {
    if (ConstantBuffer) {
      D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

      HRESULT hr = DeviceContext->Map(
          ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);

      if (SUCCEEDED(hr)) {
        memcpy(constantbufferMSR.pData, &World, sizeof(FMatrix4x4));
        DeviceContext->Unmap(ConstantBuffer, 0);
      }
    }
  }
};

class UPrimitive {
 public:
  virtual ~UPrimitive() {}
  virtual void Update(float DeltaTime, bool GravityEnable, bool SpinEnable) = 0;
  virtual FMatrix4x4 GetWorldMatrix() const = 0;
  virtual bool CheckCollision(const UPrimitive* Other) = 0;
};

class UBall : public UPrimitive {
 public:
  // 클래스 이름과, 아래 다섯개의 변수 이름은 변경하지 않습니다.
  FVector Location;
  FVector Velocity;
  float Radius;
  float Mass;
  inline static int TotalNumBalls{0};
  inline static ID3D11Buffer* SphereVertexBuffer{nullptr};

  // 각속도 추가
  float AngularVelocity;
  float Rotation;

  UBall() {
    Radius = GetRandomFloat(0.02f, 0.15f);
    Mass = (4.0f / 3.0f) * PI * powf(Radius, 3);
    TotalNumBalls++;

    Rotation = 0.0f;
    AngularVelocity = GetRandomFloat(-5.0f, 5.0f);
  }

  ~UBall() { TotalNumBalls--; }

  float GetRadius() const { return Radius; }
  const FVector& GetLocation() const { return Location; }
  static void Init_VertexBuffer(ID3D11Buffer* _SphereVertexBuffer) {
    SphereVertexBuffer = _SphereVertexBuffer;
  }

  virtual void Update(float DeltaTime, bool GravityEnable,
                      bool SpinEnable) override {
    if (GravityEnable) {
      Velocity.y += G * DeltaTime;

      if (Location.y - Radius < -1.0f) {
        Location.y = -1.0f + Radius;
        Velocity.y *= -0.8f;
        if (fabsf(Velocity.y) < 0.05f) {
          Velocity.y = 0;
        }
      }
    }
    Location.x += Velocity.x * DeltaTime;
    Location.y += Velocity.y * DeltaTime;

    if (Location.x + Radius > 1.0f) {
      Location.x = 1.0f - Radius;
      Velocity.x *= -1.0f;

    } else if (Location.x - Radius < -1.0f) {
      Location.x = -1.0f + Radius;
      Velocity.x *= -1.0f;
    }

    if (Location.y + Radius > 1.0f) {
      Location.y = 1.0f - Radius;
      Velocity.y *= -1.0f;

    } else if (Location.y - Radius < -1.0f) {
      Location.y = -1.0f + Radius;
      Velocity.y *= -1.0f;
    }

    if (SpinEnable) {
      Rotation += AngularVelocity * DeltaTime;
      if (Location.y - Radius < -1.0f) {
        AngularVelocity += (Velocity.x - (AngularVelocity * Radius)) * 0.1f;

      } else if (Location.y + Radius > 1.0f) {
        AngularVelocity += (Velocity.x + (AngularVelocity * Radius)) * 0.1f;

      } else if (Location.x - Radius < -1.0f) {
        AngularVelocity += (-(Velocity.y) - (AngularVelocity * Radius)) * 0.1f;

      } else if (Location.x + Radius > 1.0f) {
        AngularVelocity += (Velocity.y - (AngularVelocity * Radius)) * 0.1f;
      }
    }
  }

  virtual FMatrix4x4 GetWorldMatrix() const override {
    FMatrix4x4 mScale;
    mScale.m[0][0] = Radius;
    mScale.m[1][1] = Radius;
    mScale.m[2][2] = Radius;

    FMatrix4x4 mRotation;
    float CosCalc{cosf(Rotation)};
    float SinCalc{sinf(Rotation)};
    mRotation.m[0][0] = CosCalc;
    mRotation.m[0][1] = SinCalc;
    mRotation.m[1][0] = -SinCalc;
    mRotation.m[1][1] = CosCalc;

    FMatrix4x4 mTranslation;
    mTranslation.m[3][0] = Location.x;
    mTranslation.m[3][1] = Location.y;
    mTranslation.m[3][2] = Location.z;

    return mScale * mRotation * mTranslation;
  }

  virtual bool CheckCollision(const UPrimitive* Other) override {
    const UBall* OtherBall{dynamic_cast<const UBall*>(Other)};
    if (OtherBall) {
      float dx{Location.x - OtherBall->Location.x};
      float dy{Location.y - OtherBall->Location.y};
      float distanceSquared{dx * dx + dy * dy};
      float radiusSum{Radius + OtherBall->Radius};
      return distanceSquared <= (radiusSum * radiusSum);
    }
    return false;
  }

  void ResolveCollision(UBall* Other) {
    float dx{Other->Location.x - Location.x};
    float dy{Other->Location.y - Location.y};
    float Distance{sqrtf(dx * dx + dy * dy)};

    if (Distance == 0.0f) return;

    float nx{dx / Distance};
    float ny{dy / Distance};

    float rvx{Other->Velocity.x - Velocity.x};
    float rvy{Other->Velocity.y - Velocity.y};

    float VelocityAlongNormal{rvx * nx + rvy * ny};

    if (VelocityAlongNormal > 0) return;

    float e{1.0f};

    float j{-(1.0f + e) * VelocityAlongNormal};
    j /= (1.0f / Mass + 1.0f / Other->Mass);

    float ImpulseX{j * nx};
    float ImpulseY{j * ny};

    Velocity.x -= (1.0f / Mass) * ImpulseX;
    Velocity.y -= (1.0f / Mass) * ImpulseY;
    Other->Velocity.x += (1.0f / Other->Mass) * ImpulseX;
    Other->Velocity.y += (1.0f / Other->Mass) * ImpulseY;

    float Overlap{(Radius + Other->Radius) - Distance};
    if (Overlap > 0) {
      float SeparationX{(Overlap / 2.0f) * nx};
      float SeparationY{(Overlap / 2.0f) * ny};
      Location.x -= SeparationX;
      Location.y -= SeparationY;
      Other->Location.x += SeparationX;
      Other->Location.y += SeparationY;
    }
  }
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
    return true;
  }

  switch (message) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR IpCmdLine, int nShowCmd) {
  WCHAR WindowClass[] = L"JungleWindowClass";
  WCHAR Title[] = L"Game Tech Lab";
  WNDCLASSW wndclass = {0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass};

  RegisterClassW(&wndclass);

  HWND hwnd = CreateWindowExW(0, WindowClass, Title,
                              WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024, nullptr,
                              nullptr, hInstance, nullptr);
  URenderer renderer;
  renderer.Create(hwnd);
  renderer.CreateShader();
  renderer.CreateConstantBuffer();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  ImGui_ImplWin32_Init((void*)hwnd);
  ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

  FVertexSimple* vertices = sphere_vertices;
  UINT ByteWidth = sizeof(sphere_vertices);
  UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

  ID3D11Buffer* vertexBufferSphere =
      renderer.CreateVertexBuffer(sphere_vertices, ByteWidth);
  UBall::Init_VertexBuffer(vertexBufferSphere);

  int UBallArraySize{100};
  UPrimitive** PrimitiveList{new UPrimitive*[UBallArraySize]};

  const int targetFPS{60};
  const double targetFrameTime{1000.0 / targetFPS};

  LARGE_INTEGER frequency;
  LARGE_INTEGER startTime, endTime;
  double elapsedTime{0.0};
  QueryPerformanceFrequency(&frequency);

  bool bShowSpawnError{false};
  float ErrorDisplayTimer{0.0f};
  int ImGuiBallCount{1};
  bool SpinEnable{false};
  bool GravityEnable{false};

  bool bIsExit{false};
  while (bIsExit == false) {
    MSG msg;
    QueryPerformanceCounter(&startTime);

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      if (msg.message == WM_QUIT) {
        bIsExit = true;
        break;
      }
    }

    if (ImGuiBallCount > UBall::TotalNumBalls &&
        UBall::TotalNumBalls < UBallArraySize) {
      while (UBall::TotalNumBalls < ImGuiBallCount) {
        int targetIdx{UBall::TotalNumBalls};
        UBall* TempBall{new UBall()};
        bool bIsOverlapped{false};
        int maxAttempts{50};
        int attempts{0};

        do {
          bIsOverlapped = false;
          float rx{GetRandomFloat(-1.0f + TempBall->Radius,
                                  1.0f - TempBall->Radius)};
          float ry{GetRandomFloat(-1.0f + TempBall->Radius,
                                  1.0f - TempBall->Radius)};
          TempBall->Location = FVector(rx, ry, 0.0f);

          for (int i = 0; i < UBall::TotalNumBalls - 1; ++i) {
            if (PrimitiveList[i] &&
                TempBall->CheckCollision(PrimitiveList[i])) {
              bIsOverlapped = true;
              break;
            }
          }
          attempts++;

        } while (bIsOverlapped && attempts < maxAttempts);
        if (attempts >= maxAttempts) {
          delete TempBall;
          bShowSpawnError = true;
          ErrorDisplayTimer = 3.0f;
          ImGuiBallCount = UBall::TotalNumBalls;
          break;

        } else {
          PrimitiveList[targetIdx] = TempBall;
          TempBall->Velocity.x = GetRandomFloat(-0.3f, 0.3f);
          TempBall->Velocity.y = GetRandomFloat(-0.3f, 0.3f);
        }
      }

    } else if (ImGuiBallCount < UBall::TotalNumBalls &&
               UBall::TotalNumBalls > 0) {
      while (UBall::TotalNumBalls > ImGuiBallCount) {
        int deleteIdx = rand() % UBall::TotalNumBalls;
        delete PrimitiveList[deleteIdx];
        for (int i{deleteIdx}; i < UBall::TotalNumBalls; ++i) {
          PrimitiveList[i] = PrimitiveList[i + 1];
        }
        PrimitiveList[UBall::TotalNumBalls] = nullptr;
      }
    }

    renderer.Prepare();
    renderer.PrepareShader();

    float deltaTime{static_cast<float>(targetFrameTime) / 1000.0f};
    for (int i{0}; i < UBall::TotalNumBalls; ++i) {
      for (int j{i + 1}; j < UBall::TotalNumBalls; ++j) {
        UBall* ballA{static_cast<UBall*>(PrimitiveList[i])};
        UBall* ballB{static_cast<UBall*>(PrimitiveList[j])};

        if (ballA->CheckCollision(ballB)) {
          ballA->ResolveCollision(ballB);
        }
      }
      PrimitiveList[i]->Update(deltaTime, GravityEnable, SpinEnable);
      renderer.UpdateConstant(PrimitiveList[i]->GetWorldMatrix());
      renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Jungle Property Window");
    ImGui::Text("Hello Jungle World!");
    ImGui::Checkbox("Gravity", &GravityEnable);
    ImGui::Checkbox("Spin", &SpinEnable);
    if (ImGui::InputInt("Number of Ball", &ImGuiBallCount)) {
      if (ImGuiBallCount < 1) ImGuiBallCount = 1;
      if (ImGuiBallCount > 100) ImGuiBallCount = 100;
    }
    if (bShowSpawnError) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Error: No space to spawn more balls!");
      ErrorDisplayTimer -= (float)targetFrameTime / 1000.0f;
      if (ErrorDisplayTimer <= 0) bShowSpawnError = false;
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    renderer.SwapBuffer();

    do {
      Sleep(0);
      QueryPerformanceCounter(&endTime);
      elapsedTime =
          (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

    } while (elapsedTime < targetFrameTime);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  renderer.ReleaseVertexBuffer(vertexBufferSphere);

  renderer.ReleaseConstantBuffer();
  renderer.ReleaseShader();
  renderer.Release();

  int finalCount = UBall::TotalNumBalls;
  for (int i{0}; i < finalCount; ++i) {
    delete PrimitiveList[i];
  }
  delete[] PrimitiveList;

  return 0;
}
