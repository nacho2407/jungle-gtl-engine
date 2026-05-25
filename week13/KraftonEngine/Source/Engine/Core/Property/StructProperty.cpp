#include "StructProperty.h"

#include "Serialization/Archive.h"
#include "Object/Reflection/UStruct.h"

void FStructProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	Ar.BeginObject();

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child || !Child->Name || !Ar.HasProperty(Child->Name))
		{
			continue;
		}

		Ar.BeginProperty(Child->Name);
		Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Ar, Context);
		Ar.EndProperty();
	}

	Ar.EndObject();
}

void FStructProperty::AddReferencedObjects(void* ValuePtr, FReferenceCollector& Collector) const
{
    if (!ValuePtr || !StructType)
    {
        return;
    }

    TArray<const FProperty*> Children;
    StructType->GetPropertyRefs(Children);
    for (const FProperty* Child : Children)
    {
        if (!Child) continue;
        FScopedReferenceName ChildReferenceScope(Collector, Child->Name);
        Child->AddReferencedObjects(Child->GetValuePtrFor(ValuePtr), Collector);
    }
}

void FStructProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	Ar.BeginObject();

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child || !Child->Name || !Ar.HasProperty(Child->Name))
		{
			continue;
		}

		Ar.BeginProperty(Child->Name);
		Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Ar);
		Ar.EndProperty();
	}

	Ar.EndObject();
}
