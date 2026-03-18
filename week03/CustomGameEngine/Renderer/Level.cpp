#include "Level.h"

void FLevel::RegisterRenderObject(RenderObject* RenderObj)
{
	RenderObjects.push_back(RenderObj);
}

// Renderer가 순회하는 리스트에서만 제거. 할당 해제는 UPrimitiveComponent에서.
void FLevel::UnregisterRenderObject(RenderObject* RenderObj)
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
