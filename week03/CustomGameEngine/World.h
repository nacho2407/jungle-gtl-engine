#pragma once

#include "Singleton.h"
#include "EngineCore/Logger.h"		// TEST
#include "Scene.h"
#include "Renderer/Level.h"
#include "Component/PrimitiveComponent.h"
#include "SceneManager.h"

class FLevel;
class SceneManager;

class UWorld : public UObject, public ISingleton<UWorld>
{
	friend class ISingleton<UWorld>;
private:
	UWorld();

public:
	~UWorld() override;

	SceneManager* GetSceneManager();
	// === Scene Management
	//UScene* GetActiveScene();
	//void SetActiveScene(UScene*);
	//UScene* NewScene();
	//void SaveScene(FString sceneName);
	//UScene* LoadScene(FString sceneName);
	//void UnloadScene();
	// === Scene Management

	void Update(float deltaTime);
	inline void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	void OnBeforeRender() const;

private:
	SceneManager* SceneMgr;
	URenderer* Renderer;
};

UWorld& GetWorld();