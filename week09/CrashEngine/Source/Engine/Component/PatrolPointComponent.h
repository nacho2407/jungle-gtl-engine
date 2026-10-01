#pragma once

#include "Component/ActorComponent.h"

class UPatrolPointComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UPatrolPointComponent, UActorComponent)

    UPatrolPointComponent() = default;
    ~UPatrolPointComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    const FString& GetPatrolGroup() const { return PatrolGroup; }
    void SetPatrolGroup(const FString& InPatrolGroup) { PatrolGroup = InPatrolGroup; }

    int32 GetPatrolOrder() const { return PatrolOrder; }
    void SetPatrolOrder(int32 InPatrolOrder) { PatrolOrder = InPatrolOrder; }

private:
    FString PatrolGroup = "Default";
    int32 PatrolOrder = 0;
};
