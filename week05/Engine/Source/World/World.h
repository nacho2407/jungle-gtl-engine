#pragma once
#include "CoreMinimal.h"
#include "Object/Object.h"
#include "Scene/SceneTypes.h"

// Forward declarations — include 최소화
class UScene;
class AActor;
class UCameraComponent;
class CCamera;
class FFrustum;
struct FRenderCommandQueue;
struct ID3D11Device;

class ENGINE_API UWorld : public UObject
{
public:
	DECLARE_RTTI(UWorld, UObject)
	~UWorld();

	template <typename T>
	T* SpawnActor(const FString& InName);
	void DestroyActor(AActor* InActor);

	// ── Persistent Level ──
	UScene* GetPersistentLevel() const { return PersistentLevel; }
	// ── Streaming Levels ──
	UScene* LoadStreamingLevel(const FString& LevelName, ID3D11Device* Device = nullptr);
	void UnloadStreamingLevel(const FString& LevelName);
	UScene* FindStreamingLevel(const FString& LevelName) const;
	const TArray<UScene*>& GetStreamingLevels() const { return StreamingLevels; }

	// ── 전체 액터 조회 (Persistent + Streaming 합산) ──
	TArray<AActor*> GetAllActors() const;
	const TArray<AActor*>& GetActors() const;  // PersistentLevel만

	UScene* GetScene() const { return PersistentLevel; }
	// 카메라
	void SetActiveCameraComponent(UCameraComponent* InCamera);
	UCameraComponent* GetActiveCameraComponent() const;
	CCamera* GetCamera() const;


	// 라이프사이클
	void InitializeWorld(float AspectRatio, ID3D11Device* Device = nullptr);
	void BeginPlay();
	void Tick(float InDeltaTime);
	void CleanupWorld();
	


	ESceneType GetWorldType() const { return WorldType; }
	void SetWorldType(ESceneType InType) { WorldType = InType; }
	float GetWorldTime() const { return WorldTime; }
	float GetDeltaTime() const { return DeltaSeconds; }

private:
	UScene* PersistentLevel = nullptr;      
	TArray<UScene*> StreamingLevels;

	bool bBegunPlay = false;
	float WorldTime = 0.f;
	float DeltaSeconds = 0.f;
	ESceneType WorldType = ESceneType::Game;
	UCameraComponent* SceneCameraComponent = nullptr;    
	TObjectPtr<UCameraComponent> ActiveCameraComponent;
};
#include "Scene/Scene.h"

template <typename T>
T* UWorld::SpawnActor(const FString& InName)
{
	static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");
	if (!PersistentLevel) return nullptr;
	return PersistentLevel->SpawnActor<T>(InName);
}