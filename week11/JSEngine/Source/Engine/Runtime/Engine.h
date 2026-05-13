#pragma once

#include "Object/Object.h"
#include "Audio/AudioSystem.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Render/Common/ViewTypes.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Renderer/IRenderPipeline.h"
#include "UI/RmlUi/RmlUiSystem.h"

#include <memory>

class FWindowsWindow;
class FTimer;
class UCameraComponent;
class APlayerController;
struct FGuiInputState;

enum class ERuntimeInputMode : uint8
{
	GameOnly,
	UIOnly,
	GameAndUI
};

struct FRuntimeInputPermissions
{
	bool bAllowRuntimeUIInput = true;
	bool bAllowPlayerInput = true;
	bool bAllowLuaKeyboardInput = true;
	bool bAllowLuaMouseInput = true;
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
	virtual bool CanCloseApplication() { return true; }
	virtual bool RequestQuitGame();
	bool RequestOpenScene(const FString& ScenePath);
	bool RequestReloadScene();
	bool IsSceneOpenPending() const { return bPendingSceneOpen; }
	FString GetCurrentScenePath() const { return CurrentScenePath; }

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
	const FShowFlags& GetRuntimeShowFlags() const { return RuntimeShowFlags; }
	FShowFlags& GetMutableRuntimeShowFlags() { return RuntimeShowFlags; }
	virtual APlayerController* GetPrimaryPlayerController() const;
	FRmlUiSystem& GetRmlUiSystem() { return RmlUiSystem; }
	const FRmlUiSystem& GetRmlUiSystem() const { return RmlUiSystem; }
	FAudioSystem& GetAudioSystem() { return AudioSystem; }
	const FAudioSystem& GetAudioSystem() const { return AudioSystem; }
	void SetRuntimeInputMode(ERuntimeInputMode InMode);
	ERuntimeInputMode GetRuntimeInputMode() const { return RuntimeInputMode; }
	FRuntimeInputPermissions BuildRuntimeInputPermissions(
		const FGuiInputState& GuiState,
		bool bRuntimeUIConsumedInput = false) const;
	void SetRuntimeCursorVisible(bool bVisible);
	bool IsRuntimeCursorVisible() const { return bRuntimeCursorVisible; }
	void SetRuntimeCursorLocked(bool bLocked);
	bool IsRuntimeCursorLocked() const { return bRuntimeCursorLocked; }

protected:
	void Render(float DeltaTime);
	void SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline);
	virtual void WorldTick(float DeltaTime);
	void ProcessPendingSceneOpen();
	virtual bool OpenSceneNow(const FString& ScenePath);
	virtual void OnSceneWorldWillUnload(UWorld* OldWorld) {}
	virtual void OnSceneWorldLoaded(UWorld* NewWorld) {}
	FString ResolveScenePath(const FString& ScenePath) const;

protected:
	FWindowsWindow* Window = nullptr;

	FName ActiveWorldHandle;
	TArray<FWorldContext> WorldList;

	FTimer* Timer = nullptr;

	FRenderer Renderer;
	FShowFlags RuntimeShowFlags;
	FAudioSystem AudioSystem;
	FRmlUiSystem RmlUiSystem;
	ERuntimeInputMode RuntimeInputMode = ERuntimeInputMode::GameOnly;
	bool bRuntimeCursorVisible = false;
	bool bRuntimeCursorLocked = true;
	bool bPendingSceneOpen = false;
	FString PendingSceneOpenPath;
	FString CurrentScenePath;

private:
	std::unique_ptr<IRenderPipeline> RenderPipeline;
};

extern UEngine* GEngine;
