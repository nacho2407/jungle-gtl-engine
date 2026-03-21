#pragma once

#include "Core/Containers/String.h"

#include <typeindex>

struct FEditorContext;
struct FPanelOpenRequest;
class IPanel;

enum class EPanelOpenPolicy
{
    ReuseIfOpenElseCreate,
    FocusIfOpenElseCreate,
    AlwaysCreateNew
};

struct FPanelOpenRequest
{
    std::type_index PanelType = std::type_index(typeid(void));
    FString ContextKey;
    EPanelOpenPolicy OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate;
};

class IPanelService
{
public:
    virtual ~IPanelService() = default;

    virtual IPanel* OpenPanel(const FPanelOpenRequest& Request) = 0;
    virtual IPanel* FindPanel(const FPanelOpenRequest& Request) = 0;
    virtual void ClosePanel(const FPanelOpenRequest& Request) = 0;
    virtual void TogglePanel(const FPanelOpenRequest& Request) = 0;

    template <typename TPanel>
    TPanel* OpenPanel(const FString& ContextKey = {}, EPanelOpenPolicy OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate)
    {
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        Request.OpenPolicy = OpenPolicy;
        return static_cast<TPanel*>(OpenPanel(Request));
    }

    template <typename TPanel>
    TPanel* FindPanel(const FString& ContextKey = {})
    {
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        return static_cast<TPanel*>(FindPanel(Request));
    }

    template <typename TPanel>
    void ClosePanel(const FString& ContextKey = {})
    {
        FPanelOpenRequest Request;
        Request.PanelType = std::type_index(typeid(TPanel));
        Request.ContextKey = ContextKey;
        ClosePanel(Request);
    }

    template <typename TPanel>
    void TogglePanel(const FString& ContextKey = {})
    {
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

	virtual void Initialize(FEditorContext* InContext, IPanelService* InPanelService);
	virtual void ShutDown() {}

	virtual const wchar_t* GetPanelID() const = 0;
	virtual const wchar_t* GetDisplayName() const = 0;
	virtual bool ShouldOpenByDefault() const { return false; }

	virtual void OnOpen() {}
	virtual void OnClose() {}
	virtual void ApplyOpenRequest(const FPanelOpenRequest& Request);
	virtual bool MatchesRequest(const FPanelOpenRequest& Request) const;
	virtual void Tick(float DeltaTime) {}
	virtual void Draw() = 0;

	bool IsOpen() const { return bOpen; }
	const FString& GetContextKey() const { return ContextKey; }
	void SetOpen(bool bInOpen);
	void ToggleOpen();

protected:
	FEditorContext* GetContext() const { return Context; }
	IPanelService* GetPanelService() const { return PanelService; }

protected:
	FEditorContext* Context = nullptr;
	IPanelService* PanelService = nullptr;
	FString ContextKey;
private:
	bool bOpen = false;
};
