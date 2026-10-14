#pragma once
#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"

class UGameEngine;
class UWorld;
struct FMinimalViewInfo;

class FGameRenderPipeline : public IRenderPipeline
{
public:
	FGameRenderPipeline(UGameEngine* InGame, FRenderer& InRenderer);
	~FGameRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
	// EditorRenderPipeline 과 동일 패턴 — POV 통화 시그니처. 컴포넌트 의존은 PrepareViewport
	// 의 OnResize 한 곳에 격리 (PC->PlayerCameraManager->ActiveCamera 직접 가져옴).
	// World 는 BuildFrame 내부에서 PC 경유 fade/vignette 상태 read 용.
	void PrepareViewport(FViewport* VP, ID3D11DeviceContext* Ctx);
	void BuildFrame(FViewport* VP, const FMinimalViewInfo& POV, FScene* Scene, UWorld* World);
	void CollectCommands(FScene* Scene, FRenderer& Renderer, FCollectOutput& Output);

private:
	UGameEngine* Game = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
};
