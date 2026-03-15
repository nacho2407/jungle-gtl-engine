#pragma once
#include <string>
#include "UEngineTypes.h"
#include "USceneComponent.h"

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

	bool IsCompSelected{ false };
	FVector SelectedCompPos;


	void Update(float DeltaTime);
	std::string ToJson();
	static std::string ToJson(UScene*);
	static UScene* FromJson(std::string);

public:
	void Render() const;
};