#pragma once

#include <functional>
#include <memory>
#include "Core/CoreTypes.h"
#include "Render/RenderPass/RenderPassBase.h"

/*
	FRenderPassRegistry — 렌더패스 자동 등록 레지스트리.
	UE의 IMPLEMENT_MESH_PASS_PROCESSOR 패턴을 차용하여,
	각 패스 .cpp에 REGISTER_RENDER_PASS 매크로만 추가하면
	파이프라인에 자동으로 등록됩니다.
*/

using FRenderPassFactory = std::function<std::unique_ptr<FRenderPassBase>()>;

class FRenderPassRegistry
{
public:
	static FRenderPassRegistry& Get();

	// 팩토리 등록 (정적 초기화 시 호출)
	void Register(FRenderPassFactory Factory);

	// 등록된 모든 패스 인스턴스 생성 → PassType 순 정렬
	TArray<std::unique_ptr<FRenderPassBase>> CreateAll() const;

private:
	TArray<FRenderPassFactory> Factories;
};

// ============================================================
// REGISTER_RENDER_PASS — 패스 .cpp 끝에 추가하면 자동 등록
// ============================================================
#define REGISTER_RENDER_PASS(ClassName) \
	static struct ClassName##_AutoRegister { \
		ClassName##_AutoRegister() { \
			FRenderPassRegistry::Get().Register([]() -> std::unique_ptr<FRenderPassBase> { \
				return std::make_unique<ClassName>(); \
			}); \
		} \
	} g_##ClassName##_AutoRegister;
