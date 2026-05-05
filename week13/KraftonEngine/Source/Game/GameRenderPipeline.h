#pragma once
#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"

class UGameEngine;

class FGameRenderPipeline : public IRenderPipeline
{
public:
	FGameRenderPipeline(UGameEngine* InGame, FRenderer& InRenderer);
	~FGameRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
	void PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx);
	void BuildFrame(FViewport* VP, UCameraComponent* Camera, FScene* Scene, UWorld* World);
	void CollectCommands(FScene* Scene, FRenderer& Renderer, FCollectOutput& Output);
		
private:
	UGameEngine* Game = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
};
