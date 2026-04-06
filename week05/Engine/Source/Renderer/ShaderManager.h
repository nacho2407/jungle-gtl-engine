#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>
#include <memory>

class FVertexShader;
class FPixelShader;
class FComputeShader;

class ENGINE_API FShaderManager
{
public:
    FShaderManager() = default;
    ~FShaderManager();

    bool LoadVertexShader(ID3D11Device* Device, const wchar_t* FilePath);
    bool LoadPixelShader(ID3D11Device* Device, const wchar_t* FilePath);
    bool LoadComputeShader(ID3D11Device* Device, const wchar_t* FilePath, const char* EntryPoint = "main");
    void Bind(ID3D11DeviceContext* DeviceContext);
    void BindCompute(ID3D11DeviceContext* DeviceContext);
    void DispatchCompute(ID3D11DeviceContext* DeviceContext, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    void Release();

private:
    std::shared_ptr<FVertexShader> VS;
    std::shared_ptr<FPixelShader> PS;
    std::shared_ptr<FComputeShader> CS;
};
