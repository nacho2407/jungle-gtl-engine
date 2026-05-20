#include "ArrayProperty.h"

#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FArrayProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->GetConstElementPtr || !InnerProperty)
	{
		return JSON();
	}

	JSON Arr = json::Array();
	const size_t Num = ArrayOps->GetNum(ValuePtr);
	for (size_t Index = 0; Index < Num; ++Index)
	{
		Arr.append(InnerProperty->SerializeValue(const_cast<void*>(ArrayOps->GetConstElementPtr(ValuePtr, Index))));
	}
	return Arr;
}

json::JSON FArrayProperty::SerializeValue(void* ValuePtr, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->GetConstElementPtr || !InnerProperty)
	{
		return JSON();
	}

	JSON Arr = json::Array();
	const size_t Num = ArrayOps->GetNum(ValuePtr);
	for (size_t Index = 0; Index < Num; ++Index)
	{
		Arr.append(InnerProperty->SerializeValue(const_cast<void*>(ArrayOps->GetConstElementPtr(ValuePtr, Index)), RefContext));
	}
	return Arr;
}

void FArrayProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	size_t Num = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		(void)Elem;
		++Num;
	}

	ArrayOps->Resize(ValuePtr, Num);

	size_t Index = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		InnerProperty->DeserializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Elem);
		++Index;
	}
}

void FArrayProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	size_t Num = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		(void)Elem;
		++Num;
	}

	ArrayOps->Resize(ValuePtr, Num);

	size_t Index = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		InnerProperty->DeserializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Elem, RefContext);
		++Index;
	}
}

json::JSON FArrayProperty::SerializeValue(void* ValuePtr, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->GetConstElementPtr || !InnerProperty)
	{
		return JSON();
	}

	JSON Arr = json::Array();
	const size_t Num = ArrayOps->GetNum(ValuePtr);
	for (size_t Index = 0; Index < Num; ++Index)
	{
		Arr.append(InnerProperty->SerializeValue(
			const_cast<void*>(ArrayOps->GetConstElementPtr(ValuePtr, Index)),
			Owner,
			RefContext));
	}
	return Arr;
}

void FArrayProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	size_t Num = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		(void)Elem;
		++Num;
	}

	ArrayOps->Resize(ValuePtr, Num);

	size_t Index = 0;
	for (auto& Elem : Value.ArrayRange())
	{
		InnerProperty->DeserializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Elem, Owner, RefContext);
		++Index;
	}
}

void FArrayProperty::SerializeValue(void* ValuePtr, UObject* Owner, FArchive& Ar) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	uint32 Num = static_cast<uint32>(ArrayOps->GetNum(ValuePtr));
	Ar << Num;
	if (Ar.IsLoading())
	{
		ArrayOps->Resize(ValuePtr, Num);
	}

	for (uint32 Index = 0; Index < Num; ++Index)
	{
		InnerProperty->SerializeValue(ArrayOps->GetElementPtr(ValuePtr, Index), Owner, Ar);
	}
}

void FArrayProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->Resize || !ArrayOps->GetElementPtr || !InnerProperty)
	{
		return;
	}

	uint32 Num = static_cast<uint32>(ArrayOps->GetNum(ValuePtr));
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
