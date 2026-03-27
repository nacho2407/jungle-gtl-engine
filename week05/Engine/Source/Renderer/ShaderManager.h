#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>
#include <memory>

class FVertexShader;
class FPixelShader;

class ENGINE_API CShaderManager
{
public:
    CShaderManager() = default;
    ~CShaderManager();

    bool LoadVertexShader(ID3D11Device* Device, const wchar_t* FilePath);
    bool LoadPixelShader(ID3D11Device* Device, const wchar_t* FilePath);
    void Bind(ID3D11DeviceContext* DeviceContext);
    void Release();

private:
    std::shared_ptr<FVertexShader> VS;
    std::shared_ptr<FPixelShader> PS;
};
