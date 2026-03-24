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

private:
	UEditorEngine* Editor = nullptr;
	FRenderCollector Collector;
	FRenderBus Bus;
};
