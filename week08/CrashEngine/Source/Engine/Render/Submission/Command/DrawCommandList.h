#pragma once

#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "DrawCommand.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

// FDrawBindStateCache는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FDrawBindStateCache
{
    bool bForceAll = true;

    FGraphicsProgram*         Shader         = nullptr;
    EDepthStencilState        DepthStencil   = {};
    EBlendState               Blend          = {};
    ERasterizerState          Rasterizer     = {};
    D3D11_PRIMITIVE_TOPOLOGY  Topology       = {};
    uint8                     StencilRef     = 0;
    FMeshBuffer*              MeshBuffer     = nullptr;
    ID3D11Buffer*             RawVB          = nullptr;
    ID3D11Buffer*             RawIB          = nullptr;
    FConstantBuffer*          PerObjectCB    = nullptr;
    FConstantBuffer*          PerShaderCB[2] = {};
    FConstantBuffer*          LightCB        = nullptr;
    ID3D11ShaderResourceView* DiffuseSRV     = nullptr;
    ID3D11ShaderResourceView* NormalSRV      = nullptr;
    ID3D11ShaderResourceView* SpecularSRV    = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV  = nullptr;

    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11DepthStencilView* DSV = nullptr;

    void Reset();

    void Cleanup(ID3D11DeviceContext* Ctx);
};

// FDrawCommandList는 렌더 영역의 핵심 동작을 담당합니다.
class FDrawCommandList
{
public:
    FDrawCommand& AddCommand();
    void          Sort();
    void          GetPassRange(ERenderPass Pass, uint32& OutStart, uint32& OutEnd) const;
    void          Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void          SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void          SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FDrawBindStateCache& Cache);
    void          Reset();

    bool   IsEmpty() const { return Commands.empty(); }
    uint32 GetCommandCount() const { return static_cast<uint32>(Commands.size()); }
    uint32 GetCommandCount(ERenderPass Pass) const;

    TArray<FDrawCommand>&       GetCommands() { return Commands; }
    const TArray<FDrawCommand>& GetCommands() const { return Commands; }

private:
    void SubmitCommand(const FDrawCommand& Cmd, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FDrawBindStateCache& Cache);

    TArray<FDrawCommand> Commands;
    uint32               PassOffsets[(uint32)ERenderPass::MAX + 1] = {};
};
