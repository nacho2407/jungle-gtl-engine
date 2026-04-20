#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
class FPixelShaderStage
{
public:
    FPixelShaderStage() = default;
    ~FPixelShaderStage() { Release(); }
    FPixelShaderStage(const FPixelShaderStage&) = delete;
    FPixelShaderStage& operator=(const FPixelShaderStage&) = delete;
    FPixelShaderStage(FPixelShaderStage&& Other) noexcept;
    FPixelShaderStage& operator=(FPixelShaderStage&& Other) noexcept;
    void Set(ID3D11PixelShader* InShader, size_t InByteSize);
    void Release();
    ID3D11PixelShader* Get() const { return Shader; }
    size_t GetByteSize() const { return ByteSize; }

private:
    ID3D11PixelShader* Shader = nullptr;
    size_t ByteSize = 0;
};
