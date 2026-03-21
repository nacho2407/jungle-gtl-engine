#pragma once

#include "Panel.h"

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"

#include <functional>
#include <memory>
#include <type_traits>

struct FEditorContext;

class FPanelManager : public IPanelService
{
public:
    void Initialize(FEditorContext* InContext);
    void Shutdown();

    void RegisterPanel(std::unique_ptr<IPanel> Panel);

    template <typename TPanel, typename... TArgs>
    TPanel* RegisterPanelInstance(TArgs&&... Args)
    {
        static_assert(std::is_base_of_v<IPanel, TPanel>);

        auto Panel = std::make_unique<TPanel>(std::forward<TArgs>(Args)...);
        TPanel* RawPanel = Panel.get();
        RegisterPanel(std::move(Panel));
        return RawPanel;
    }

    template <typename TPanel>
    void RegisterPanelType()
    {
        static_assert(std::is_base_of_v<IPanel, TPanel>);
        PanelFactories[std::type_index(typeid(TPanel))] = []()
        {
            return std::make_unique<TPanel>();
        };
    }

    void Tick(float DeltaTime);
    void DrawPanels();

    IPanel* FindPanelById(const wchar_t* PanelId);

    IPanel* OpenPanel(const FPanelOpenRequest& Request) override;
    IPanel* FindPanel(const FPanelOpenRequest& Request) override;
    void ClosePanel(const FPanelOpenRequest& Request) override;
    void TogglePanel(const FPanelOpenRequest& Request) override;

private:
    std::unique_ptr<IPanel> CreatePanel(const FPanelOpenRequest& Request);
    IPanel* FindMatchingPanel(const FPanelOpenRequest& Request) const;

    FEditorContext* Context = nullptr;
    TArray<std::unique_ptr<IPanel>> Panels;
    TMap<std::type_index, std::function<std::unique_ptr<IPanel>()>> PanelFactories;
};
