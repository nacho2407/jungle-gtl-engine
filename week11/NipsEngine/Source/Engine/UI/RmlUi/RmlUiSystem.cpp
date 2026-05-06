#include "UI/RmlUi/RmlUiSystem.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Renderer/Renderer.h"

#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetNextSibling
#undef GetNextSibling
#endif
#include "RmlUi/Core.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Elements/ElementFormControl.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Factory.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/Types.h"

#include <algorithm>
#include <chrono>
#include <windows.h>

namespace
{
    struct FScaledRuntimeUIViewport
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 1.0f;
        float Height = 1.0f;
        float Scale = 1.0f;
    };

    FScaledRuntimeUIViewport CalculateScaledRuntimeUIViewport(
        float ViewportX,
        float ViewportY,
        float ViewportWidth,
        float ViewportHeight,
        int LayoutWidth,
        int LayoutHeight)
    {
        const float SafeViewportWidth = std::max(ViewportWidth, 1.0f);
        const float SafeViewportHeight = std::max(ViewportHeight, 1.0f);
        const float SafeLayoutWidth = static_cast<float>(std::max(LayoutWidth, 1));
        const float SafeLayoutHeight = static_cast<float>(std::max(LayoutHeight, 1));
        const float Scale = std::min(SafeViewportWidth / SafeLayoutWidth, SafeViewportHeight / SafeLayoutHeight);
        const float ScaledWidth = SafeLayoutWidth * Scale;
        const float ScaledHeight = SafeLayoutHeight * Scale;

        FScaledRuntimeUIViewport Result;
        Result.X = ViewportX + (SafeViewportWidth - ScaledWidth) * 0.5f;
        Result.Y = ViewportY + (SafeViewportHeight - ScaledHeight) * 0.5f;
        Result.Width = ScaledWidth;
        Result.Height = ScaledHeight;
        Result.Scale = std::max(Scale, 0.0001f);
        return Result;
    }

    bool IsMouseButtonVK(int VK)
    {
        return VK == VK_LBUTTON
            || VK == VK_RBUTTON
            || VK == VK_MBUTTON
            || VK == VK_XBUTTON1
            || VK == VK_XBUTTON2;
    }

    void SetDocumentVisibleIfNeeded(Rml::ElementDocument* Document, bool bVisible)
    {
        if (!Document || Document->IsVisible() == bVisible)
        {
            return;
        }

        if (bVisible)
        {
            Document->Show();
        }
        else
        {
            Document->Hide();
        }
    }

    Rml::Input::KeyIdentifier MapVirtualKeyToRmlKey(int VK)
    {
        using namespace Rml::Input;

        if (VK >= '0' && VK <= '9')
        {
            return static_cast<KeyIdentifier>(KI_0 + (VK - '0'));
        }
        if (VK >= 'A' && VK <= 'Z')
        {
            return static_cast<KeyIdentifier>(KI_A + (VK - 'A'));
        }
        if (VK >= VK_F1 && VK <= VK_F24)
        {
            return static_cast<KeyIdentifier>(KI_F1 + (VK - VK_F1));
        }
        if (VK >= VK_NUMPAD0 && VK <= VK_NUMPAD9)
        {
            return static_cast<KeyIdentifier>(KI_NUMPAD0 + (VK - VK_NUMPAD0));
        }

        switch (VK)
        {
        case VK_SPACE: return KI_SPACE;
        case VK_BACK: return KI_BACK;
        case VK_TAB: return KI_TAB;
        case VK_RETURN: return KI_RETURN;
        case VK_ESCAPE: return KI_ESCAPE;
        case VK_PRIOR: return KI_PRIOR;
        case VK_NEXT: return KI_NEXT;
        case VK_END: return KI_END;
        case VK_HOME: return KI_HOME;
        case VK_LEFT: return KI_LEFT;
        case VK_UP: return KI_UP;
        case VK_RIGHT: return KI_RIGHT;
        case VK_DOWN: return KI_DOWN;
        case VK_INSERT: return KI_INSERT;
        case VK_DELETE: return KI_DELETE;
        case VK_SHIFT: return KI_LSHIFT;
        case VK_LSHIFT: return KI_LSHIFT;
        case VK_RSHIFT: return KI_RSHIFT;
        case VK_CONTROL: return KI_LCONTROL;
        case VK_LCONTROL: return KI_LCONTROL;
        case VK_RCONTROL: return KI_RCONTROL;
        case VK_MENU: return KI_LMENU;
        case VK_LMENU: return KI_LMENU;
        case VK_RMENU: return KI_RMENU;
        case VK_OEM_1: return KI_OEM_1;
        case VK_OEM_PLUS: return KI_OEM_PLUS;
        case VK_OEM_COMMA: return KI_OEM_COMMA;
        case VK_OEM_MINUS: return KI_OEM_MINUS;
        case VK_OEM_PERIOD: return KI_OEM_PERIOD;
        case VK_OEM_2: return KI_OEM_2;
        case VK_OEM_3: return KI_OEM_3;
        case VK_OEM_4: return KI_OEM_4;
        case VK_OEM_5: return KI_OEM_5;
        case VK_OEM_6: return KI_OEM_6;
        case VK_OEM_7: return KI_OEM_7;
        case VK_MULTIPLY: return KI_MULTIPLY;
        case VK_ADD: return KI_ADD;
        case VK_SEPARATOR: return KI_SEPARATOR;
        case VK_SUBTRACT: return KI_SUBTRACT;
        case VK_DECIMAL: return KI_DECIMAL;
        case VK_DIVIDE: return KI_DIVIDE;
        case VK_PAUSE: return KI_PAUSE;
        case VK_CAPITAL: return KI_CAPITAL;
        case VK_NUMLOCK: return KI_NUMLOCK;
        case VK_SCROLL: return KI_SCROLL;
        default: return KI_UNKNOWN;
        }
    }

    bool IsElementOrAncestorFormControl(Rml::Element* Element)
    {
        for (Rml::Element* Current = Element; Current != nullptr; Current = Current->GetParentNode())
        {
            if (rmlui_dynamic_cast<Rml::ElementFormControl*>(Current) != nullptr)
            {
                return true;
            }
        }
        return false;
    }

    bool IsElementOrDescendantFocused(Rml::Element* Element, Rml::Element* FocusedElement)
    {
        if (!Element || !FocusedElement)
        {
            return false;
        }

        for (Rml::Element* Current = FocusedElement; Current != nullptr; Current = Current->GetParentNode())
        {
            if (Current == Element)
            {
                return true;
            }
        }
        return false;
    }

    void CaptureTextInputForFocusedRmlElement(Rml::Element* Element)
    {
        if (!IsElementOrAncestorFormControl(Element))
        {
            return;
        }

        InputSystem& Input = InputSystem::Get();
        Input.SetGuiKeyboardCapture(true);
        Input.SetGuiTextInputCapture(true);
        Input.SetGuiViewportMouseBlock(true);
    }
}

class FRmlUiSystemActionEventListener final : public Rml::EventListener
{
public:
    explicit FRmlUiSystemActionEventListener(FRmlUiSystem* InOwner)
        : Owner(InOwner)
    {
    }

    void ProcessEvent(Rml::Event& Event) override
    {
        if (!Owner)
        {
            return;
        }

        Rml::Element* Element = Event.GetTargetElement();
        while (Element)
        {
            Rml::String Action = Element->GetAttribute<Rml::String>("data-action", "");
            if (Action.empty())
            {
                Action = Element->GetAttribute<Rml::String>("action", "");
            }

            if (!Action.empty())
            {
                Owner->EnqueueActionEvent(Action, Element->GetOwnerDocument());
                return;
            }

            Element = Element->GetParentNode();
        }
    }

private:
    FRmlUiSystem* Owner = nullptr;
};

bool FRmlUiSystem::Initialize(FRenderer& Renderer, const char* InContextName, int32 LayoutWidth, int32 LayoutHeight)
{
    if (bInitialized)
    {
        return true;
    }

    ContextName = (InContextName && InContextName[0] != '\0') ? InContextName : "RuntimeUI";

    if (!RenderInterface.Initialize(
        Renderer.GetFD3DDevice().GetDevice(),
        Renderer.GetFD3DDevice().GetDeviceContext()))
    {
        UE_LOG_ERROR("[RmlUi] Failed to initialize render interface. Context=%s", ContextName.c_str());
        return false;
    }

    if (!RuntimeModule.Initialize())
    {
        UE_LOG_ERROR("[RmlUi] Failed to initialize runtime module. Context=%s", ContextName.c_str());
        RenderInterface.Shutdown();
        return false;
    }

    Context = Rml::CreateContext(
        ContextName,
        Rml::Vector2i(std::max(LayoutWidth, 1), std::max(LayoutHeight, 1)),
        &RenderInterface);
    if (!Context)
    {
        UE_LOG_ERROR("[RmlUi] Failed to create context: %s", ContextName.c_str());
        RuntimeModule.Shutdown();
        RenderInterface.Shutdown();
        return false;
    }

    delete ActionListener;
    ActionListener = new FRmlUiSystemActionEventListener(this);
    bInitialized = true;
    return true;
}

void FRmlUiSystem::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    if (Context)
    {
        Context->UnloadAllDocuments();
        Context->Update();
        RuntimeModule.ReleaseCachedFontRenderResources();
        Rml::RemoveContext(ContextName);
        Context = nullptr;
    }

    delete ActionListener;
    ActionListener = nullptr;
    PendingActionEvents.clear();
    PreviewPendingActionEvents.clear();
    DocumentsByScreenId.clear();
    DocumentPathByScreenId.clear();
    RuntimeModule.Shutdown();
    RenderInterface.Shutdown();
    bInitialized = false;
}

int32 FRmlUiSystem::GetVisibleDocumentCount() const
{
    int32 VisibleCount = 0;
    for (const auto& Pair : DocumentsByScreenId)
    {
        if (Pair.second && Pair.second->IsVisible())
        {
            ++VisibleCount;
        }
    }
    return VisibleCount;
}

bool FRmlUiSystem::LoadDocument(const FString& ScreenId, const FString& Path)
{
    const auto StartTime = std::chrono::steady_clock::now();
    if (!bInitialized || !Context || ScreenId.empty() || Path.empty())
    {
        return false;
    }

    UnloadDocument(ScreenId);
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    Rml::ElementDocument* Document = Context->LoadDocument(Path);
    if (!Document)
    {
        UE_LOG_ERROR("[RmlUi] Failed to load document. Screen=%s Path=%s", ScreenId.c_str(), Path.c_str());
        return false;
    }

    AttachDocumentListeners(Document);
    Document->Show();
    DocumentsByScreenId[ScreenId] = Document;
    DocumentPathByScreenId[ScreenId] = Path;

    const double ElapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - StartTime).count();
    UE_LOG("[RmlUiPerf] Loaded document. Context=%s Screen=%s Path=%s Time=%.4fs",
        ContextName.c_str(), ScreenId.c_str(), Path.c_str(), ElapsedSec);
    return true;
}

bool FRmlUiSystem::UnloadDocument(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindDocument(ScreenId);
    if (!Document || !Context)
    {
        return false;
    }

    Context->UnloadDocument(Document);
    Context->Update();
    DocumentsByScreenId.erase(ScreenId);
    DocumentPathByScreenId.erase(ScreenId);
    if (ScreenId == RuntimeUIPreviewScreenId)
    {
        PreviewPendingActionEvents.clear();
    }
    return true;
}

bool FRmlUiSystem::ReloadDocument(const FString& ScreenId)
{
    auto It = DocumentPathByScreenId.find(ScreenId);
    if (It == DocumentPathByScreenId.end())
    {
        return false;
    }

    const FString Path = It->second;
    UnloadDocument(ScreenId);
    return LoadDocument(ScreenId, Path);
}

void FRmlUiSystem::UnloadAllDocuments()
{
    if (!Context)
    {
        return;
    }

    TArray<FString> ScreenIds;
    for (const auto& Pair : DocumentsByScreenId)
    {
        ScreenIds.push_back(Pair.first);
    }

    for (const FString& ScreenId : ScreenIds)
    {
        UnloadDocument(ScreenId);
    }

    DocumentPathByScreenId.clear();
    PendingActionEvents.clear();
    PreviewPendingActionEvents.clear();
}

void FRmlUiSystem::UnloadGameplayDocuments()
{
    if (!Context)
    {
        return;
    }

    TArray<FString> ScreenIds;
    for (const auto& Pair : DocumentsByScreenId)
    {
        if (Pair.first != RuntimeUIPreviewScreenId)
        {
            ScreenIds.push_back(Pair.first);
        }
    }

    for (const FString& ScreenId : ScreenIds)
    {
        UnloadDocument(ScreenId);
        DocumentPathByScreenId.erase(ScreenId);
    }

    PendingActionEvents.clear();
}

bool FRmlUiSystem::ShowScreen(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindDocument(ScreenId);
    if (!Document)
    {
        return false;
    }

    Document->Show();
    return true;
}

bool FRmlUiSystem::HideScreen(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindDocument(ScreenId);
    if (!Document)
    {
        return false;
    }

    Document->Hide();
    return true;
}

bool FRmlUiSystem::HasElement(const FString& ElementId) const
{
    return FindElement(ElementId) != nullptr;
}

FString FRmlUiSystem::GetElementText(const FString& ElementId) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element ? Element->GetInnerRML() : "";
}

bool FRmlUiSystem::SetElementText(const FString& ElementId, const FString& Text)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (Element->GetInnerRML() == Text)
    {
        return true;
    }
    Element->SetInnerRML(Text);
    return true;
}

FString FRmlUiSystem::GetElementValue(const FString& ElementId) const
{
    Rml::Element* Element = FindElement(ElementId);
    if (Rml::ElementFormControl* Control = Element ? rmlui_dynamic_cast<Rml::ElementFormControl*>(Element) : nullptr)
    {
        return Control->GetValue();
    }
    return GetElementAttribute(ElementId, "value");
}

bool FRmlUiSystem::SetElementValue(const FString& ElementId, const FString& Value)
{
    Rml::Element* Element = FindElement(ElementId);
    if (Rml::ElementFormControl* Control = Element ? rmlui_dynamic_cast<Rml::ElementFormControl*>(Element) : nullptr)
    {
        if (Control->GetValue() == Value)
        {
            return true;
        }
        Control->SetValue(Value);
        return true;
    }
    return SetElementAttribute(ElementId, "value", Value);
}

bool FRmlUiSystem::SetElementVisible(const FString& ElementId, bool bVisible)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    const Rml::Property* Display = Element->GetProperty("display");
    const bool bCurrentlyHidden = Display && Display->ToString() == "none";
    if (bVisible)
    {
        if (!bCurrentlyHidden)
        {
            return true;
        }
        Element->RemoveProperty("display");
    }
    else
    {
        if (bCurrentlyHidden)
        {
            return true;
        }
        Element->SetProperty("display", "none");
    }
    return true;
}

bool FRmlUiSystem::SetElementEnabled(const FString& ElementId, bool bEnabled)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    const bool bCurrentlyDisabled = Element->HasAttribute("disabled") || Element->IsClassSet("disabled");
    if (bCurrentlyDisabled == !bEnabled)
    {
        return true;
    }
    if (bEnabled)
    {
        Element->RemoveAttribute("disabled");
    }
    else
    {
        Element->SetAttribute("disabled", "disabled");
    }
    Element->SetClass("disabled", !bEnabled);
    return true;
}

bool FRmlUiSystem::SetElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (Element->IsClassSet(ClassName) == bEnabled)
    {
        return true;
    }
    Element->SetClass(ClassName, bEnabled);
    return true;
}

bool FRmlUiSystem::HasElementClass(const FString& ElementId, const FString& ClassName) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element ? Element->IsClassSet(ClassName) : false;
}

FString FRmlUiSystem::GetElementClassNames(const FString& ElementId) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element ? Element->GetClassNames() : "";
}

bool FRmlUiSystem::SetElementClassNames(const FString& ElementId, const FString& ClassNames)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (Element->GetClassNames() == ClassNames)
    {
        return true;
    }
    Element->SetClassNames(ClassNames);
    return true;
}

bool FRmlUiSystem::HasElementAttribute(const FString& ElementId, const FString& Name) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element ? Element->HasAttribute(Name) : false;
}

FString FRmlUiSystem::GetElementAttribute(const FString& ElementId, const FString& Name) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element ? Element->GetAttribute<Rml::String>(Name, "") : "";
}

bool FRmlUiSystem::SetElementAttribute(const FString& ElementId, const FString& Name, const FString& Value)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (Element->GetAttribute<Rml::String>(Name, "") == Value)
    {
        return true;
    }
    Element->SetAttribute(Name, Value);
    return true;
}

bool FRmlUiSystem::RemoveElementAttribute(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->RemoveAttribute(Name);
    return true;
}

FString FRmlUiSystem::GetElementStyle(const FString& ElementId, const FString& Name) const
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return "";
    }

    const Rml::Property* Property = Element->GetProperty(Name);
    return Property ? Property->ToString() : "";
}

bool FRmlUiSystem::SetElementStyle(const FString& ElementId, const FString& Name, const FString& Value)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    const Rml::Property* Property = Element->GetProperty(Name);
    if (Property && Property->ToString() == Value)
    {
        return true;
    }
    return Element->SetProperty(Name, Value);
}

bool FRmlUiSystem::RemoveElementStyle(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->RemoveProperty(Name);
    return true;
}

bool FRmlUiSystem::FocusElement(const FString& ElementId, bool bFocusVisible)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    const bool bFocused = Element->Focus(bFocusVisible);
    if (bFocused)
    {
        CaptureTextInputForFocusedRmlElement(Element);
    }
    return bFocused;
}

bool FRmlUiSystem::IsElementFocused(const FString& ElementId) const
{
    Rml::Element* Element = FindElement(ElementId);
    return Element && Context && IsElementOrDescendantFocused(Element, Context->GetFocusElement());
}

bool FRmlUiSystem::BlurElement(const FString& ElementId)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->Blur();
    return true;
}

bool FRmlUiSystem::ClickElement(const FString& ElementId)
{
    Rml::Element* Element = FindElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->Click();
    return true;
}

TArray<FString> FRmlUiSystem::PollActionEvents()
{
    TArray<FString> Events = PendingActionEvents;
    PendingActionEvents.clear();
    return Events;
}

TArray<FString> FRmlUiSystem::PollPreviewActionEvents()
{
    TArray<FString> Events = PreviewPendingActionEvents;
    PreviewPendingActionEvents.clear();
    return Events;
}

void FRmlUiSystem::Render(const FRuntimeUIRenderContext& InContext, FRenderer& Renderer)
{
    if (!bInitialized || !Context || DocumentsByScreenId.empty())
    {
        return;
    }

    const int LayoutWidth = std::max(
        static_cast<int>(InContext.LayoutSize.X > 0.0f ? InContext.LayoutSize.X : InContext.ViewportSize.X),
        1);
    const int LayoutHeight = std::max(
        static_cast<int>(InContext.LayoutSize.Y > 0.0f ? InContext.LayoutSize.Y : InContext.ViewportSize.Y),
        1);
    Context->SetDimensions(Rml::Vector2i(LayoutWidth, LayoutHeight));

    const FScaledRuntimeUIViewport ScaledViewport = CalculateScaledRuntimeUIViewport(
        InContext.ViewportMin.X,
        InContext.ViewportMin.Y,
        InContext.ViewportSize.X,
        InContext.ViewportSize.Y,
        LayoutWidth,
        LayoutHeight);

    Renderer.UseBackBufferRenderTargets();
    RenderInterface.BeginFrame(
        Rml::Vector2f(ScaledViewport.X, ScaledViewport.Y),
        Rml::Vector2f(ScaledViewport.Width, ScaledViewport.Height),
        Rml::Vector2f(ScaledViewport.Scale, ScaledViewport.Scale));

    const TArray<std::pair<Rml::ElementDocument*, bool>> VisibilitySnapshot =
        ApplyDocumentVisibilityFilter(InContext.bPreviewDocumentOnly);
    Context->Update();
    Context->Render();
    RestoreDocumentVisibility(VisibilitySnapshot);
}

bool FRmlUiSystem::PumpViewportInput(
    InputSystem& Input,
    FWindowsWindow* Window,
    bool bAllowRuntimeUIInput,
    const FViewportRect& ViewportRect,
    int32 LayoutWidth,
    int32 LayoutHeight,
    bool bPreviewDocumentOnly)
{
    if (!bInitialized || !Context || !Window || !Window->GetHWND()
        || ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
    {
        return false;
    }

    if (!bAllowRuntimeUIInput)
    {
        Input.ConsumeTextInput();
        return false;
    }

    POINT ClientMousePos = Input.GetMousePos();
    ::ScreenToClient(Window->GetHWND(), &ClientMousePos);

    const bool bInsideViewport =
        ClientMousePos.x >= ViewportRect.X && ClientMousePos.y >= ViewportRect.Y
        && ClientMousePos.x < ViewportRect.X + ViewportRect.Width
        && ClientMousePos.y < ViewportRect.Y + ViewportRect.Height;
    const bool bHasFocusedElement = Context->GetFocusElement() != nullptr;

    if (!bInsideViewport && !Context->IsMouseInteracting() && !bHasFocusedElement)
    {
        return false;
    }

    LayoutWidth = std::max(LayoutWidth > 0 ? LayoutWidth : static_cast<int32>(ViewportRect.Width), 1);
    LayoutHeight = std::max(LayoutHeight > 0 ? LayoutHeight : static_cast<int32>(ViewportRect.Height), 1);
    Context->SetDimensions(Rml::Vector2i(LayoutWidth, LayoutHeight));
    const TArray<std::pair<Rml::ElementDocument*, bool>> VisibilitySnapshot =
        ApplyDocumentVisibilityFilter(bPreviewDocumentOnly);

    const int Modifiers = GetKeyModifierState(Input);
    bool bConsumed = false;

    if (bInsideViewport)
    {
        const int LocalX = static_cast<int>(
            static_cast<float>(ClientMousePos.x - ViewportRect.X) * static_cast<float>(LayoutWidth) / static_cast<float>(ViewportRect.Width));
        const int LocalY = static_cast<int>(
            static_cast<float>(ClientMousePos.y - ViewportRect.Y) * static_cast<float>(LayoutHeight) / static_cast<float>(ViewportRect.Height));
        const bool bMouseFree = Context->ProcessMouseMove(LocalX, LocalY, Modifiers);
        bConsumed = (!bMouseFree && Context->IsMouseInteracting()) || bConsumed;
    }
    else
    {
        const bool bMouseFree = Context->ProcessMouseLeave();
        bConsumed = (!bMouseFree) || bConsumed;
    }

    auto PumpMouseButton = [&](int VK, int ButtonIndex)
    {
        if (Input.GetKeyDown(VK))
        {
            const bool bMouseFree = Context->ProcessMouseButtonDown(ButtonIndex, Modifiers);
            bConsumed = (!bMouseFree) || bConsumed;
        }
        if (Input.GetKeyUp(VK))
        {
            const bool bMouseFree = Context->ProcessMouseButtonUp(ButtonIndex, Modifiers);
            bConsumed = (!bMouseFree) || bConsumed;
        }
    };

    PumpMouseButton(VK_LBUTTON, 0);
    PumpMouseButton(VK_RBUTTON, 1);
    PumpMouseButton(VK_MBUTTON, 2);

    if (Input.GetScrollDelta() != 0)
    {
        const float WheelDelta = -Input.GetScrollNotches();
        const bool bEventNotConsumed = Context->ProcessMouseWheel(Rml::Vector2f(0.0f, WheelDelta), Modifiers);
        bConsumed = (!bEventNotConsumed) || bConsumed;
    }

    bool bKeyboardConsumed = false;
    for (int VK = 0; VK < 256; ++VK)
    {
        if (IsMouseButtonVK(VK))
        {
            continue;
        }

        const Rml::Input::KeyIdentifier Key = MapVirtualKeyToRmlKey(VK);
        if (Key == Rml::Input::KI_UNKNOWN)
        {
            continue;
        }

        if (Input.GetKeyDown(VK))
        {
            const bool bEventNotConsumed = Context->ProcessKeyDown(Key, Modifiers);
            bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
        }
        if (Input.GetKeyUp(VK))
        {
            const bool bEventNotConsumed = Context->ProcessKeyUp(Key, Modifiers);
            bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
        }
    }

    for (uint32_t Codepoint : Input.ConsumeTextInput())
    {
        const bool bEventNotConsumed = Context->ProcessTextInput(static_cast<Rml::Character>(Codepoint));
        bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
    }

    RestoreDocumentVisibility(VisibilitySnapshot);

    bConsumed = bKeyboardConsumed || bConsumed;
    const bool bTextInputFocused = IsElementOrAncestorFormControl(Context->GetFocusElement());

    if (bConsumed)
    {
        Input.SetGuiMouseCapture(true);
        Input.SetGuiViewportMouseBlock(true);
        if (bKeyboardConsumed)
        {
            Input.SetGuiKeyboardCapture(true);
        }
    }
    if (bTextInputFocused)
    {
        Input.SetGuiKeyboardCapture(true);
        Input.SetGuiTextInputCapture(true);
    }

    return bConsumed;
}

bool FRmlUiSystem::PumpGameInput(
    InputSystem& Input,
    FWindowsWindow* Window,
    bool bAllowRuntimeUIInput,
    int32 LayoutWidth,
    int32 LayoutHeight)
{
    if (!Window)
    {
        return false;
    }

    const FViewportRect ViewportRect(
        0,
        0,
        static_cast<uint32>(std::max(static_cast<int32>(Window->GetWidth()), 1)),
        static_cast<uint32>(std::max(static_cast<int32>(Window->GetHeight()), 1)));

    const FScaledRuntimeUIViewport ScaledViewport = CalculateScaledRuntimeUIViewport(
        0.0f,
        0.0f,
        static_cast<float>(ViewportRect.Width),
        static_cast<float>(ViewportRect.Height),
        LayoutWidth,
        LayoutHeight);

    FViewportRect ScaledRect;
    ScaledRect.X = static_cast<int32>(ScaledViewport.X);
    ScaledRect.Y = static_cast<int32>(ScaledViewport.Y);
    ScaledRect.Width = static_cast<uint32>(std::max(ScaledViewport.Width, 1.0f));
    ScaledRect.Height = static_cast<uint32>(std::max(ScaledViewport.Height, 1.0f));

    return PumpViewportInput(Input, Window, bAllowRuntimeUIInput, ScaledRect, LayoutWidth, LayoutHeight, false);
}

void FRmlUiSystem::EnqueueActionEvent(const FString& EventName, Rml::ElementDocument* SourceDocument)
{
    if (EventName.empty())
    {
        return;
    }

    auto It = std::find_if(
        DocumentsByScreenId.begin(),
        DocumentsByScreenId.end(),
        [SourceDocument](const auto& Pair)
        {
            return Pair.second == SourceDocument;
        });

    if (It != DocumentsByScreenId.end() && It->first == RuntimeUIPreviewScreenId)
    {
        PreviewPendingActionEvents.push_back(EventName);
        return;
    }

    PendingActionEvents.push_back(EventName);
}

Rml::ElementDocument* FRmlUiSystem::FindDocument(const FString& ScreenId) const
{
    auto It = DocumentsByScreenId.find(ScreenId);
    return It != DocumentsByScreenId.end() ? It->second : nullptr;
}

Rml::Element* FRmlUiSystem::FindElement(const FString& ElementId) const
{
    if (ElementId.empty())
    {
        return nullptr;
    }

    for (const auto& Pair : DocumentsByScreenId)
    {
        if (Pair.first == RuntimeUIPreviewScreenId)
        {
            continue;
        }
        if (Pair.second)
        {
            if (Rml::Element* Element = Pair.second->GetElementById(ElementId))
            {
                return Element;
            }
        }
    }

    return nullptr;
}

void FRmlUiSystem::AttachDocumentListeners(Rml::ElementDocument* Document)
{
    if (!Document || !ActionListener)
    {
        return;
    }

    Document->AddEventListener("click", ActionListener);
    Document->AddEventListener("change", ActionListener);
    Document->AddEventListener("submit", ActionListener);
}

int FRmlUiSystem::GetKeyModifierState(const InputSystem& Input) const
{
    int Modifiers = 0;
    if (Input.GetKey(VK_CONTROL) || Input.GetKey(VK_LCONTROL) || Input.GetKey(VK_RCONTROL))
    {
        Modifiers |= Rml::Input::KM_CTRL;
    }
    if (Input.GetKey(VK_SHIFT) || Input.GetKey(VK_LSHIFT) || Input.GetKey(VK_RSHIFT))
    {
        Modifiers |= Rml::Input::KM_SHIFT;
    }
    if (Input.GetKey(VK_MENU) || Input.GetKey(VK_LMENU) || Input.GetKey(VK_RMENU))
    {
        Modifiers |= Rml::Input::KM_ALT;
    }
    if ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_CAPSLOCK;
    }
    if ((::GetKeyState(VK_NUMLOCK) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_NUMLOCK;
    }
    if ((::GetKeyState(VK_SCROLL) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_SCROLLLOCK;
    }
    return Modifiers;
}

TArray<std::pair<Rml::ElementDocument*, bool>> FRmlUiSystem::ApplyDocumentVisibilityFilter(bool bPreviewDocumentOnly)
{
    TArray<std::pair<Rml::ElementDocument*, bool>> ChangedVisibilitySnapshot;
    if (!Context)
    {
        return ChangedVisibilitySnapshot;
    }

    for (const auto& Pair : DocumentsByScreenId)
    {
        Rml::ElementDocument* Document = Pair.second;
        if (!Document)
        {
            continue;
        }

        const bool bWasVisible = Document->IsVisible();
        const bool bIsPreviewDocument = Pair.first == RuntimeUIPreviewScreenId;
        const bool bShouldRender = bPreviewDocumentOnly ? bIsPreviewDocument : (!bIsPreviewDocument && bWasVisible);

        if (bWasVisible != bShouldRender)
        {
            ChangedVisibilitySnapshot.push_back(std::make_pair(Document, bWasVisible));
            SetDocumentVisibleIfNeeded(Document, bShouldRender);
        }
    }

    return ChangedVisibilitySnapshot;
}

void FRmlUiSystem::RestoreDocumentVisibility(const TArray<std::pair<Rml::ElementDocument*, bool>>& VisibilitySnapshot)
{
    for (const auto& Pair : VisibilitySnapshot)
    {
        Rml::ElementDocument* Document = Pair.first;
        if (!Document)
        {
            continue;
        }

        SetDocumentVisibleIfNeeded(Document, Pair.second);
    }
}
