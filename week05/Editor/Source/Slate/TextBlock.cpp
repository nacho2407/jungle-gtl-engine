#include "TextBlock.h"

void STextBlock::SetText(const FString& InText)
{
	if (Text == InText) return;
	Text = InText;
	delete CachedMesh;
	CachedMesh = nullptr;
}
