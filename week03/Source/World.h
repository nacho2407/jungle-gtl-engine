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

	void Update(float deltaTime);
	void DrawDebugLines();
	inline void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	URenderer* GetRenderer() const { return Renderer; }
	void OnBeforeRender() const;

private:
	SceneManager* SceneMgr;
	URenderer* Renderer;
};

UWorld& GetWorld();