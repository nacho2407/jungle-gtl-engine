#include "Component/PatrolAgentComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_COMPONENT_CLASS(UPatrolAgentComponent, UActorComponent, EEditorComponentCategory::Movement)

void UPatrolAgentComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << PatrolGroup;
    Ar << MoveSpeed;
    Ar << ReachDistance;
    Ar << bLoop;
}

void UPatrolAgentComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Patrol Group", EPropertyType::String, &PatrolGroup });
    OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed, 0.0f, 100.0f, 0.1f });
    OutProps.push_back({ "Reach Distance", EPropertyType::Float, &ReachDistance, 0.0f, 100.0f, 0.1f });
    OutProps.push_back({ "Loop", EPropertyType::Bool, &bLoop });
}
