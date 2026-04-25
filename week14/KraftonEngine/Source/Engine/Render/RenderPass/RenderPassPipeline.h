#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Pipeline/PassRenderStateTable.h"
#include <memory>

class FD3DDevice;
class FDrawCommandList;
struct FSystemResources;

/*
	FRenderPassPipeline — 렌더패스 실행 파이프라인.
	Registry에서 패스 인스턴스를 생성하고,
	BeginPass → Submit → EndPass 루프를 캡슐화합니다.
	FRenderer는 Pipeline.Execute() 한 줄로 전체 패스를 실행합니다.
*/
class FRenderPassPipeline
{
public:
	// Registry로부터 패스 생성 + 상태 테이블 빌드
	void Initialize();

	// 전체 패스 루프 실행
	void Execute(const FPassContext& Ctx, FDrawCommandList& CmdList,
	             FD3DDevice& Device, FSystemResources& Resources);

	// DrawCommandBuilder용 상태 테이블
	const FPassRenderStateTable& GetStateTable() const { return StateTable; }

	// 타입별 패스 조회
	template<typename T>
	T* FindPass() const
	{
		for (const auto& Pass : Passes)
		{
			if (T* Found = dynamic_cast<T*>(Pass.get()))
				return Found;
		}
		return nullptr;
	}

private:
	TArray<std::unique_ptr<FRenderPassBase>> Passes;
	FPassRenderStateTable StateTable;
};
