#pragma once

#include "Engine/Runtime/Engine.h"
#include "Core/Containers/Map.h"
#include "UI/RmlUi/RmlUiRenderInterfaceD3D11.h"
#include "UI/RmlUi/RmlUiRuntimeModule.h"

class APlayerController;
class InputSystem;
class FRmlUiActionEventListener;

namespace Rml
{
    class Context;
    class Element;
    class ElementDocument;
}

class UGameEngine : public UEngine
{
public:
    DECLARE_CLASS(UGameEngine, UEngine)

    void Init(FWindowsWindow* InWindow) override;
    void Shutdown() override;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void OnWindowResized(uint32 Width, uint32 Height) override;
    void RenderRuntimeUI(const FRuntimeUIRenderContext& Context) override;
    APlayerController* GetPrimaryPlayerController() const override { return PlayerController; }
    bool LoadRmlUIDocument(const FString& ScreenId, const FString& Path) override;
    bool UnloadRmlUIDocument(const FString& ScreenId) override;
    bool ReloadRmlUIDocument(const FString& ScreenId) override;
    bool ShowRmlUIScreen(const FString& ScreenId) override;
    bool HideRmlUIScreen(const FString& ScreenId) override;
    bool HasRmlUIElement(const FString& ElementId) override;
    FString GetRmlUIElementText(const FString& ElementId) override;
    bool SetRmlUIElementText(const FString& ElementId, const FString& Text) override;
    bool SetRmlUIElementVisible(const FString& ElementId, bool bVisible) override;
    bool SetRmlUIElementEnabled(const FString& ElementId, bool bEnabled) override;
    bool SetRmlUIElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled) override;
    bool HasRmlUIElementClass(const FString& ElementId, const FString& ClassName) override;
    FString GetRmlUIElementClassNames(const FString& ElementId) override;
    bool SetRmlUIElementClassNames(const FString& ElementId, const FString& ClassNames) override;
    bool HasRmlUIElementAttribute(const FString& ElementId, const FString& Name) override;
    FString GetRmlUIElementAttribute(const FString& ElementId, const FString& Name) override;
    bool SetRmlUIElementAttribute(const FString& ElementId, const FString& Name, const FString& Value) override;
    bool RemoveRmlUIElementAttribute(const FString& ElementId, const FString& Name) override;
    FString GetRmlUIElementStyle(const FString& ElementId, const FString& Name) override;
    bool SetRmlUIElementStyle(const FString& ElementId, const FString& Name, const FString& Value) override;
    bool RemoveRmlUIElementStyle(const FString& ElementId, const FString& Name) override;
    bool FocusRmlUIElement(const FString& ElementId, bool bFocusVisible) override;
    bool BlurRmlUIElement(const FString& ElementId) override;
    bool ClickRmlUIElement(const FString& ElementId) override;
    TArray<FString> PollRmlUIActionEvents() override;

    APlayerController* GetPlayerController() const { return PlayerController; }
    void EnqueueRmlUIActionEvent(const FString& EventName);

private:
    struct FGameStartupSettings
    {
        FString GameName = "NipsGame";
        FString StartupScene = "Asset/Scene/Default.scene";
        FString PlayerControllerClass = "APlayerController";
    };

    void LoadGameSettings();
    void LoadStartupWorld();
    void EnsurePlayerController();
    void MaintainGameInputCapture(InputSystem& Input);
    bool PumpRuntimeUIInput(InputSystem& Input);
    void PumpPlayerInput(InputSystem& Input);
    FString ResolveStartupScenePath() const;
    void InitializeRmlUiRuntime();
    void ShutdownRmlUiRuntime();
    void RenderRmlUiTestDocument(const FRuntimeUIRenderContext& Context);
    bool PumpRmlUiInput(InputSystem& Input);
    int GetRmlUiKeyModifierState(const InputSystem& Input) const;
    Rml::ElementDocument* FindRmlUIDocument(const FString& ScreenId) const;
    Rml::Element* FindRmlUIElement(const FString& ElementId) const;
    void AttachRmlUIDocumentListeners(Rml::ElementDocument* Document);

private:
    FGameStartupSettings StartupSettings;
    APlayerController* PlayerController = nullptr;
    bool bLoggedInputCapture = false;
    bool bLoggedFirstInput = false;
    bool bRmlUiRuntimeInitialized = false;
    FRmlUiRuntimeModule RmlUiRuntimeModule;
    FRmlUiRenderInterfaceD3D11 RmlUiRenderInterface;
    Rml::Context* RmlUiContext = nullptr;
    Rml::ElementDocument* RmlUiTestDocument = nullptr;
    TMap<FString, FString> RmlUiDocumentPathByScreenId;
    TMap<FString, Rml::ElementDocument*> RmlUiDocumentsByScreenId;
    TArray<FString> RmlUiPendingActionEvents;
    FRmlUiActionEventListener* RmlUiActionListener = nullptr;
};
