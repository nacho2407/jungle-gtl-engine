#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreTypes.h"
#include "Core/Paths.h"

// Forward declarations
class UObject;
class UWorld;
class USceneComponent;
class UCameraComponent;
class UPrimitiveComponent;

namespace json {
	class JSON;
}

using std::string;

class FSceneSaveManager {
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";

	// FPaths 기반 Scene 디렉터리 (wstring)
	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }

	// Creates a .json save file at the given destination
	static void SaveSceneAsJSON(const string& SceneName, TArray<UWorld*>& Scene);
	static void LoadSceneFromJSON(const string& filepath, TArray<UWorld*>& Scene);

	// Returns list of .Scene file names (without extension) in SceneDirectory
	static TArray<FString> GetSceneFileList();

	// If file exists at given path, delete, then save. Otherwise, simply create a new save
	static void OverwriteSave(const string& filepath, UWorld* World);

private:

	//-----------------------------------------------------------------------------------
	// Serialization
	// ----------------------------------------------------------------------------------
	// Creates a .json save file at the given destination
	static json::JSON SerializeObject(UObject* Object);

	static json::JSON SerializeVector(float X, float Y, float Z);

	//-----------------------------------------------------------------------------------
	// Desrialization
	// ----------------------------------------------------------------------------------
	// Resolves parent-child and owning references between components, actors, and world
	static void LinkReferences(const TMap<uint32, UObject*>& uuidMap, json::JSON Savedata);
	
	// Generate spatial vectors for USceneCompoents from a json save
	static void DeserializeSpaceVectors(USceneComponent* SceneComp, json::JSON& Savedata);

	static void DecodeCamera(UCameraComponent* Camera, json::JSON& Savedata);

	static void DecodePrimitiveComponents(UPrimitiveComponent* Prim, json::JSON& Savedata);

	static string GetCurrentTimeStamp();
};
