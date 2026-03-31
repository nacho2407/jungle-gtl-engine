#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "GameFramework/WorldContext.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;

namespace json {
	class JSON;
}

struct FPropertyDescriptor;

using std::string;

class FSceneSaveManager {
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";

	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }

	static void SaveSceneAsJSON(const string& SceneName, FWorldContext& WorldContext);
	static void LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext);

	static TArray<FString> GetSceneFileList();

private:
	// ---- Serialization ----
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx);
	static json::JSON SerializeActor(AActor* Actor);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp);
	static json::JSON SerializeProperties(UActorComponent* Comp);
	static json::JSON SerializePropertyValue(const FPropertyDescriptor& Prop);

	// ---- Deserialization ----
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner);
	static void DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON);
	static void DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value);

	static string GetCurrentTimeStamp();
};
