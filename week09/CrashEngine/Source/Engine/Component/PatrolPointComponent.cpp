#include "Component/PatrolPointComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_COMPONENT_CLASS(UPatrolPointComponent, UActorComponent, EEditorComponentCategory::Movement)

void UPatrolPointComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << PatrolGroup;
    Ar << PatrolOrder;
}

void UPatrolPointComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Patrol Group", EPropertyType::String, &PatrolGroup });
    OutProps.push_back({ "Patrol Order", EPropertyType::Int, &PatrolOrder, 0.0f, 1000.0f, 1.0f });
}
