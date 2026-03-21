#include "PanelManager.h"

#include "Panel.h"

void FPanelManager::Initialize(FEditorContext* InContext)
{
	Context = InContext;
}

void FPanelManager::Shutdown()
{
	// TODO : 초기화 작업
	for (auto& Panel : Panels)
	{
		delete Panel;
	}

	Panels.clear();
}

void FPanelManager::RegisterPanel(IPanel* Panel)
{
	if (Panel)
	{
		Panel->Initialize(Context);
		Panels.push_back(Panel);
	}
}

void FPanelManager::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{
		Panel->Tick(DeltaTime);
	}
}

void FPanelManager::DrawPanels()
{
	for (auto& Panel : Panels)
	{
		Panel->Draw();
	}
}

IPanel* FPanelManager::FindPanelById(const char* PanelId)
{
}
