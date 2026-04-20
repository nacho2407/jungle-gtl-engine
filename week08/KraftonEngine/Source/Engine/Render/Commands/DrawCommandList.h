#pragma once

#include "DrawCommand.h"
#include "Render/Hardware/Device/D3DDevice.h"
#include "Render/Hardware/Resources/Buffer.h"

/*
	FStateCache — Submit 루프에서 중복 GPU 상태 전환을 방지합니다.
	이전 커맨드와 동일한 상태는 skip하여 DeviceContext 호출을 최소화합니다.
*/
struct FStateCache
{
	// 첫 커맨드에서 모든 GPU 상태를 무조건 세팅 (센티넬 불필요)
	bool bForceAll = true;

	FShader*                  Shader       = nullptr;
	EDepthStencilState        DepthStencil = {};
	EBlendState               Blend        = {};
	ERasterizerState          Rasterizer   = {};
	D3D11_PRIMITIVE_TOPOLOGY  Topology     = {};
	uint8                     StencilRef   = 0;
	FMeshBuffer*              MeshBuffer   = nullptr;
	ID3D11Buffer*             RawVB        = nullptr;   // 동적 지오메트리 VB 추적
	ID3D11Buffer*             RawIB        = nullptr;   // 동적 지오메트리 IB 추적
	FConstantBuffer*          PerObjectCB    = nullptr;
	FConstantBuffer*          PerShaderCB[2] = {};
	ID3D11ShaderResourceView* DiffuseSRV    = nullptr;   // t0: BaseColor / Diffuse
	ID3D11ShaderResourceView* NormalSRV     = nullptr;   // t1: Normal map

	// Render target 추적 (CopyResource 후 DSV 복원 등)
	ID3D11RenderTargetView*  RTV         = nullptr;
	ID3D11DepthStencilView*  DSV         = nullptr;

	void Reset();

	// 프레임 끝 정리 — t0 SRV 언바인딩
	void Cleanup(ID3D11DeviceContext* Ctx);
};

/*
	FDrawCommandList — 프레임 단위 커맨드 버퍼.
	RenderCollector가 커맨드를 추가하고, Sort() 후 Submit()으로 GPU에 제출합니다.
*/
class FDrawCommandList
{
public:
	// 커맨드 추가 — 기본값으로 초기화된 FDrawCommand 참조 반환
	FDrawCommand& AddCommand();

	// Pass → SortKey 순 정렬 + 패스별 오프셋 빌드
	void Sort();

	// 패스별 커맨드 범위 [Start, End)
	void GetPassRange(ERenderPass Pass, uint32& OutStart, uint32& OutEnd) const;

	// StateCache 기반 GPU 제출 (전체)
	void Submit(FD3DDevice& Device, ID3D11DeviceContext* Ctx);

	// 범위 제출 — [StartIdx, EndIdx) 구간의 커맨드만 제출
	void SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
		ID3D11DeviceContext* Ctx);

	// 외부 FStateCache 공유 — 패스 간 상태 유지
	void SubmitRange(uint32 StartIdx, uint32 EndIdx, FD3DDevice& Device,
		ID3D11DeviceContext* Ctx, FStateCache& Cache);

	// 프레임 끝 초기화
	void Reset();

	// 비어 있는지 확인
	bool IsEmpty() const { return Commands.empty(); }

	// 현재 커맨드 수
	uint32 GetCommandCount() const { return static_cast<uint32>(Commands.size()); }

	// 특정 패스의 커맨드 수 (디버그/통계용)
	uint32 GetCommandCount(ERenderPass Pass) const;

	// 커맨드 직접 수정이 필요한 패스(예: 풀스크린 포스트 프로세스)용 접근
	TArray<FDrawCommand>& GetCommands() { return Commands; }

	// 읽기 전용 접근
	const TArray<FDrawCommand>& GetCommands() const { return Commands; }

private:
	void SubmitCommand(const FDrawCommand& Cmd, FD3DDevice& Device,
		ID3D11DeviceContext* Ctx, FStateCache& Cache);

	TArray<FDrawCommand> Commands;
	uint32 PassOffsets[(uint32)ERenderPass::MAX + 1] = {};
};
