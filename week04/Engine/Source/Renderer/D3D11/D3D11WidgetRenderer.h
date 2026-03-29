#pragma once
#include "Core/CoreMinimal.h"
#include "Renderer/Types/ShaderConstants.h"
#include <d3d11.h>
#include "Renderer/D3D11/D3D11Common.h"

class FD3D11RHI;

class FD3D11WidgetRenderer
{
public:
  void Initialize(FD3D11RHI* InRHI);
  void Shutdown();

  void BeginFrame(uint32 ScreenWidth, uint32 ScreenHeight);
  void DrawWidget(ID3D11DeviceContext* Context, float X, float Y, float W, float H, FColor Color);

private:
  FD3D11RHI* RHI = nullptr;
  TComPtr<ID3D11Buffer> ConstantBuffer = nullptr;
  TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
  TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
  TComPtr<ID3D11VertexShader> VertexShader = nullptr;
  TComPtr<ID3D11PixelShader> PixelShader = nullptr;
  TComPtr<ID3D11InputLayout>  InputLayout = nullptr;

  TComPtr<ID3D11RasterizerState> RasterizerState = nullptr;
  TComPtr<ID3D11BlendState>      BlendState = nullptr;
  TComPtr<ID3D11DepthStencilState> DepthStencilState = nullptr;

  // Needs to be rebuilt at the start of each frame
  FMatrix OrthographicMatrix;
};