#pragma once

#include "Singleton.h"
#include "RenderObject.h"
#include "EngineTypes.h"

class FLevel : public ISingleton<FLevel>
{
	friend class ISingleton<FLevel>;

public:
	void PushRenderObject(RenderObject* RenderObj);
	void RemoveRenderObject(RenderObject* RenderObj);
	void Clear();
	TArray<RenderObject*> RenderObjects;
};

