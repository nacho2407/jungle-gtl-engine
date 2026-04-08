#pragma once

#include "CoreMinimal.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/SceneCommandBuilder.h"
#include <d3d11.h>

class FRenderer;
class FMaterial;
struct FSceneViewRenderRequest;

/**
 * 씬 전용 렌더러다.
 * 씬 패킷을 실제 렌더 커맨드로 바꾸고, 정렬과 패스 실행까지 담당한다.
 */
class ENGINE_API FSceneRenderer
{
public:
	// 새 프레임을 시작하며 이전 씬 커맨드 캐시 상태를 초기화한다.
	void BeginFrame();
	// 직전 프레임의 씬 커맨드 개수를 반환한다.
	size_t GetPrevCommandCount() const;

	// 씬 패킷을 커맨드로 변환해 지정한 타깃에 바로 렌더링한다.
	bool RenderPacketToTarget(
		FRenderer& Renderer,
		ID3D11RenderTargetView* RenderTargetView,
		ID3D11DepthStencilView* DepthStencilView,
		const D3D11_VIEWPORT& Viewport,
		const FSceneRenderPacket& Packet,
		const FSceneViewRenderRequest& SceneView,
		const FRenderCommandQueue& AdditionalCommands,
		bool bForceWireframe,
		FMaterial* WireframeMaterial,
		const float ClearColor[4]);

	// 이미 구성된 커맨드 큐를 지정한 타깃에 렌더링한다.
	bool RenderQueueToTarget(
		FRenderer& Renderer,
		ID3D11RenderTargetView* RenderTargetView,
		ID3D11DepthStencilView* DepthStencilView,
		const D3D11_VIEWPORT& Viewport,
		const FRenderCommandQueue& Queue,
		const float ClearColor[4]);

private:
	// 고수준 씬 패킷을 실제 씬 커맨드 큐로 변환한다.
	void BuildQueue(
		FRenderer& Renderer,
		const FSceneRenderPacket& Packet,
		const FSceneViewRenderRequest& SceneView,
		FRenderCommandQueue& OutQueue);
	// 내부 실행 리스트에 커맨드 하나를 추가하고 정렬 키를 계산한다.
	void AddCommand(FRenderer& Renderer, const FRenderCommand& Command);
	// 다음 패스를 위해 내부 실행 리스트를 비운다.
	void ClearCommandList();
	// 큐 내용을 내부 실행 리스트에 적재한다.
	void SubmitCommands(FRenderer& Renderer, const FRenderCommandQueue& Queue);
	// 내부 씬 커맨드 리스트를 정렬하고 실행한다.
	void ExecuteCommands(FRenderer& Renderer);
	// 내부 씬 커맨드 리스트 중 특정 레이어 하나를 실행한다.
	void ExecuteRenderPass(FRenderer& Renderer, ERenderLayer RenderLayer);

	// 외부에서 만든 추가 커맨드를 씬 큐 뒤에 합친다.
	static void AppendAdditionalCommands(FRenderCommandQueue& InOutQueue, const FRenderCommandQueue& AdditionalCommands);
	// 요청 시 씬 머티리얼을 와이어프레임 머티리얼로 치환한다.
	static void ApplyWireframeOverride(FRenderCommandQueue& InOutQueue, FMaterial* WireframeMaterial);

private:
	FSceneCommandBuilder SceneCommandBuilder;
	TArray<FRenderCommand> CommandList;
	size_t PrevCommandCount = 0;
	uint64 NextSubmissionOrder = 0;
};
