#pragma once
#include "RenderObject.h"
#include "ISingleton.h"

class FLevel : public ISingleton<FLevel>
{
	friend class ISingleton<FLevel>;

public:
	void PushRenderObject(RenderObject* RenderObj);
	void RemoveRenderObject(RenderObject* RenderObj);
	TArray<RenderObject*> RenderObjects;
};

