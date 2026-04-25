#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/RenderPass/PassRenderStateTable.h"
#include <memory>

/*
	FRenderPassPipeline — 렌더패스 실행 파이프라인.
	Registry에서 패스 인스턴스를 생성하고,
	BeginPass → Execute → EndPass 루프를 캡슐화합니다.
	FRenderer는 Pipeline.Execute() 한 줄로 전체 패스를 실행합니다.
*/
class FRenderPassPipeline
{
public:
	// Registry로부터 패스 생성 + 상태 테이블 빌드
	void Initialize();

	// 전체 패스 루프 실행
	void Execute(const FPassContext& Ctx);

	// DrawCommandBuilder용 상태 테이블
	const FPassRenderStateTable& GetStateTable() const { return StateTable; }

	// 타입으로 패스 검색 (에디터 디버그 등에서 사용)
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
