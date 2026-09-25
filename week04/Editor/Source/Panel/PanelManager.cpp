#include "PanelManager.h"

#include <cwchar>

void FPanelManager::Initialize(FEditorContext* InContext)
{
    // 이후 생성되는 모든 패널이 같은 EditorContext를 참조하도록 저장합니다.
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
    PanelDescriptors.clear();
    PanelDescriptorRegisteredCallback = nullptr;
}

void FPanelManager::RegisterPanel(std::unique_ptr<IPanel> Panel)
{
    if (!Panel)
    {
        return;
    }

    // Window 메뉴 자동 등록 등에 필요한 descriptor를 먼저 수집합니다.
    RegisterPanelDescriptor(std::type_index(typeid(*Panel)), *Panel);
    Panel->Initialize(Context, this);
    if (Panel->ShouldOpenByDefault())
    {
        // 기본 열림 패널은 등록 직후 바로 열린 상태로 맞춥니다.
        Panel->SetOpen(true);
    }

    Panels.push_back(std::move(Panel));
}

void FPanelManager::SetPanelDescriptorRegisteredCallback(
    FPanelDescriptorRegisteredCallback InCallback)
{
    PanelDescriptorRegisteredCallback = std::move(InCallback);
}

void FPanelManager::Tick(float DeltaTime)
{
    for (auto& Panel : Panels)
    {
        if (Panel->IsOpen())
        {
            // 보이지 않는 패널은 갱신 비용을 쓰지 않도록 열려 있는 패널만 Tick합니다.
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
            // 열린 패널만 실제 ImGui 창을 그립니다.
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
            // 재사용 가능한 기존 패널이 있으면 그 패널을 다시 열어 씁니다.
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

    // 새 패널은 등록 후 요청 문맥을 반영하고 열린 상태로 반환합니다.
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
        // 기존 패널이 있으면 그 패널의 열림 상태만 뒤집습니다.
        ExistingPanel->ToggleOpen();
        return;
    }

    // 기존 패널이 없으면 새로 열어서 토글 요청을 만족시킵니다.
    OpenPanel(Request);
}

std::unique_ptr<IPanel> FPanelManager::CreatePanel(const FPanelOpenRequest& Request)
{
    const auto FactoryIt = PanelFactories.find(Request.PanelType);
    if (FactoryIt == PanelFactories.end())
    {
        return nullptr;
    }

    // 타입 등록 시 저장해 둔 팩토리로 실제 패널 인스턴스를 생성합니다.
    return FactoryIt->second();
}

IPanel* FPanelManager::FindMatchingPanel(const FPanelOpenRequest& Request) const
{
    for (const auto& Panel : Panels)
    {
        if (Panel->MatchesRequest(Request))
        {
            // 같은 타입과 문맥 키를 가진 첫 번째 패널을 반환합니다.
            return Panel.get();
        }
    }

    return nullptr;
}

void FPanelManager::RegisterPanelDescriptor(const std::type_index& PanelType, const IPanel& Panel)
{
    for (FPanelDescriptor& ExistingDescriptor : PanelDescriptors)
    {
        if (ExistingDescriptor.PanelType != PanelType)
        {
            continue;
        }

        // 같은 타입 descriptor가 이미 있으면 최신 패널 메타데이터로 덮어씁니다.
        ExistingDescriptor.PanelId = Panel.GetPanelID() != nullptr ? Panel.GetPanelID() : L"";
        ExistingDescriptor.DisplayName =
            Panel.GetDisplayName() != nullptr ? Panel.GetDisplayName() : L"";
        ExistingDescriptor.WindowMenuPath = Panel.GetWindowMenuPath();
        ExistingDescriptor.WindowMenuOrder = Panel.GetWindowMenuOrder();
        ExistingDescriptor.bShowInWindowMenu = Panel.ShouldShowInWindowMenu();
        return;
    }

    FPanelDescriptor Descriptor;
    Descriptor.PanelType = PanelType;
    Descriptor.PanelId = Panel.GetPanelID() != nullptr ? Panel.GetPanelID() : L"";
    Descriptor.DisplayName = Panel.GetDisplayName() != nullptr ? Panel.GetDisplayName() : L"";
    Descriptor.WindowMenuPath = Panel.GetWindowMenuPath();
    Descriptor.WindowMenuOrder = Panel.GetWindowMenuOrder();
    Descriptor.bShowInWindowMenu = Panel.ShouldShowInWindowMenu();
    PanelDescriptors.push_back(Descriptor);

    if (PanelDescriptorRegisteredCallback)
    {
        // 새 descriptor가 생기면 Editor가 Window 메뉴 항목을 만들 수 있게 알립니다.
        PanelDescriptorRegisteredCallback(Descriptor);
    }
}
