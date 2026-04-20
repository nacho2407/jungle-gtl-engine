#pragma once

#include "DrawCommand.h"
#include "Render/D3D11/Device/D3DDevice.h"
#include "Render/D3D11/Buffers/Buffers.h"

/*
    FStateCache — Submit 루프에서 중복 GPU 상태 전환을 방지합니다.
    이전 커맨드와 동일한 상태는 skip하여 DeviceContext 호출을 최소화합니다.
*/
struct FStateCache
{
    // 첫 커맨드에서 모든 GPU 상태를 무조건 세팅 (센티넬 불필요)
    bool bForceAll = true;

    FShader* Shader = nullptr;
    EDepthStencilState DepthStencil = {};
    EBlendState Blend = {};
    ERasterizerState Rasterizer = {};
    D3D11_PRIMITIVE_TOPOLOGY Topology = {};
    uint8 StencilRef = 0;
    FMeshBuffer* MeshBuffer = nullptr;
    ID3D11Buffer* RawVB = nullptr;   // 동적 지오메트리 VB 추적
    ID3D11Buffer* RawIB = nullptr;   // 동적 지오메트리 IB 추적
    FConstantBuffer* PerObjectCB = nullptr;
    FConstantBuffer* PerShaderCB[2] = {};
    FConstantBuffer* LightCB = nullptr;
    ID3D11ShaderResourceView* DiffuseSRV = nullptr;
    ID3D11ShaderResourceView* NormalSRV = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV = nullptr;

    // Render target 추적 (CopyResource 후 DSV 복원 등)
    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11DepthStencilView* DSV = nullptr;

    void Reset();

    // 프레임 끝 정리 — material/system SRV 언바인딩
    void Cleanup(ID3D11DeviceContext* Ctx);
};

/*
    FDrawCommandList — 프레임 단위 커맨드 버퍼.
    RenderCollector가 커맨드를 추가하고, Sort() 후 Submit()으로 GPU에 제출합니다.
*/
class FDrawCommandList
{
public:
    FDrawCommand& AddCommand();
    void Sort();
    void GetPassRange(ERenderPass Pass, uint32& OutStart, uint32& OutEnd) const;
    void Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FStateCache& Cache);
    void Reset();

    bool IsEmpty() const { return Commands.empty(); }
    uint32 GetCommandCount() const { return static_cast<uint32>(Commands.size()); }
    uint32 GetCommandCount(ERenderPass Pass) const;

    TArray<FDrawCommand>& GetCommands() { return Commands; }
    const TArray<FDrawCommand>& GetCommands() const { return Commands; }

private:
    void SubmitCommand(const FDrawCommand& Cmd, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FStateCache& Cache);

    TArray<FDrawCommand> Commands;
    uint32 PassOffsets[(uint32)ERenderPass::MAX + 1] = {};
};
