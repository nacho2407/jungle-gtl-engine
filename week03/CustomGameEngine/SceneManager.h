#pragma once

#include "Scene.h"
#include "Singleton.h"
#include "EngineTypes.h"

class SceneManager : public ISingleton<SceneManager>
{
	friend class ISingleton<SceneManager>;
private:
	SceneManager();

public:
	~SceneManager();

	// Unload current scene and create new scene
	UScene* NewScene();

	// Save Scene Synchronously
	void SaveScene(FString sceneName);

	// Load Scene Synchronously
	UScene* LoadScene(FString sceneName);

	// Unload Scene Synchronously
	void UnloadScene();
};

