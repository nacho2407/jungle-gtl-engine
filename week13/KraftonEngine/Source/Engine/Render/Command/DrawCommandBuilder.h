#pragma once

#include "Render/Command/DrawCommandList.h"
#include "Render/Types/FrameContext.h"
#include "Render/Geometry/LineGeometry.h"
#include "Render/Geometry/FontGeometry.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

class FPassRenderStateTable;
class FTextRenderSceneProxy;
class FScene;
struct FCollectOutput;

/*
	FDrawCommandBuilder — Collect 페이즈에서 Proxy/Scene 데이터를 FDrawCommand로 변환합니다.
	FRenderer에서 커맨드 빌드 책임을 분리하여, Renderer는 GPU 제출에만 집중합니다.
*/
class FDrawCommandBuilder
{
public:
	void Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, const FPassRenderStateTable* InPassRenderStateTable);
	void Release();

	// Collect 시작 — 커맨드 리스트 + 동적 지오메트리 초기화
	void BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount = 0);

	// Proxy → FDrawCommand 변환
	void BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass);
	void BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy);

	// Font proxy → FontGeometry 배칭
	void AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame);

	// FCollectOutput → 프록시 커맨드 + 동적 커맨드 일괄 생성
	void BuildCommands(const FFrameContext& Frame, FScene* Scene, const FCollectOutput& Output);

	// 결과 접근
	FDrawCommandList& GetCommandList() { return DrawCommandList; }
	bool HasSelectionMaskCommands() const { return bHasSelectionMaskCommands; }

private:
	// BuildCommands 서브 메서드
	void BuildProxyCommands(const FFrameContext& Frame, FScene& Scene, const FCollectOutput& Output);
	void BuildDecalCommands(FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame, const FCollectOutput& Output);
	void BuildMeshCommands(const FPrimitiveSceneProxy* Proxy);
	void BuildSelectionCommands(FPrimitiveSceneProxy* Proxy, bool bShowBoundingVolume, FScene& Scene);

	// Scene 경량 데이터 → 동적 지오메트리 → FDrawCommand
	void BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene);

	void PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene);
	void BuildDynamicDrawCommands(const FFrameContext& Frame, const FScene* Scene);

	// BuildDynamicDrawCommands 서브 메서드
	void BuildEditorLineCommands(EViewMode ViewMode);
	void BuildPostProcessCommands(const FFrameContext& Frame, const FScene* Scene);
	void BuildFontCommands(EViewMode ViewMode);

	// 공통 헬퍼
	void EmitLineCommand(FLineGeometry& Lines, FShader* Shader, const FDrawCommandRenderState& RS);
	void ApplyMaterialRenderState(FDrawCommandRenderState& OutState, const UMaterial* Mat, const FDrawCommandRenderState& BaseState);
	FShader* SelectEffectiveShader(FShader* ProxyShader, EViewMode ViewMode);

	FConstantBuffer* GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);
	void EnsurePerObjectCBPoolCapacity(uint32 RequiredCount);

	// 커맨드 버퍼
	FDrawCommandList DrawCommandList;

	// Collect 페이즈 상태
	const FPassRenderStateTable* PassRenderStateTable = nullptr;
	EViewMode CollectViewMode = EViewMode::Lit_Phong;
	bool bHasSelectionMaskCommands = false;

	// 동적 지오메트리
	FLineGeometry  EditorLines;
	FLineGeometry  GridLines;
	FFontGeometry  FontGeometry;

	// PerObject CB 풀
	TArray<FConstantBuffer> PerObjectCBPool;

	// PostProcess CBs (Fog, Outline, SceneDepth, FXAA)
	FConstantBuffer FogCB;
	FConstantBuffer OutlineCB;
	FConstantBuffer SceneDepthCB;
	FConstantBuffer FXAACB;

	// D3D 디바이스 캐시 (Create 시 설정, 변하지 않음)
	ID3D11Device*        CachedDevice  = nullptr;
	ID3D11DeviceContext* CachedContext = nullptr;
};
