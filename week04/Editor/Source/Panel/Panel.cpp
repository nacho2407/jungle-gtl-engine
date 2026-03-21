#include "Panel.h"

void IPanel::Initialize(FEditorContext* InContext)
{
	Context = InContext;
}

void IPanel::SetOpen(bool bInOpen)
{
	if (bOpen == bInOpen)
	{
		return;
	}

	bOpen = bInOpen;

	if (bOpen)
	{
		OnOpen();
	}

	else
	{
		OnClose();
	}
}

void IPanel::ToggleOpen()
{
	SetOpen(!bOpen);
}
