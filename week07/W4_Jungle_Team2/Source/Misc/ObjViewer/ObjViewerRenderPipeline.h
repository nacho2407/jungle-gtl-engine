#pragma once
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Engine/Render/Renderer/IRenderPipeline.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderBus.h"

class UObjViewerEngine;

class FObjViewerRenderPipeline : public IRenderPipeline
{
public:
	FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer);
	~FObjViewerRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
	UObjViewerEngine* Engine = nullptr;
	FRenderCollector Collector;
	FRenderBus Bus;
};