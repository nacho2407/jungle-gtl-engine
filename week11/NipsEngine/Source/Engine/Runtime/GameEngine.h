#pragma once

#include "Engine/Runtime/Engine.h"

class APlayerController;
class InputSystem;

class UGameEngine : public UEngine
{
public:
    DECLARE_CLASS(UGameEngine, UEngine)

    void Init(FWindowsWindow* InWindow) override;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void OnWindowResized(uint32 Width, uint32 Height) override;

    APlayerController* GetPlayerController() const { return PlayerController; }

private:
    struct FGameStartupSettings
    {
        FString GameName = "NipsGame";
        FString StartupScene = "Asset/Scene/Default.scene";
        FString PlayerControllerClass = "AGameJamPlayerController";
    };

    void LoadGameSettings();
    void LoadStartupWorld();
    void EnsurePlayerController();
    void MaintainGameInputCapture(InputSystem& Input);
    void PumpPlayerInput(InputSystem& Input);
    FString ResolveStartupScenePath() const;

private:
    FGameStartupSettings StartupSettings;
    APlayerController* PlayerController = nullptr;
    bool bLoggedInputCapture = false;
    bool bLoggedFirstInput = false;
};
