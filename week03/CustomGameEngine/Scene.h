#pragma once
#include <string>
#include "json.hpp"

class UPrimitiveComponent;
class URenderer;
class FScene
{
public:
	int Version;
	int NextUUID;

	void Update(float DeltaTime);
	std::string ToJson();
	static std::string ToJson(FScene);
	static FScene FromJson(std::string);
	void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	std::vector<UPrimitiveComponent*> Primitives; // TODO : Change to TArray And Private

public:
	void Render() const;

private:
	URenderer* Renderer;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FScene, Version, NextUUID)