#pragma once
#include <string>
#include "UEngineTypes.h"
#include "USceneComponent.h"

class UPrimitiveComponent;
class URenderer;
class FScene
{
public:
	int Version;
	int NextUUID;
	TArray<USceneComponent> Primitives;


	void Update(float DeltaTime);
	std::string ToJson();
	static std::string ToJson(FScene);
	static FScene FromJson(std::string);
	void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	std::vector<UPrimitiveComponent*> PrimitiveContainer; // TODO : Change to TArray And Private

public:
	void Render() const;

private:
	URenderer* Renderer;
};