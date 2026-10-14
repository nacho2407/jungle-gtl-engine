#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/CurvedWallRunColliderActor.generated.h"

class UBoxComponent;
class UCurvedWallRunColliderComponent;

/**
 * @brief 자동 물리 패널을 생성하는 곡선형 wall-run collider 액터
 */
UCLASS()
class ACurvedWallRunColliderActor : public AActor
{
public:
	GENERATED_BODY()

	ACurvedWallRunColliderActor() = default;

	/**
	 * @brief 기본 곡선 query component를 루트로 생성합니다
	 */
	void InitDefaultComponents();

	void PostDuplicate() override;
	void BeginPlay() override;
	void EndPlay() override;

	UCurvedWallRunColliderComponent* GetCurvedColliderComponent() const { return CurvedColliderComponent; }

private:
	/**
	 * @brief 현재 곡선 설정에 맞춰 물리 차단용 box 패널을 생성합니다
	 */
	void BuildPhysicalPanels();

	/**
	 * @brief 런타임에 생성한 물리 패널을 제거합니다
	 */
	void ClearPhysicalPanels();

	UCurvedWallRunColliderComponent* CurvedColliderComponent = nullptr;
	TArray<UBoxComponent*> PhysicalPanels;
};
