#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/DrawCommandBuilder.h"
#include "Render/Pipeline/PassRenderStateTable.h"
#include "Render/Pipeline/PassEventBuilder.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"

class FScene;

class FRenderer
{
public:
	void Create(HWND hWindow);
	void Release();

	// --- Render phase: 정렬 + GPU 제출 ---
	void BeginFrame();
	void Render(const FFrameContext& Frame, FScene& Scene);
	void EndFrame();

	FD3DDevice& GetFD3DDevice() { return Device; }

	// Collect 페이즈에서 커맨드 빌드를 담당하는 Builder
	FDrawCommandBuilder& GetBuilder() { return Builder; }

	// 뷰포트 리사이즈 후 렌더 상태 캐시 초기화
	void ResetRenderStateCache() { Resources.ResetRenderStateCache(); }

private:
	// 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
	void CleanupPassState(FStateCache& Cache);

private:
	FD3DDevice Device;

	FSystemResources Resources;
	FDrawCommandBuilder Builder;
	FPassRenderStateTable PassRenderStateTable;
	FPassEventBuilder PassEventBuilder;
};
