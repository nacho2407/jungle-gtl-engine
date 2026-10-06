#include "RenderPassRegistry.h"

#include <algorithm>

FRenderPassRegistry& FRenderPassRegistry::Get()
{
	static FRenderPassRegistry Instance;
	return Instance;
}

void FRenderPassRegistry::Register(FRenderPassFactory Factory)
{
	Factories.push_back(std::move(Factory));
}

TArray<std::unique_ptr<FRenderPassBase>> FRenderPassRegistry::CreateAll() const
{
	TArray<std::unique_ptr<FRenderPassBase>> Passes;
	Passes.reserve(Factories.size());

	for (const auto& Factory : Factories)
	{
		Passes.push_back(Factory());
	}

	// ERenderPass enum 순서 = 실행 순서
	std::sort(Passes.begin(), Passes.end(),
		[](const std::unique_ptr<FRenderPassBase>& A, const std::unique_ptr<FRenderPassBase>& B)
		{
			return static_cast<uint32>(A->GetPassType()) < static_cast<uint32>(B->GetPassType());
		});

	return Passes;
}
