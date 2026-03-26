#pragma once

#include "Singleton.h"
#include "RenderObject.h"
#include "EngineCore/EngineTypes.h"

class FLevel
{
public:
	void RegisterRenderObject(RenderObject* RenderObj);
	void UnregisterRenderObject(RenderObject* RenderObj);
	void Clear();
	TArray<RenderObject*> RenderObjects;
};

