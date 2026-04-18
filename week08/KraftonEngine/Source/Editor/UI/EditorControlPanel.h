#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Math/Vector.h"

class FEditorControlPanel : public FEditorPanel
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	// 컨트롤 패널에서 스폰 가능한 액터 타입입니다.
	enum class ESpawnActorType : int32
	{
		Cube = 0,
		Sphere,
		StaticMesh,
		Decal,
		HeightFog,
		FakeLight,
		Fireball,
		Count
	};

	// ImGui 콤보 박스에 표시할 액터 타입 이름 목록입니다.
	static constexpr const char* SpawnActorTypeNames[static_cast<int32>(ESpawnActorType::Count)] =
	{
		"Cube",
		"Sphere",
		"StaticMesh",
		"Decal",
		"Height Fog",
		"Fake Light",
		"Fireball"
	};

	int32 SelectedSpawnActorType = static_cast<int32>(ESpawnActorType::Cube);
	int32 NumberOfSpawnedActors = 1;
};
