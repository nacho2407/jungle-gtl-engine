#include "PanelManager.h"

#include <cwchar>

void FPanelManager::Initialize(FEditorContext* InContext)
{
    Context = InContext;
}

void FPanelManager::Shutdown()
{
    for (auto& Panel : Panels)
    {
        Panel->ShutDown();
    }

    Panels.clear();
    PanelFactories.clear();
}

void FPanelManager::RegisterPanel(std::unique_ptr<IPanel> Panel)
{
    if (!Panel)
    {
        return;
    }

    Panel->Initialize(Context, this);
    if (Panel->ShouldOpenByDefault())
    {
        Panel->SetOpen(true);
    }

    Panels.push_back(std::move(Panel));
}

void FPanelManager::Tick(float DeltaTime)
{
    for (auto& Panel : Panels)
    {
        if (Panel->IsOpen())
        {
            Panel->Tick(DeltaTime);
        }
    }
}

void FPanelManager::DrawPanels()
{
    for (auto& Panel : Panels)
    {
        if (Panel->IsOpen())
        {
            Panel->Draw();
        }
    }
}

IPanel* FPanelManager::FindPanelById(const wchar_t* PanelId)
{
    if (!PanelId)
    {
        return nullptr;
    }

    for (auto& Panel : Panels)
    {
        if (Panel->GetPanelID() && std::wcscmp(Panel->GetPanelID(), PanelId) == 0)
        {
            return Panel.get();
        }
    }

    return nullptr;
}

IPanel* FPanelManager::OpenPanel(const FPanelOpenRequest& Request)
{
    if (Request.OpenPolicy != EPanelOpenPolicy::AlwaysCreateNew)
    {
        if (IPanel* ExistingPanel = FindMatchingPanel(Request))
        {
            ExistingPanel->ApplyOpenRequest(Request);
            ExistingPanel->SetOpen(true);
            return ExistingPanel;
        }
    }

    std::unique_ptr<IPanel> NewPanel = CreatePanel(Request);
    if (!NewPanel)
    {
        return nullptr;
    }

    IPanel* RawPanel = NewPanel.get();
    RegisterPanel(std::move(NewPanel));
    RawPanel->ApplyOpenRequest(Request);
    RawPanel->SetOpen(true);
    return RawPanel;
}

IPanel* FPanelManager::FindPanel(const FPanelOpenRequest& Request)
{
    return FindMatchingPanel(Request);
}

void FPanelManager::ClosePanel(const FPanelOpenRequest& Request)
{
    if (IPanel* ExistingPanel = FindMatchingPanel(Request))
    {
        ExistingPanel->SetOpen(false);
    }
}

void FPanelManager::TogglePanel(const FPanelOpenRequest& Request)
{
    if (IPanel* ExistingPanel = FindMatchingPanel(Request))
    {
        ExistingPanel->ToggleOpen();
        return;
    }

    OpenPanel(Request);
}

std::unique_ptr<IPanel> FPanelManager::CreatePanel(const FPanelOpenRequest& Request)
{
    const auto FactoryIt = PanelFactories.find(Request.PanelType);
    if (FactoryIt == PanelFactories.end())
    {
        return nullptr;
    }

    return FactoryIt->second();
}

IPanel* FPanelManager::FindMatchingPanel(const FPanelOpenRequest& Request) const
{
    for (const auto& Panel : Panels)
    {
        if (Panel->MatchesRequest(Request))
        {
            return Panel.get();
        }
    }

    return nullptr;
}
