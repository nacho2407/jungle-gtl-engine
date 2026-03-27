#pragma once

#include "CoreMinimal.h"
#include "World/WorldContext.h"
#include "Scene/SceneTypes.h"
#include <memory>

class UScene;
class UWorld;
class AActor;
class CRenderer;

class ENGINE_API FWorldManager
{
public:
	FWorldManager() = default;
	~FWorldManager();
	FWorldManager(const FWorldManager&) = delete;
	FWorldManager& operator=(const FWorldManager&) = delete;
	FWorldManager(FWorldManager&&) = delete;
	FWorldManager& operator=(FWorldManager&&) = delete;

	bool Initialize(float AspectRatio, ESceneType StartupSceneType, CRenderer* InRenderer);
	void Release();

	void ActivateEditorScene() { ActiveWorldContext = EditorWorldContext.World ? &EditorWorldContext : nullptr; }
	void ActivateGameScene() { ActiveWorldContext = GameWorldContext.World ? &GameWorldContext : nullptr; }
	bool ActivatePreviewScene(const FString& ContextName);

	FEditorWorldContext* CreatePreviewWorldContext(const FString& ContextName, int32 WindowWidth, int32 WindowHeight);
	bool DestroyPreviewWorld(const FString& ContextName);

	UWorld* GetActiveWorld() const { return ActiveWorldContext ? ActiveWorldContext->World : nullptr; }
	UWorld* GetEditorWorld() const { return EditorWorldContext.World; }
	UWorld* GetGameWorld() const { return GameWorldContext.World; }

	const FWorldContext* GetActiveWorldContext() const { return ActiveWorldContext; }
	const TArray<std::unique_ptr<FEditorWorldContext>>& GetPreviewWorldContexts() const { return PreviewWorldContexts; }

	UScene* GetActiveScene() const;
	UScene* GetEditorScene() const;
	UScene* GetGameScene() const;
	UScene* GetPreviewScene(const FString& ContextName) const;

	void OnResize(int32 Width, int32 Height);

private:
	bool CreateWorldContext(FWorldContext& OutContext, const FString& ContextName,
		ESceneType WorldType, float AspectRatio, bool bDefaultScene = true);
	void DestroyWorldContext(FWorldContext& Context);
	void DestroyWorldContext(FEditorWorldContext& Context);

	FEditorWorldContext* FindPreviewWorld(const FString& ContextName);
	const FEditorWorldContext* FindPreviewWorld(const FString& ContextName) const;

private:
	FWorldContext GameWorldContext;
	FEditorWorldContext EditorWorldContext;
	TArray<std::unique_ptr<FEditorWorldContext>> PreviewWorldContexts;
	FWorldContext* ActiveWorldContext = nullptr;
	CRenderer* Renderer = nullptr;
};
