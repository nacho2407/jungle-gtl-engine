#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Math/Vector.h"
#include "Core/CoreTypes.h"

class FGraphicsProgram;
class FMeshBuffer;
class FConstantBuffer;
struct ID3D11ShaderResourceView;
struct ID3D11Buffer;

// FDrawCommand는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FDrawCommand
{
    // ===== PSO (Pipeline State Object) =====
    FGraphicsProgram*        Shader       = nullptr;
    EDepthStencilState       DepthStencil = EDepthStencilState::Default;
    EBlendState              Blend        = EBlendState::Opaque;
    ERasterizerState         Rasterizer   = ERasterizerState::SolidBackCull;
    D3D11_PRIMITIVE_TOPOLOGY Topology     = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint8                    StencilRef   = 0;

    // ===== Geometry =====
    FMeshBuffer* MeshBuffer  = nullptr;
    uint32       FirstIndex  = 0;
    uint32       IndexCount  = 0;
    uint32       VertexCount = 0;
    int32        BaseVertex  = 0; // DrawIndexed BaseVertexLocation

    ID3D11Buffer* RawVB       = nullptr;
    uint32        RawVBStride = 0;
    ID3D11Buffer* RawIB       = nullptr;

    // ===== Bindings =====
    FConstantBuffer*          PerObjectCB    = nullptr; // b1: Model + Color
    FConstantBuffer*          PerShaderCB[2] = {};      // [0]=b2 (PerShader0), [1]=b3 (PerShader1)
    FConstantBuffer*          LightCB        = nullptr; // b4: Global Lights Constant Buffer
    ID3D11ShaderResourceView* DiffuseSRV     = nullptr;
    ID3D11ShaderResourceView* NormalSRV      = nullptr;
    ID3D11ShaderResourceView* SpecularSRV    = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV  = nullptr; // t6: LocalLights StructuredBuffer

    // ===== Sort =====
    uint64 SortKey = 0;

    // ===== Debug =====
    ERenderPass Pass      = ERenderPass::Opaque;
    const char* DebugName = nullptr;

    // Pass(4bit) | UserBits(8bit) | ShaderHash(16bit) | MeshHash(16bit) | MaterialHash(20bit)
    static uint64 BuildSortKey(ERenderPass InPass,
                               uint8 InUserBits,
                               const FGraphicsProgram* InShader,
                               const FMeshBuffer* InMeshBuffer,
                               uint32 InMaterialHash)
    {
        auto PtrHash16 = [](const void* Ptr) -> uint64
        {
            uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
            return static_cast<uint64>((Val >> 4) ^ (Val >> 20)) & 0xFFFFu;
        };

        uint64 Key = 0;
        Key |= (static_cast<uint64>(InPass) & 0xFu) << 60;             // [63:60] Pass
        Key |= (static_cast<uint64>(InUserBits) & 0xFFu) << 52;        // [59:52] UserBits
        Key |= PtrHash16(InShader) << 36;                              // [51:36] Shader
        Key |= PtrHash16(InMeshBuffer) << 20;                          // [35:20] MeshBuffer
        Key |= (static_cast<uint64>(InMaterialHash) & 0xFFFFFu);       // [19:0]  Material
        return Key;
    }
};
