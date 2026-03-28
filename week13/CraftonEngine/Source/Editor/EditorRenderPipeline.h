#pragma once
#include "Render/Renderer/IRenderPipeline.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderBus.h"

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

private:
	// 단일 뷰포트 렌더 단위 — ViewportClient의 렌더 옵션을 사용
	void RenderViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer);

private:
	UEditorEngine* Editor = nullptr;
	FRenderCollector Collector;
	FRenderBus Bus;
};
