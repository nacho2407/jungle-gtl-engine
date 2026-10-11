#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"

/**
* @brief 입력 action edge 종류
*/
enum class EInputEvent : uint8
{
	Pressed,
	Released,
};

#include "Source/Engine/Component/Input/InputComponent.generated.h"

/**
* @brief Pawn 입력 mapping과 delegate binding을 관리하는 컴포넌트
*
* @details Axis mapping은 이름과 입력 코드, scale을 묶어 매 프레임 합산합니다.
* Action mapping은 이름과 입력 코드를 묶고 Pressed/Released edge에서 callback을 호출합니다.
* 입력 코드는 Win32 VK 코드와 InputCodes 게임패드 가상 코드를 함께 사용할 수 있습니다.
*/
UCLASS()
class UInputComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UInputComponent() = default;
	~UInputComponent() override = default;

	/**
	* @brief axis mapping을 추가합니다.
	*
	* @param Name axis mapping 이름
	*
	* @param InputCode Win32 VK 코드 또는 InputCodes 게임패드 가상 코드
	*
	* @param Scale 입력 값에 곱할 배율
	*/
	void AddAxisMapping(const FString& Name, int InputCode, float Scale = 1.0f);

	/**
	* @brief action mapping을 추가합니다.
	*
	* @param Name action mapping 이름
	*
	* @param InputCode Win32 VK 코드 또는 InputCodes 게임패드 버튼 가상 코드
	*/
	void AddActionMapping(const FString& Name, int InputCode);

	/**
	* @brief axis delegate를 등록합니다.
	*
	* @param Name axis mapping 이름
	*
	* @param Callback 합산된 axis 값을 받는 callback
	*/
	void BindAxis(const FString& Name, TFunction<void(float)> Callback);

	/**
	* @brief action delegate를 등록합니다.
	*
	* @param Name action mapping 이름
	*
	* @param Event 호출할 입력 edge 종류
	*
	* @param Callback 입력 edge 발생 시 호출할 callback
	*/
	void BindAction(const FString& Name, EInputEvent Event, TFunction<void()> Callback);

	/**
	* @brief 등록된 delegate binding을 모두 제거합니다.
	*/
	void ClearBindings();

	/**
	* @brief 등록된 입력 mapping을 모두 제거합니다.
	*/
	void ClearMappings();

	/**
	* @brief 등록된 입력 mapping과 delegate binding을 모두 제거합니다.
	*/
	void ClearAllMappingsAndBindings();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	struct FAxisMapping   { FString Name; int InputCode = 0; float Scale = 1.0f; };
	struct FActionMapping { FString Name; int InputCode = 0; };
	struct FAxisBinding   { FString Name; TFunction<void(float)> Callback; };
	struct FActionBinding { FString Name; EInputEvent Event = EInputEvent::Pressed; TFunction<void()> Callback; };

	TArray<FAxisMapping>   AxisMappings;
	TArray<FActionMapping> ActionMappings;
	TArray<FAxisBinding>   AxisBindings;
	TArray<FActionBinding> ActionBindings;
};
