#include "ArrayProperty.h"

#include "Serialization/Archive.h"


bool FArrayProperty::ContainsObjectReference() const
{
	return InnerProperty && InnerProperty->ContainsObjectReference();
}

void FArrayProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	uint32 Num = static_cast<uint32>(ArrayOps->GetNum(ValuePtr));
	if (Ar.BeginArray(Num))
	{
		if (Ar.IsLoading())
		{
			ArrayOps->Resize(ValuePtr, Num);
		}

		for (uint32 Index = 0; Index < Num; ++Index)
		{
			Ar.BeginArrayElement(Index);
			InnerProperty->SerializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Ar, Context);
			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	Ar << Num;
	if (Ar.IsLoading())
	{
		ArrayOps->Resize(ValuePtr, Num);
	}

	for (uint32 Index = 0; Index < Num; ++Index)
	{
		InnerProperty->SerializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Ar, Context);
	}
}

void FArrayProperty::AddReferencedObjects(void* ValuePtr, FReferenceCollector& Collector) const
{
    if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->GetElementPtr || !InnerProperty)
    {
        return;
    }

    const size_t Num = ArrayOps->GetNum(ValuePtr);
    for (size_t Index = 0; Index < Num; ++Index)
    {
        InnerProperty->AddReferencedObjects(ArrayOps->GetElementPtr(ValuePtr, Index), Collector);
    }
}

void FArrayProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	uint32 Num = static_cast<uint32>(ArrayOps->GetNum(ValuePtr));
	if (Ar.BeginArray(Num))
	{
		if (Ar.IsLoading())
		{
			ArrayOps->Resize(ValuePtr, Num);
		}

		for (uint32 Index = 0; Index < Num; ++Index)
		{
			Ar.BeginArrayElement(Index);
			InnerProperty->SerializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Ar);
			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	Ar << Num;
	if (Ar.IsLoading())
	{
		ArrayOps->Resize(ValuePtr, Num);
	}

	for (uint32 Index = 0; Index < Num; ++Index)
	{
		InnerProperty->SerializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Ar);
	}
}
