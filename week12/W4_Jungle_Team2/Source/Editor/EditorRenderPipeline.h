#pragma once
#include "Render/Renderer/IRenderPipeline.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderBus.h"

class UEditorEngine;

class FEditorRenderPipeline : public IRenderPipeline
{
public:
	FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer);
	~FEditorRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;	
	void Render3DWorld(FRenderer& Renderer);
	void Render2DOverlay(float DeltaTime, FRenderer& Renderer);

private:
	UEditorEngine* Editor = nullptr;
	FRenderCollector Collector;
	FRenderBus Bus;
};
