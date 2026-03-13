#pragma once

#include "Scene.h"
#include "ISingleton.h"
#include "UEngineTypes.h"

class SceneManager : public ISingleton<SceneManager>
{
	friend class ISingleton<SceneManager>;
private:
	SceneManager();

public:
	~SceneManager();

	FScene* GetActiveScene();

	// Unload current scene and create new scene
	FScene* NewScene();

	// Save Scene Synchronously
	void SaveScene(FString sceneName);

	// Load Scene Synchronously
	FScene* LoadScene(FString sceneName);

	// Unload Scene Synchronously
	void UnloadScene();

private:
	FScene* activeScene;
};

