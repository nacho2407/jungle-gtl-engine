#pragma once

#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Types/FrameContext.h"

class UObjViewerEngine;
class FViewport;
class UCameraComponent;

class FObjViewerRenderPipeline : public IRenderPipeline
{
public:
	FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer);
	~FObjViewerRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
	void RenderPreviewViewport(FRenderer& Renderer);

private:
	UObjViewerEngine* Engine = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
};
