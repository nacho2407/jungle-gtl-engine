#pragma once
#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Types/FrameContext.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include <memory>

class UEditorEngine;
class FViewport;
class UCameraComponent;
class FLevelEditorViewportClient;
struct FMinimalViewInfo;

class FEditorRenderPipeline : public IRenderPipeline
{
public:
	FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer);
	~FEditorRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;
	void OnSceneCleared() override;

private:
	// 단일 뷰포트 렌더 오케스트레이션
	void RenderViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer);

	// RenderViewport 내부 단계 — POV 통화로 통일. 컴포넌트 의존성은 RenderViewport 진입부의
	// 게임 ActiveCamera 폴백 한 곳에만 남는다.
	void PrepareViewport(FLevelEditorViewportClient* VC, FViewport* VP, ID3D11DeviceContext* Ctx);
	void BuildFrame(FLevelEditorViewportClient* VC, const FMinimalViewInfo& POV, FViewport* VP, UWorld* World);
	void CollectCommands(FLevelEditorViewportClient* VC, UWorld* World, FRenderer& Renderer, FCollectOutput& Output);

	// 뷰포트별 GPUOcclusion 인스턴스 (lazy init)
	FGPUOcclusionCulling& GetOcclusionForViewport(FLevelEditorViewportClient* VC);

private:
	UEditorEngine* Editor = nullptr;
	ID3D11Device* CachedDevice = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
	TMap<FLevelEditorViewportClient*, std::unique_ptr<FGPUOcclusionCulling>> GPUOcclusionMap;
};
