#pragma once
#include "IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Pipeline/FrameContext.h"

class UEngine;

class FDefaultRenderPipeline : public IRenderPipeline
{
public:
	FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer);
	~FDefaultRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
	UEngine* Engine = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
};
