#pragma once

#include "Core/CoreMinimal.h"

#include <typeindex>

struct FEditorContext;
struct FPanelOpenRequest;
class IPanel;

enum class EPanelOpenPolicy
{
    // 이미 열린 같은 패널이 있으면 재사용하고, 없으면 새로 만듭니다.
    ReuseIfOpenElseCreate,
    // 이미 열린 같은 패널이 있으면 그 패널을 다시 보여 주고, 없으면 새로 만듭니다.
    FocusIfOpenElseCreate,
    // 항상 새 패널 인스턴스를 만듭니다.
    AlwaysCreateNew
};

struct FPanelOpenRequest
{
    // 어떤 패널 타입을 열거나 찾을지 구분하는 키입니다.
    std::type_index PanelType = std::type_index(typeid(void));
    // 같은 타입 안에서도 문맥별로 서로 다른 패널 인스턴스를 구분할 때 씁니다.
    FString ContextKey;
    // 기존 패널을 재사용할지 새로 만들지 정합니다.
    EPanelOpenPolicy OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate;
};

struct FPanelDescriptor
{
    // 패널 타입별 메뉴/생성 정보를 연결하기 위한 런타임 식별자입니다.
    std::type_index PanelType = std::type_index(typeid(void));
    // 패널 자체의 고정 ID입니다.
    FWString PanelId;
    // UI 탭과 Window 메뉴 등에 표시할 이름입니다.
    FWString DisplayName;
    // Window 메뉴에서 사용할 서브메뉴 경로입니다. 비어 있으면 루트에 바로 노출됩니다.
    FWString WindowMenuPath;
    // 같은 메뉴 그룹 안에서의 정렬 우선순위입니다.
    int32 WindowMenuOrder = 0;
    // Window 메뉴에 자동으로 노출할지 여부입니다.
    bool bShowInWindowMenu = true;
};

class IPanelService
{
public:
    virtual ~IPanelService() = default;

    // 요청에 맞는 패널을 열거나 필요하면 생성해서 반환합니다.
    virtual IPanel* OpenPanel(const FPanelOpenRequest& Request) = 0;
    // 요청과 일치하는 기존 패널을 찾습니다. 생성은 하지 않습니다.
    virtual IPanel* FindPanel(const FPanelOpenRequest& Request) = 0;
    // 요청과 일치하는 패널이 있으면 닫습니다.
    virtual void ClosePanel(const FPanelOpenRequest& Request) = 0;
    // 요청과 일치하는 패널의 열림 상태를 토글합니다.
    virtual void TogglePanel(const FPanelOpenRequest& Request) = 0;

    template <typename TPanel>
    TPanel* OpenPanel(const FString& ContextKey = {},
                      EPanelOpenPolicy OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate)
    {
        // 템플릿 패널 타입을 FPanelOpenRequest 형태로 감싸는 편의 함수입니다.
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        Request.OpenPolicy = OpenPolicy;
        return static_cast<TPanel*>(OpenPanel(Request));
    }

    template <typename TPanel>
    TPanel* FindPanel(const FString& ContextKey = {})
    {
        // 타입과 문맥 키로 기존 패널만 찾는 편의 함수입니다.
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        return static_cast<TPanel*>(FindPanel(Request));
    }

    template <typename TPanel>
    void ClosePanel(const FString& ContextKey = {})
    {
        // 타입과 문맥 키에 해당하는 패널을 닫는 편의 함수입니다.
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        ClosePanel(Request);
    }

    template <typename TPanel>
    void TogglePanel(const FString& ContextKey = {})
    {
        // 타입과 문맥 키에 해당하는 패널의 열림 상태를 뒤집는 편의 함수입니다.
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        TogglePanel(Request);
    }
};

class IPanel
{
public:
    IPanel() = default;
    virtual ~IPanel() = default;

    // 패널이 에디터 컨텍스트와 PanelService를 참조할 수 있게 연결합니다.
    virtual void Initialize(FEditorContext* InContext, IPanelService* InPanelService);
    // 패널이 제거되기 직전에 필요한 정리 작업을 합니다.
    virtual void ShutDown() {}

    // 패널을 고유하게 식별하는 ID를 반환합니다.
    virtual const wchar_t* GetPanelID() const = 0;
    // UI 탭 제목과 Window 메뉴에 표시할 이름입니다.
    virtual const wchar_t* GetDisplayName() const = 0;
    // 에디터 시작 시 기본으로 열어 둘 패널인지 결정합니다.
    virtual bool ShouldOpenByDefault() const { return false; }
    // Window 메뉴에 자동 등록할지 결정합니다.
    virtual bool ShouldShowInWindowMenu() const { return true; }
    // Window 메뉴에서 들어갈 서브메뉴 경로를 반환합니다.
    virtual FWString GetWindowMenuPath() const { return {}; }
    // Window 메뉴에서의 정렬 순서를 반환합니다.
    virtual int32 GetWindowMenuOrder() const { return 0; }

    // 닫혀 있던 패널이 열릴 때 한 번 호출됩니다.
    virtual void OnOpen() {}
    // 열려 있던 패널이 닫힐 때 한 번 호출됩니다.
    virtual void OnClose() {}
    // OpenPanel 요청으로 전달된 문맥 키 등을 패널 내부 상태에 반영합니다.
    virtual void ApplyOpenRequest(const FPanelOpenRequest& Request);
    // 현재 패널이 요청과 같은 타입/문맥을 가리키는지 판정합니다.
    virtual bool MatchesRequest(const FPanelOpenRequest& Request) const;
    // 패널이 열려 있는 동안 프레임마다 호출되는 갱신 함수입니다.
    virtual void Tick(float DeltaTime) {}
    // 패널 UI를 실제로 그리는 핵심 함수입니다.
    virtual void Draw() = 0;

    // 현재 패널이 열려 있는지 조회합니다.
    bool IsOpen() const { return bOpen; }
    // 패널 인스턴스를 구분하는 현재 문맥 키를 반환합니다.
    const FString& GetContextKey() const { return ContextKey; }
    // 열림 상태를 바꾸고, 필요하면 OnOpen/OnClose를 호출합니다.
    void SetOpen(bool bInOpen);
    // 현재 열림 상태를 반전합니다.
    void ToggleOpen();

protected:
    // 패널이 에디터 전역 상태를 읽을 때 사용하는 컨텍스트입니다.
    FEditorContext* GetContext() const { return Context; }
    // 다른 패널을 열거나 닫을 때 사용하는 서비스입니다.
    IPanelService* GetPanelService() const { return PanelService; }

protected:
    FEditorContext* Context = nullptr;
    IPanelService* PanelService = nullptr;
    FString ContextKey;

private:
    bool bOpen = false;
};
