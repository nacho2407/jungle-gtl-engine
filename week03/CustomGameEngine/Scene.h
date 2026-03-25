#pragma once

#include "Component/SceneComponent.h"
#include "EngineCore/EngineTypes.h"
#include "Renderer/Level.h"
#include <string>

class UPrimitiveComponent;
class URenderer;

/**
 * 씬 클래스. Level을 가지며 씬에 존재하는
 * (특정 씬에 의존하는) 컴포넌트를 담은 리스트를 가집니다.
 */
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

	FLevel Level;

	void Update(float DeltaTime);
	void DrawDebugLines(); // AABB 시각화
	std::string ToJson();
	static std::string ToJson(UScene*);
	static UScene* FromJson(std::string);
};
