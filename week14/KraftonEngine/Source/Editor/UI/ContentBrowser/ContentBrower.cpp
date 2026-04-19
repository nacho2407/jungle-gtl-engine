#include "ContentBrower.h"

void ContextBrwoser::Render(float DeltaTime)
{
	int elementCount = BrowserElements.size();
	for (int i = 0; i < elementCount; i++)
	{
		BrowserElements[i]->Render(BrowserContext);
	}
}
