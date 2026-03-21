#pragma once
#include "Core/Containers/Array.h"

class IPanel;
struct FEditorContext;

class FPanelManager
{
public:
    void Initialize(FEditorContext* InContext);
    void Shutdown();

    void RegisterPanel(IPanel* Panel);

    void Tick(float DeltaTime);
    void DrawPanels();

    IPanel* FindPanelById(const char* PanelId);

private:
    FEditorContext* Context = nullptr;
    TArray<IPanel*> Panels;
};
