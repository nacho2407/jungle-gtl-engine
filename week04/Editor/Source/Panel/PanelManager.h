#pragma once

#include "Core/CoreMinimal.h"

#include "Panel.h"


#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

struct FEditorContext;

class FPanelManager : public IPanelService
{
public:
    using FPanelDescriptorRegisteredCallback = std::function<void(const FPanelDescriptor&)>;

    // PanelManager가 에디터 컨텍스트를 참조할 수 있도록 초기 연결을 합니다.
    void Initialize(FEditorContext* InContext);
    // 등록된 패널과 팩토리, descriptor 캐시를 모두 정리합니다.
    void Shutdown();

    // 이미 만들어진 패널 인스턴스를 매니저에 등록합니다.
    void RegisterPanel(std::unique_ptr<IPanel> Panel);
    // 새 패널 descriptor가 생겼을 때 Editor 쪽으로 알릴 콜백을 설정합니다.
    void SetPanelDescriptorRegisteredCallback(FPanelDescriptorRegisteredCallback InCallback);
    // 현재 매니저가 알고 있는 패널 메타데이터 목록을 반환합니다.
    const TArray<FPanelDescriptor>& GetPanelDescriptors() const { return PanelDescriptors; }

    template <typename TPanel, typename... TArgs>
    TPanel* RegisterPanelInstance(TArgs&&... Args)
    {
        static_assert(std::is_base_of_v<IPanel, TPanel>);

        // 패널을 즉시 생성해서 바로 사용할 때 쓰는 등록 경로입니다.
        auto Panel = std::make_unique<TPanel>(std::forward<TArgs>(Args)...);
        TPanel* RawPanel = Panel.get();
        RegisterPanel(std::move(Panel));
        return RawPanel;
    }

    template <typename TPanel>
    void RegisterPanelType()
    {
        static_assert(std::is_base_of_v<IPanel, TPanel>);
        // 실제 열기 요청이 들어왔을 때 새 패널을 만들 수 있도록 팩토리를 등록합니다.
        PanelFactories[std::type_index(typeid(TPanel))] = []()
        {
            return std::make_unique<TPanel>();
        };

        // Window 메뉴 자동 구성을 위해 타입만 등록해도 descriptor를 미리 수집합니다.
        TPanel ProbePanel;
        RegisterPanelDescriptor(std::type_index(typeid(TPanel)), ProbePanel);
    }

    // 열려 있는 패널만 프레임 갱신합니다.
    void Tick(float DeltaTime);
    // 열려 있는 패널만 실제로 그립니다.
    void DrawPanels();

    // 고정 PanelID로 이미 생성된 패널을 찾습니다.
    IPanel* FindPanelById(const wchar_t* PanelId);

    // IPanelService 구현: 요청에 맞는 패널을 열거나 생성합니다.
    IPanel* OpenPanel(const FPanelOpenRequest& Request) override;
    // IPanelService 구현: 요청과 일치하는 기존 패널만 찾습니다.
    IPanel* FindPanel(const FPanelOpenRequest& Request) override;
    // IPanelService 구현: 요청과 일치하는 패널을 닫습니다.
    void ClosePanel(const FPanelOpenRequest& Request) override;
    // IPanelService 구현: 요청과 일치하는 패널을 열거나 닫습니다.
    void TogglePanel(const FPanelOpenRequest& Request) override;

private:
    // 등록된 팩토리로 새 패널 인스턴스를 만듭니다.
    std::unique_ptr<IPanel> CreatePanel(const FPanelOpenRequest& Request);
    // 요청과 같은 타입/문맥을 가진 기존 패널을 찾습니다.
    IPanel* FindMatchingPanel(const FPanelOpenRequest& Request) const;
    // 패널의 메뉴/표시용 메타데이터를 저장하고 필요하면 Editor에 알립니다.
    void RegisterPanelDescriptor(const std::type_index& PanelType, const IPanel& Panel);

    FEditorContext* Context = nullptr;
    TArray<std::unique_ptr<IPanel>> Panels;
    TMap<std::type_index, std::function<std::unique_ptr<IPanel>()>> PanelFactories;
    TArray<FPanelDescriptor> PanelDescriptors;
    FPanelDescriptorRegisteredCallback PanelDescriptorRegisteredCallback;
};
