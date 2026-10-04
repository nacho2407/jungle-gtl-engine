#pragma once

class FRenderer;

class IRenderPipeline
{
public:
	virtual ~IRenderPipeline() = default;
	virtual void Execute(float DeltaTime, FRenderer& Renderer) = 0;
};
