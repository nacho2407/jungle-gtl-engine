#pragma once
#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Culling/GPUOcclusionCulling.h"

class UEditorEngine;
class FViewport;
class UCameraComponent;
class FLevelEditorViewportClient;

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

	// RenderViewport 내부 단계
	void PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx);
	void BuildFrame(FLevelEditorViewportClient* VC, UCameraComponent* Camera, FViewport* VP, UWorld* World);
	void CollectCommands(FLevelEditorViewportClient* VC, UWorld* World, FRenderer& Renderer);

private:
	UEditorEngine* Editor = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
	FGPUOcclusionCulling GPUOcclusion;
};
