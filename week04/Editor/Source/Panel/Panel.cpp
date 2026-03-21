#include "Panel.h"

void IPanel::Initialize(FEditorContext* InContext, IPanelService* InPanelService)
{
	Context = InContext;
	PanelService = InPanelService;
}

void IPanel::ApplyOpenRequest(const FPanelOpenRequest& Request)
{
	ContextKey = Request.ContextKey;
}

bool IPanel::MatchesRequest(const FPanelOpenRequest& Request) const
{
	return std::type_index(typeid(*this)) == Request.PanelType && ContextKey == Request.ContextKey;
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
