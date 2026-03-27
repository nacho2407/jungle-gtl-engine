#pragma once

#include "Core/Engine.h"

class ENGINE_API FGameEngine : public FEngine
{
public:
	FGameEngine() = default;
	~FGameEngine() override = default;

protected:
	ESceneType GetStartupSceneType() const override { return ESceneType::Game; }
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
};
