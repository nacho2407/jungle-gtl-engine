#pragma once

#include "Animation/AnimTypes.h"
#include "Core/CoreMinimal.h"
#include "Object/Object.h"

class USkeletalMeshComponent;

UCLASS(Abstract, DisplayName = "Anim Notify")
class UAnimNotify : public UObject
{
public:
	GENERATED_BODY(UAnimNotify, UObject)
	virtual ~UAnimNotify() override = default;

	virtual void Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) {}
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) {}
	virtual void NotifyTick(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event, float DeltaTime) {}
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) {}
};

UCLASS(Abstract, DisplayName = "Anim Notify State")
class UAnimNotifyState : public UAnimNotify
{
public:
	GENERATED_BODY(UAnimNotifyState, UAnimNotify)
	~UAnimNotifyState() override = default;
};

UCLASS(DisplayName = "Log Notify", Category = "Animation")
class UAnimNotify_LogEvent : public UAnimNotify
{
public:
	GENERATED_BODY(UAnimNotify_LogEvent, UAnimNotify)
	~UAnimNotify_LogEvent() override = default;

	void Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) override;
	void NotifyBegin(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) override;
	void NotifyTick(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event, float DeltaTime) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) override;
};

UCLASS(DisplayName = "Footstep Sound Notify", Category = "Animation")
class UAnimNotify_FootstepSound : public UAnimNotify
{
public:
	GENERATED_BODY(UAnimNotify_FootstepSound, UAnimNotify)
	~UAnimNotify_FootstepSound() override = default;

	void Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event) override;
};
