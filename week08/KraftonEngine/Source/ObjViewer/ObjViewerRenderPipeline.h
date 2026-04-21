// #pragma once
//
// #include "Render/Submission/Collect/RenderCollector.h"
// #include "Render/Core/FrameContext.h"
//
// class UObjViewerEngine;
// class FViewport;
// class UCameraComponent;
//
// class FObjViewerRenderPipeline
//{
// public:
//	FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer);
//	~FObjViewerRenderPipeline() override;
//
//	void Execute(float DeltaTime, FRenderer& Renderer);
//
// private:
//	void RenderPreviewViewport(FRenderer& Renderer);
//
// private:
//	UObjViewerEngine* Engine = nullptr;
//	FRenderCollector Collector;
//	FFrameContext Frame;
// };
