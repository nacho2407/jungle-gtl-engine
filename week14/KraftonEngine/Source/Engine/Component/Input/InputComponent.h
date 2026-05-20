#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"

// UE 의 EInputEvent 의 minimal subset — Repeat/DoubleClick 등은 후속.
enum class EInputEvent : uint8
{
	Pressed,
	Released,
};

// UE 의 UInputComponent 패턴 minimal:
//   - Axis mapping: 이름과 키(VKey)+scale 묶음. 같은 이름에 여러 키 가능 (e.g. MoveForward = W(+1), S(-1)).
//   - Action mapping: 이름과 키. 같은 이름에 여러 키 가능 (e.g. Jump = Space, Gamepad A).
//   - BindAxis: 매 frame 합산된 axis value(float) 로 callback.
//   - BindAction: 키 edge (Pressed/Released) 일 때만 callback.
//
// APawn 자식이 SetupInputComponent override 안에서 AddXxxMapping + BindXxx 호출하는 게 통상 패턴.
// Lua 도 sol2 binding 으로 동일 API 사용 가능.

#include "Source/Engine/Component/Input/InputComponent.generated.h"

UCLASS()
class UInputComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UInputComponent() = default;
	~UInputComponent() override = default;

	// 매핑 — 코드 또는 ProjectSettings(.ini) 가 호출. 같은 이름에 여러 키 가능.
	void AddAxisMapping(const FString& Name, int VKey, float Scale = 1.0f);
	void AddActionMapping(const FString& Name, int VKey);

	// Binding — Pawn 자식이 SetupInputComponent 안에서 호출.
	void BindAxis(const FString& Name, TFunction<void(float)> Callback);
	void BindAction(const FString& Name, EInputEvent Event, TFunction<void()> Callback);

	// 등록된 binding 전부 제거 — SetupInputComponent 재호출 전 호출.
	void ClearBindings();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	struct FAxisMapping   { FString Name; int VKey = 0; float Scale = 1.0f; };
	struct FActionMapping { FString Name; int VKey = 0; };
	struct FAxisBinding   { FString Name; TFunction<void(float)> Callback; };
	struct FActionBinding { FString Name; EInputEvent Event = EInputEvent::Pressed; TFunction<void()> Callback; };

	TArray<FAxisMapping>   AxisMappings;
	TArray<FActionMapping> ActionMappings;
	TArray<FAxisBinding>   AxisBindings;
	TArray<FActionBinding> ActionBindings;
};
