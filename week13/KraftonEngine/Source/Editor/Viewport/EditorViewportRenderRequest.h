#pragma once

#include "Core/Types/CoreTypes.h"

class FViewport;
class UWorld;
class IPOVProvider;
struct FViewportRenderOptions;

struct FEditorViewportRenderRequest
{
	FViewport* Viewport = nullptr;
	UWorld* World = nullptr;
	IPOVProvider* POVProvider = nullptr;
	FViewportRenderOptions* RenderOptions = nullptr;

	
};
