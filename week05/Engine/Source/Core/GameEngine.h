#pragma once

#include "Core/Engine.h"

class ENGINE_API FGameEngine : public FEngine
{
public:
	FGameEngine() = default;
	~FGameEngine() override = default;

protected:
	bool InitializeWorlds(int32 Width, int32 Height) override;
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
	void TickWorlds(float DeltaTime) override;
};
