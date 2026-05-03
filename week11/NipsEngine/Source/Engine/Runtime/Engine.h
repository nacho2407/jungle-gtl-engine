#pragma once

#include "Object/Object.h"
#include "Audio/AudioSystem.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Renderer/IRenderPipeline.h"
#include "UI/RuntimeUITypes.h"

#include <memory>

class FWindowsWindow;
class FTimer;
class UCameraComponent;
class APlayerController;

enum class ERuntimeInputMode : uint8
{
	GameOnly,
	UIOnly,
	GameAndUI
};

class UEngine : public UObject
{
public:
	DECLARE_CLASS(UEngine, UObject)

	UEngine() = default;
	~UEngine() override = default;

	// Lifecycle
	virtual void Init(FWindowsWindow* InWindow);
	virtual void Shutdown();
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);

	virtual void OnWindowResized(uint32 Width, uint32 Height);
	virtual bool RequestQuitGame();

	// World context management
	FWorldContext& CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name = "");
	void DestroyWorldContext(const FName& Handle);

	// World context lookup
	FWorldContext* GetWorldContextFromHandle(const FName& Handle);
	const FWorldContext* GetWorldContextFromHandle(const FName& Handle) const;
	FWorldContext* GetWorldContextFromWorld(const UWorld* World);

	// Active world
	virtual void SetActiveWorld(const FName& Handle);
	FName GetActiveWorldHandle() const { return ActiveWorldHandle; }

	// Accessors
	FWindowsWindow* GetWindow() const { return Window; }
	virtual UWorld* GetWorld() const;

	const TArray<FWorldContext>& GetWorldList() const { return WorldList; }
	TArray<FWorldContext>& GetWorldList() { return WorldList; }

	void SetTimer(FTimer* InTimer) { Timer = InTimer; }
	FTimer* GetTimer() const { return Timer; }

	FRenderer& GetRenderer() { return Renderer; }
	IRenderPipeline* GetRenderPipeline() const { return RenderPipeline.get(); }
	virtual APlayerController* GetPrimaryPlayerController() const;
	virtual bool LoadRmlUIDocument(const FString& ScreenId, const FString& Path) { return false; }
	virtual bool UnloadRmlUIDocument(const FString& ScreenId) { return false; }
	virtual bool ReloadRmlUIDocument(const FString& ScreenId) { return false; }
	virtual bool ShowRmlUIScreen(const FString& ScreenId) { return false; }
	virtual bool HideRmlUIScreen(const FString& ScreenId) { return false; }
	virtual bool SetRmlUIElementText(const FString& ElementId, const FString& Text) { return false; }
	virtual bool SetRmlUIElementVisible(const FString& ElementId, bool bVisible) { return false; }
	virtual bool SetRmlUIElementEnabled(const FString& ElementId, bool bEnabled) { return false; }
	virtual bool SetRmlUIElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled) { return false; }
	virtual bool SetRmlUIElementAttribute(const FString& ElementId, const FString& Name, const FString& Value) { return false; }
	virtual bool SetRmlUIElementStyle(const FString& ElementId, const FString& Name, const FString& Value) { return false; }
	virtual TArray<FString> PollRmlUIActionEvents() { return {}; }
	FAudioSystem& GetAudioSystem() { return AudioSystem; }
	const FAudioSystem& GetAudioSystem() const { return AudioSystem; }
	void SetRuntimeInputMode(ERuntimeInputMode InMode);
	ERuntimeInputMode GetRuntimeInputMode() const { return RuntimeInputMode; }
	void SetRuntimeCursorVisible(bool bVisible);
	bool IsRuntimeCursorVisible() const { return bRuntimeCursorVisible; }
	void SetRuntimeCursorLocked(bool bLocked);
	bool IsRuntimeCursorLocked() const { return bRuntimeCursorLocked; }
	void SetTimeScale(float InTimeScale);
	float GetTimeScale() const { return TimeScale; }
	float GetDeltaTime() const { return LastDeltaTime; }
	float GetUnscaledDeltaTime() const { return LastUnscaledDeltaTime; }
	double GetGameTime() const { return GameTimeSeconds; }
	double GetRealTime() const { return RealTimeSeconds; }
	virtual void RenderRuntimeUI(const FRuntimeUIRenderContext& Context) {}

protected:
	void UpdateTimeState(float DeltaTime);
	void Render(float DeltaTime);
	void SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline);
	virtual void WorldTick(float DeltaTime);

protected:
	FWindowsWindow* Window = nullptr;

	FName ActiveWorldHandle;
	TArray<FWorldContext> WorldList;

	FTimer* Timer = nullptr;

	FRenderer Renderer;
	FAudioSystem AudioSystem;
	ERuntimeInputMode RuntimeInputMode = ERuntimeInputMode::GameOnly;
	bool bRuntimeCursorVisible = false;
	bool bRuntimeCursorLocked = true;
	float TimeScale = 1.0f;
	float LastDeltaTime = 0.0f;
	float LastUnscaledDeltaTime = 0.0f;
	double GameTimeSeconds = 0.0;
	double RealTimeSeconds = 0.0;

private:
	std::unique_ptr<IRenderPipeline> RenderPipeline;
};

extern UEngine* GEngine;
