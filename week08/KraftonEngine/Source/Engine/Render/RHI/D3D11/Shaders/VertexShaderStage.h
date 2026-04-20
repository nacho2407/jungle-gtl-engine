#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
class FVertexShaderStage
{
public:
    FVertexShaderStage() = default;
    ~FVertexShaderStage() { Release(); }
    FVertexShaderStage(const FVertexShaderStage&) = delete;
    FVertexShaderStage& operator=(const FVertexShaderStage&) = delete;
    FVertexShaderStage(FVertexShaderStage&& Other) noexcept;
    FVertexShaderStage& operator=(FVertexShaderStage&& Other) noexcept;
    void Set(ID3D11VertexShader* InShader, size_t InByteSize);
    void Release();
    ID3D11VertexShader* Get() const { return Shader; }
    size_t GetByteSize() const { return ByteSize; }

private:
    ID3D11VertexShader* Shader = nullptr;
    size_t ByteSize = 0;
};
