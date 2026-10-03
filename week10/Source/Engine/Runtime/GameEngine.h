#pragma once

#include "Engine/Runtime/Engine.h"

class APlayerController;
class AGameModeBase;
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
    APlayerController* GetPrimaryPlayerController() const override { return PlayerController; }

    APlayerController* GetPlayerController() const { return PlayerController; }

private:
    struct FGameStartupSettings
    {
        FString GameName = "JSEngineGame";
        FString StartupScene = "Asset/Scene/Default.scene";
        FString GameModeClass = "AGameModeBase";
        FString PlayerControllerClass = "APlayerController";
        FString DefaultPawnClass = "ADefaultPawn";
        FString DefaultPawnPrefabPath;
    };

    void LoadGameSettings();
    void LoadStartupWorld();
    AGameModeBase* EnsureGameMode();
    void EnsurePlayerController();
    void MaintainGameInputCapture(InputSystem& Input);
    bool PumpRuntimeUIInput(InputSystem& Input);
    void PumpPlayerInput(InputSystem& Input);
    void OnSceneWorldWillUnload(UWorld* OldWorld) override;
    void OnSceneWorldLoaded(UWorld* NewWorld) override;
    FString ResolveStartupScenePath() const;

private:
    FGameStartupSettings StartupSettings;
    AGameModeBase* GameMode = nullptr;
    APlayerController* PlayerController = nullptr;
    bool bLoggedInputCapture = false;
    bool bLoggedFirstInput = false;
    uint64 PlayerInputFrameCounter = 0;
};
