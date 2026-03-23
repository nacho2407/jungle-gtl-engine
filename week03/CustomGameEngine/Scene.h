#pragma once

#include "Component/SceneComponent.h"
#include "EngineCore/EngineTypes.h"
#include "Renderer/Level.h"
#include <string>

class UPrimitiveComponent;
class URenderer;

class UScene : public UObject
{
	DECLARE_RTTI(UScene, UObject)
public:
	UScene();
	virtual ~UScene() override;
public:
	int Version;
	int NextUUID;
	TArray<USceneComponent*> SceneComponents;
	//FName Name;

	bool IsCompSelected{ false };
	FVector SelectedCompPos;

	FLevel Level;

	void Update(float DeltaTime);
	std::string ToJson();
	static std::string ToJson(UScene*);
	static UScene* FromJson(std::string);
};