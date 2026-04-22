#pragma once

#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Execute/Context/PipelineStateTypes.h"
#include "Math/Vector.h"
#include "Core/CoreTypes.h"

class FShader;
class FMeshBuffer;
class FConstantBuffer;
struct ID3D11ShaderResourceView;
struct ID3D11Buffer;

/*
    FDrawCommand ???�로?�콜 1개에 ?�요??모든 ?�보�?캡슐?�합?�다.
    UE5??FMeshDrawCommand ?�턴??차용?�여,
    PSO ?�태 + Geometry + Bindings + ?�렬 ?��? ?�나??구조체로 ?�합?�니??
*/
struct FDrawCommand
{
    // ===== PSO (Pipeline State Object) =====
    FShader*                 Shader       = nullptr;
    EDepthStencilState       DepthStencil = EDepthStencilState::Default;
    EBlendState              Blend        = EBlendState::Opaque;
    ERasterizerState         Rasterizer   = ERasterizerState::SolidBackCull;
    D3D11_PRIMITIVE_TOPOLOGY Topology     = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint8                    StencilRef   = 0;

    // ===== Geometry =====
    FMeshBuffer* MeshBuffer  = nullptr; // VB + IB (nullptr ??RawVB ?�는 SV_VertexID 기반 ?�로??
    uint32       FirstIndex  = 0;       // ?�덱???�작 ?�프??
    uint32       IndexCount  = 0;       // DrawIndexed ?�덱????
    uint32       VertexCount = 0;       // IB ?�을 ??Draw(VertexCount, 0)
    int32        BaseVertex  = 0;       // DrawIndexed BaseVertexLocation

    // ===== Raw Buffer (?�적 지?�메?�리????MeshBuffer가 nullptr?????�용) =====
    ID3D11Buffer* RawVB       = nullptr;
    uint32        RawVBStride = 0;
    ID3D11Buffer* RawIB       = nullptr;

    // ===== Bindings =====
    FConstantBuffer*          PerObjectCB    = nullptr; // b1: Model + Color
    FConstantBuffer*          PerShaderCB[2] = {};      // [0]=b2 (PerShader0), [1]=b3 (PerShader1)
    FConstantBuffer*          LightCB        = nullptr; // b4: Global Lights Constant Buffer
    ID3D11ShaderResourceView* DiffuseSRV     = nullptr; // t0: Base / Diffuse ?�스�?
    ID3D11ShaderResourceView* NormalSRV      = nullptr; // t1: Normal map ?�스�?
    ID3D11ShaderResourceView* SpecularSRV    = nullptr; // t2: Specular map ?�스�?
    ID3D11ShaderResourceView* LocalLightSRV  = nullptr; // t6: LocalLights StructuredBuffer

    // ===== Sort =====
    uint64 SortKey = 0; // ?�렬 ??(Pass ??Shader ??MeshBuffer ??SRV)

    // ===== Debug =====
    ERenderPass Pass      = ERenderPass::Opaque; // ?�속 ?�스 (?�버�??�계??
    const char* DebugName = nullptr;             // ?�버�??�름

    // ===== SortKey ?�성 ?�틸리티 =====
    // Pass(4bit) | ShaderHash(16bit) | MeshHash(16bit) | SRVHash(16bit) | UserBits(12bit)
    static uint64 BuildSortKey(ERenderPass InPass, const FShader* InShader,
                               const FMeshBuffer* InMeshBuffer, const ID3D11ShaderResourceView* InSRV,
                               uint16 UserBits = 0)
    {
        auto PtrHash16 = [](const void* Ptr) -> uint16
        {
            // ?�인?��? 16비트�?축소 ???�태 ?�환 그룹?�용?��?�?충돌 ?�용
            uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
            return static_cast<uint16>((Val >> 4) ^ (Val >> 20));
        };

        uint64 Key = 0;
        Key |= (static_cast<uint64>(InPass) & 0xF) << 60;            // [63:60] Pass
        Key |= (static_cast<uint64>(PtrHash16(InShader))) << 44;     // [59:44] Shader
        Key |= (static_cast<uint64>(PtrHash16(InMeshBuffer))) << 28; // [43:28] MeshBuffer
        Key |= (static_cast<uint64>(PtrHash16(InSRV))) << 12;        // [27:12] SRV
        Key |= (static_cast<uint64>(UserBits) & 0xFFF);              // [11:0]  User
        return Key;
    }
};
