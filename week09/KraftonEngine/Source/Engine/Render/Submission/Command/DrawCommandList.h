#pragma once

#include "Render/Execute/Context/PipelineStateTypes.h"
#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "DrawCommand.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    FDrawSubmitStateCache ? Submit �������� �ߺ� GPU ���� ��ȯ�� �����մϴ�.
    ���� Ŀ�ǵ�� ������ ���´� skip�Ͽ� DeviceContext ȣ���� �ּ�ȭ�մϴ�.
*/
struct FDrawSubmitStateCache
{
    // ù Ŀ�ǵ忡�� ��� GPU ���¸� ������ ���� (��Ƽ�� ���ʿ�)
    bool bForceAll = true;

    FShader*                  Shader         = nullptr;
    EDepthStencilState        DepthStencil   = {};
    EBlendState               Blend          = {};
    ERasterizerState          Rasterizer     = {};
    D3D11_PRIMITIVE_TOPOLOGY  Topology       = {};
    uint8                     StencilRef     = 0;
    FMeshBuffer*              MeshBuffer     = nullptr;
    ID3D11Buffer*             RawVB          = nullptr; // ���� ������Ʈ�� VB ����
    ID3D11Buffer*             RawIB          = nullptr; // ���� ������Ʈ�� IB ����
    FConstantBuffer*          PerObjectCB    = nullptr;
    FConstantBuffer*          PerShaderCB[2] = {};
    FConstantBuffer*          LightCB        = nullptr;
    ID3D11ShaderResourceView* DiffuseSRV     = nullptr;
    ID3D11ShaderResourceView* NormalSRV      = nullptr;
    ID3D11ShaderResourceView* SpecularSRV    = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV  = nullptr;

    // Render target ���� (CopyResource �� DSV ���� ��)
    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11DepthStencilView* DSV = nullptr;

    void Reset();

    // ������ �� ���� ? material/system SRV ����ε�
    void Cleanup(ID3D11DeviceContext* Ctx);
};

/*
    FDrawCommandList ? ������ ���� Ŀ�ǵ� ����.
    DrawCollector�� Ŀ�ǵ带 �߰��ϰ�, Sort() �� Submit()���� GPU�� �����մϴ�.
*/
class FDrawCommandList
{
public:
    FDrawCommand& AddCommand();
    void          Sort();
    void          GetPassRange(ERenderPass Pass, uint32& OutStart, uint32& OutEnd) const;
    void          Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void          SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx);
    void          SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FDrawSubmitStateCache& Cache);
    void          Reset();

    bool   IsEmpty() const { return Commands.empty(); }
    uint32 GetCommandCount() const { return static_cast<uint32>(Commands.size()); }
    uint32 GetCommandCount(ERenderPass Pass) const;

    TArray<FDrawCommand>&       GetCommands() { return Commands; }
    const TArray<FDrawCommand>& GetCommands() const { return Commands; }

private:
    void SubmitCommand(const FDrawCommand& Cmd, FD3DDevice& Device, ID3D11DeviceContext* Ctx, FDrawSubmitStateCache& Cache);

    TArray<FDrawCommand> Commands;
    uint32               PassOffsets[(uint32)ERenderPass::MAX + 1] = {};
};
