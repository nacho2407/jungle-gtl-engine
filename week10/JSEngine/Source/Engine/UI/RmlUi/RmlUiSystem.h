#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Runtime/ViewportRect.h"
#include "UI/RuntimeUITypes.h"
#include "UI/RmlUi/RmlUiRenderInterfaceD3D11.h"
#include "UI/RmlUi/RmlUiRuntimeModule.h"

#include <utility>

class FRenderer;
class FWindowsWindow;
class InputSystem;
class FRmlUiSystemActionEventListener;

namespace Rml
{
    class Context;
    class Element;
    class ElementDocument;
}

class FRmlUiSystem
{
public:
    bool Initialize(FRenderer& Renderer, const char* ContextName, int32 LayoutWidth = 1920, int32 LayoutHeight = 1080);
    void Shutdown();
    bool IsInitialized() const { return bInitialized; }
    size_t GetDocumentCount() const { return DocumentsByScreenId.size(); }
    int32 GetVisibleDocumentCount() const;

    bool LoadDocument(const FString& ScreenId, const FString& Path);
    bool UnloadDocument(const FString& ScreenId);
    bool ReloadDocument(const FString& ScreenId);
    void UnloadAllDocuments();
    void UnloadGameplayDocuments();

    bool ShowScreen(const FString& ScreenId);
    bool HideScreen(const FString& ScreenId);

    bool HasElement(const FString& ElementId) const;
    FString GetElementText(const FString& ElementId) const;
    bool SetElementText(const FString& ElementId, const FString& Text);
    FString GetElementValue(const FString& ElementId) const;
    bool SetElementValue(const FString& ElementId, const FString& Value);
    bool SetElementVisible(const FString& ElementId, bool bVisible);
    bool SetElementEnabled(const FString& ElementId, bool bEnabled);
    bool SetElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled);
    bool HasElementClass(const FString& ElementId, const FString& ClassName) const;
    FString GetElementClassNames(const FString& ElementId) const;
    bool SetElementClassNames(const FString& ElementId, const FString& ClassNames);
    bool HasElementAttribute(const FString& ElementId, const FString& Name) const;
    FString GetElementAttribute(const FString& ElementId, const FString& Name) const;
    bool SetElementAttribute(const FString& ElementId, const FString& Name, const FString& Value);
    bool RemoveElementAttribute(const FString& ElementId, const FString& Name);
    FString GetElementStyle(const FString& ElementId, const FString& Name) const;
    bool SetElementStyle(const FString& ElementId, const FString& Name, const FString& Value);
    bool RemoveElementStyle(const FString& ElementId, const FString& Name);
    bool FocusElement(const FString& ElementId, bool bFocusVisible);
    bool IsElementFocused(const FString& ElementId) const;
    bool BlurElement(const FString& ElementId);
    bool ClickElement(const FString& ElementId);

    TArray<FString> PollActionEvents();
    TArray<FString> PollPreviewActionEvents();

    void Render(const FRuntimeUIRenderContext& Context, FRenderer& Renderer);
    bool PumpViewportInput(
        InputSystem& Input,
        FWindowsWindow* Window,
        bool bAllowRuntimeUIInput,
        const FViewportRect& ViewportRect,
        int32 LayoutWidth = 0,
        int32 LayoutHeight = 0,
        bool bPreviewDocumentOnly = false);
    bool PumpGameInput(
        InputSystem& Input,
        FWindowsWindow* Window,
        bool bAllowRuntimeUIInput,
        int32 LayoutWidth = 1920,
        int32 LayoutHeight = 1080);

private:
    void EnqueueActionEvent(const FString& EventName, Rml::ElementDocument* SourceDocument);
    Rml::ElementDocument* FindDocument(const FString& ScreenId) const;
    Rml::Element* FindElement(const FString& ElementId) const;
    void AttachDocumentListeners(Rml::ElementDocument* Document);
    int GetKeyModifierState(const InputSystem& Input) const;
    TArray<std::pair<Rml::ElementDocument*, bool>> ApplyDocumentVisibilityFilter(bool bPreviewDocumentOnly);
    void RestoreDocumentVisibility(const TArray<std::pair<Rml::ElementDocument*, bool>>& VisibilitySnapshot);

private:
    FString ContextName;
    bool bInitialized = false;
    FRmlUiRuntimeModule RuntimeModule;
    FRmlUiRenderInterfaceD3D11 RenderInterface;
    Rml::Context* Context = nullptr;
    TMap<FString, FString> DocumentPathByScreenId;
    TMap<FString, Rml::ElementDocument*> DocumentsByScreenId;
    TArray<FString> PendingActionEvents;
    TArray<FString> PreviewPendingActionEvents;
    FRmlUiSystemActionEventListener* ActionListener = nullptr;

    static constexpr const char* RuntimeUIPreviewScreenId = "__RuntimeUIPreview";

    friend class FRmlUiSystemActionEventListener;
};
