#include "Level.h"

void FLevel::RegisterRenderObject(RenderObject* RenderObj)
{
	//중복으로 들어가는 경우를 방지
	for(auto* it : RenderObjects)
	{
		if (it == RenderObj)
		{
			return;
		}
	}
	RenderObjects.push_back(RenderObj);
}

// Renderer가 순회하는 리스트에서만 제거. 할당 해제는 UPrimitiveComponent에서.
void FLevel::UnregisterRenderObject(RenderObject* RenderObj)
{
	for (auto it = RenderObjects.begin(); it != RenderObjects.end();)
	{
		if (*it == RenderObj)
		{
			RenderObjects.erase(it);
			return;
		}
		else
		{
			++it;
		}
	}
}

void FLevel::Clear()
{
	RenderObjects.clear();
}
