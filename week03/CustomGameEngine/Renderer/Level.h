#pragma once

#include "Singleton.h"
#include "RenderObject.h"
#include "EngineTypes.h"

class FLevel : public ISingleton<FLevel>
{
	friend class ISingleton<FLevel>;

public:
	void RegisterRenderObject(RenderObject* RenderObj);
	void UnregisterRenderObject(RenderObject* RenderObj);
	void Clear();
	TArray<RenderObject*> RenderObjects;
};

