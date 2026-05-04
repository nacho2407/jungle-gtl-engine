#pragma once

#include "Component/ActorComponent.h"

class UPatrolAgentComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UPatrolAgentComponent, UActorComponent)

    UPatrolAgentComponent() = default;
    ~UPatrolAgentComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    const FString& GetPatrolGroup() const { return PatrolGroup; }
    void SetPatrolGroup(const FString& InPatrolGroup) { PatrolGroup = InPatrolGroup; }

    float GetMoveSpeed() const { return MoveSpeed; }
    void SetMoveSpeed(float InMoveSpeed) { MoveSpeed = InMoveSpeed; }

    float GetReachDistance() const { return ReachDistance; }
    void SetReachDistance(float InReachDistance) { ReachDistance = InReachDistance; }

    bool IsLoop() const { return bLoop; }
    void SetLoop(bool bInLoop) { bLoop = bInLoop; }

private:
    FString PatrolGroup = "Default";
    float MoveSpeed = 5.0f;
    float ReachDistance = 1.0f;
    bool bLoop = true;
};
