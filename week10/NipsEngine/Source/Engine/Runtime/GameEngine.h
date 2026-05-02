#pragma once

#include "Engine/Runtime/Engine.h"
#include "UI/Backends/ImGuiRuntimeUIBackend.h"

class APlayerController;
class InputSystem;

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

    APlayerController* GetPlayerController() const { return PlayerController; }

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
    void InitializeRuntimeUIBackend();
    void ShutdownRuntimeUIBackend();
    FRuntimeUIResolvedImage ResolveRuntimeUIImage(const FString& ImagePath) const;

private:
    FGameStartupSettings StartupSettings;
    APlayerController* PlayerController = nullptr;
    bool bLoggedInputCapture = false;
    bool bLoggedFirstInput = false;
    bool bRuntimeUIBackendInitialized = false;
    FImGuiRuntimeUIBackend RuntimeUIBackend;
};
