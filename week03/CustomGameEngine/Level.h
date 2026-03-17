#pragma once
#include "RenderObject.h"
#include "Singleton.h"

class FLevel : public ISingleton<FLevel>
{
	friend class ISingleton<FLevel>;

public:
	void PushRenderObject(RenderObject* RenderObj);
	void RemoveRenderObject(RenderObject* RenderObj);
	TArray<RenderObject*> RenderObjects;
};

