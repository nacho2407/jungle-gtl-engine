#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Math/Vector.h"
#include "Core/CoreTypes.h"

class FShader;
class FMeshBuffer;
class FConstantBuffer;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11Buffer;

/*
	FDrawCommand — 드로우콜 1개에 필요한 모든 정보를 캡슐화합니다.
	UE5의 FMeshDrawCommand 패턴을 차용하여,
	PSO 상태 + Geometry + Bindings + 정렬 키를 하나의 구조체로 통합합니다.
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
	bool                     bReadOnlyDSV = false;  // DSVReadOnly + SRV 동시 바인딩 (PostProcess)

	// ===== Geometry =====
	FMeshBuffer* MeshBuffer  = nullptr;   // VB + IB (nullptr → RawVB 또는 SV_VertexID 기반 드로우)
	uint32       FirstIndex  = 0;         // 인덱스 시작 오프셋
	uint32       IndexCount  = 0;         // DrawIndexed 인덱스 수
	uint32       VertexCount = 0;         // IB 없을 때 Draw(VertexCount, 0)
	int32        BaseVertex  = 0;         // DrawIndexed BaseVertexLocation

	// ===== Raw Buffer (동적 지오메트리용 — MeshBuffer가 nullptr일 때 사용) =====
	ID3D11Buffer* RawVB       = nullptr;
	uint32        RawVBStride = 0;
	ID3D11Buffer* RawIB       = nullptr;

	// ===== Bindings =====
	FConstantBuffer*         PerObjectCB = nullptr;   // b1: Model + Color
	FConstantBuffer*         ExtraCB     = nullptr;   // b2: Gizmo 등 (optional)
	uint32                   ExtraCBSlot = 0;         // ExtraCB의 슬롯 번호
	FConstantBuffer*         MaterialCB  = nullptr;   // b4: UVScroll, SectionColor (optional)
	ID3D11ShaderResourceView* DiffuseSRV  = nullptr;  // t0: 디퓨즈 텍스처
	ID3D11SamplerState*       Sampler     = nullptr;  // s0: 샘플러 오버라이드 (nullptr → 기본 샘플러)

	// ===== Material 인라인 데이터 (Submit 시 MaterialCB에 업로드) =====
	FVector4 SectionColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	uint32   bIsUVScroll  = 0;

	// ===== Sort =====
	uint64 SortKey = 0;                              // 정렬 키 (Pass → Shader → MeshBuffer → SRV)

	// ===== Debug =====
	ERenderPass  Pass      = ERenderPass::Opaque;     // 소속 패스 (디버그/통계용)
	const char*  DebugName = nullptr;                  // 디버그 이름

	// ===== SortKey 생성 유틸리티 =====
	// Pass(4bit) | ShaderHash(16bit) | MeshHash(16bit) | SRVHash(16bit) | UserBits(12bit)
	static uint64 BuildSortKey(ERenderPass InPass, const FShader* InShader,
		const FMeshBuffer* InMeshBuffer, const ID3D11ShaderResourceView* InSRV,
		uint16 UserBits = 0)
	{
		auto PtrHash16 = [](const void* Ptr) -> uint16
		{
			// 포인터를 16비트로 축소 — 상태 전환 그룹핑용이므로 충돌 허용
			uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
			return static_cast<uint16>((Val >> 4) ^ (Val >> 20));
		};

		uint64 Key = 0;
		Key |= (static_cast<uint64>(InPass) & 0xF) << 60;           // [63:60] Pass
		Key |= (static_cast<uint64>(PtrHash16(InShader))) << 44;     // [59:44] Shader
		Key |= (static_cast<uint64>(PtrHash16(InMeshBuffer))) << 28; // [43:28] MeshBuffer
		Key |= (static_cast<uint64>(PtrHash16(InSRV))) << 12;        // [27:12] SRV
		Key |= (static_cast<uint64>(UserBits) & 0xFFF);              // [11:0]  User
		return Key;
	}
};
