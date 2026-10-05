#include "Object/Function.h"

#include "Object/Object.h"

#include <new>

UFunction::UFunction(
	const char* InName,
	UStruct* InOuterStruct,
	size_t InParmsSize,
	size_t InAlignment,
	EFunctionFlags InFunctionFlags,
	const IStructOps* InParamsOps,
	FNativeFuncPtr InFunc,
	const char* InDisplayName,
	const char* InCategory)
	: UStruct(InName, nullptr, InParmsSize, InAlignment, InDisplayName, InCategory)
	, OuterStruct(InOuterStruct)
	, FunctionFlags(InFunctionFlags)
	, ParamsOps(InParamsOps)
	, Func(InFunc)
{
}

void UFunction::Invoke(UObject* Context, void* Params) const
{
	if (Func && Context)
	{
		Func(Context, Params);
	}
}

void UFunction::ConstructParams(void* Params) const
{
	if (ParamsOps && Params)
	{
		ParamsOps->Construct(Params);
	}
}

void UFunction::DestructParams(void* Params) const
{
	if (ParamsOps && Params)
	{
		ParamsOps->Destruct(Params);
	}
}

void UFunction::CopyParams(void* Dst, const void* Src) const
{
	if (ParamsOps && Dst && Src)
	{
		ParamsOps->Copy(Dst, Src);
	}
}

const FProperty* UFunction::GetReturnProperty() const
{
	for (const FProperty& Property : GetProperties())
	{
		if (HasPropertyFlag(Property.Flags, EPropertyFlags::ReturnParm))
		{
			return &Property;
		}
	}
	return nullptr;
}

bool UFunction::HasAnyFunctionFlags(EFunctionFlags Flags) const
{
	return (static_cast<uint32>(FunctionFlags) & static_cast<uint32>(Flags)) != 0;
}

FScopedFunctionParams::FScopedFunctionParams(const UFunction* InFunction)
	: Function(InFunction)
{
	if (!Function || Function->GetParmsSize() == 0)
	{
		return;
	}

	const std::align_val_t Alignment = static_cast<std::align_val_t>(Function->GetMinAlignment());
	Memory = ::operator new(Function->GetParmsSize(), Alignment);
	Function->ConstructParams(Memory);
}

FScopedFunctionParams::~FScopedFunctionParams()
{
	if (!Function || !Memory)
	{
		return;
	}

	Function->DestructParams(Memory);
	::operator delete(Memory, static_cast<std::align_val_t>(Function->GetMinAlignment()));
}
