#include "Level.h"

void FLevel::PushRenderObject(RenderObject* RenderObj)
{
	RenderObjects.push_back(RenderObj);
}

void FLevel::RemoveRenderObject(RenderObject* RenderObj)
{
	for (auto it = RenderObjects.begin(); it != RenderObjects.end(); it++)
	{
		if (*it == RenderObj)
		{
			RenderObjects.erase(it);
			return;
		}
	}
}

void FLevel::Clear()
{
	RenderObjects.clear();
}
