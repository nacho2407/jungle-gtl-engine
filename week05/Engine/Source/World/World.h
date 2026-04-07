#pragma once
#include "CoreMinimal.h"
#include "Object/Object.h"
#include "Scene/WorldTypes.h"

// Forward declarations로 include 의존성을 최소화한다.
class UScene;
class AActor;
class UCameraComponent;
class FCamera;
class FFrustum;
struct FRenderCommandQueue;
struct ID3D11Device;

class ENGINE_API UWorld : public UObject
{
public:
	DECLARE_RTTI(UWorld, UObject)
	~UWorld() override;

	/** PersistentLevel에 새 액터를 스폰하고, 생성 직후 초기화까지 마친 뒤 반환한다. */
	template <typename T>
	T* SpawnActor(const FString& InName);
	/** 액터를 즉시 제거하지 않고 현재 월드에서 파괴 대상으로 표시한다. */
	void DestroyActor(AActor* InActor);

	/** 항상 존재하는 기본 씬(Persistent Level)을 반환한다. */
	UScene* GetPersistentLevel() const { return PersistentLevel; }
	/** 스트리밍 레벨을 로드해 월드에 추가한다. 이미 로드돼 있으면 기존 레벨을 반환한다. */
	UScene* LoadStreamingLevel(const FString& LevelName, ID3D11Device* Device = nullptr);
	/** 이름으로 스트리밍 레벨을 찾아 월드에서 제거한다. */
	void UnloadStreamingLevel(const FString& LevelName);
	/** 이름에 해당하는 스트리밍 레벨을 찾는다. */
	UScene* FindStreamingLevel(const FString& LevelName) const;
	/** 현재 로드된 모든 스트리밍 레벨 목록을 반환한다. */
	const TArray<UScene*>& GetStreamingLevels() const { return StreamingLevels; }

	/** Persistent + Streaming 레벨을 합친 전체 액터 목록을 새 배열로 돌려준다. */
	TArray<AActor*> GetAllActors() const;
	/** PersistentLevel에 속한 액터 목록만 반환한다. */
	const TArray<AActor*>& GetActors() const;

	/** 기본 씬 접근용 별칭이다. */
	UScene* GetScene() const { return PersistentLevel; }
	/** 현재 월드가 사용할 활성 카메라 컴포넌트를 지정한다. null이면 기본 씬 카메라로 되돌린다. */
	void SetActiveCameraComponent(UCameraComponent* InCamera);
	/** 렌더링과 뷰 계산에 사용할 활성 카메라 컴포넌트를 반환한다. */
	UCameraComponent* GetActiveCameraComponent() const;
	/** 활성 카메라 컴포넌트가 보유한 실제 카메라 객체를 반환한다. */
	FCamera* GetCamera() const;

	/** 월드의 기본 씬과 기본 카메라를 준비하고, 필요하면 기본 씬 파일도 로드한다. */
	void InitializeWorld(float AspectRatio, ID3D11Device* Device = nullptr);
	/** 월드 안 모든 레벨과 액터에 BeginPlay를 한 번만 전파한다. */
	void BeginPlay();
	/** 시간 누적 후 Persistent/Streaming 레벨을 모두 Tick한다. */
	void Tick(float InDeltaTime);
	/** 월드가 소유한 씬, 카메라, 스트리밍 레벨을 정리하고 초기 상태로 되돌린다. */
	void CleanupWorld();

	EWorldType GetWorldType() const { return WorldType; }
	void SetWorldType(EWorldType InType) { WorldType = InType; }
	float GetWorldTime() const { return WorldTime; }
	float GetDeltaTime() const { return DeltaSeconds; }

	static UWorld* DuplicateWorldForPIE(UWorld* EditorWorld);

private:
	UScene* PersistentLevel = nullptr;
	TArray<UScene*> StreamingLevels;

	bool bBegunPlay = false;
	float WorldTime = 0.f;
	float DeltaSeconds = 0.f;
	EWorldType WorldType = EWorldType::Game;
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
