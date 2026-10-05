#pragma once

#include "Object/Class.h"

class UObject;

enum class EFunctionFlags : uint32
{
	None = 0,
	Native = 1 << 0,
	Callable = 1 << 1,
	LuaCallable = 1 << 2,
	BlueprintCallable = 1 << 3,
	BlueprintPure = 1 << 4,
	Const = 1 << 5,
};

constexpr EFunctionFlags operator|(EFunctionFlags Lhs, EFunctionFlags Rhs)
{
	return static_cast<EFunctionFlags>(static_cast<uint32>(Lhs) | static_cast<uint32>(Rhs));
}

constexpr EFunctionFlags operator&(EFunctionFlags Lhs, EFunctionFlags Rhs)
{
	return static_cast<EFunctionFlags>(static_cast<uint32>(Lhs) & static_cast<uint32>(Rhs));
}

constexpr EFunctionFlags& operator|=(EFunctionFlags& Lhs, EFunctionFlags Rhs)
{
	Lhs = Lhs | Rhs;
	return Lhs;
}

constexpr bool HasFunctionFlag(EFunctionFlags Value, EFunctionFlags Flag)
{
	return (static_cast<uint32>(Value) & static_cast<uint32>(Flag)) != 0;
}

class UFunction : public UStruct
{
public:
	using FNativeFuncPtr = void(*)(UObject* Context, void* Params);

	UFunction(
		const char* InName,
		UStruct* InOuterStruct,
		size_t InParmsSize,
		size_t InAlignment,
		EFunctionFlags InFunctionFlags,
		const IStructOps* InParamsOps,
		FNativeFuncPtr InFunc,
		const char* InDisplayName = nullptr,
		const char* InCategory = nullptr);

	void Invoke(UObject* Context, void* Params) const;

	void ConstructParams(void* Params) const;
	void DestructParams(void* Params) const;
	void CopyParams(void* Dst, const void* Src) const;

	UStruct* GetOuterStruct() const { return OuterStruct; }
	size_t GetParmsSize() const { return GetStructureSize(); }
	EFunctionFlags GetFunctionFlags() const { return FunctionFlags; }
	const FProperty* GetReturnProperty() const;

	bool HasAnyFunctionFlags(EFunctionFlags Flags) const;

private:
	UStruct* OuterStruct = nullptr;
	EFunctionFlags FunctionFlags = EFunctionFlags::None;
	const IStructOps* ParamsOps = nullptr;
	FNativeFuncPtr Func = nullptr;
};

class FScopedFunctionParams
{
public:
	explicit FScopedFunctionParams(const UFunction* InFunction);
	~FScopedFunctionParams();

	FScopedFunctionParams(const FScopedFunctionParams&) = delete;
	FScopedFunctionParams& operator=(const FScopedFunctionParams&) = delete;

	void* GetMemory() { return Memory; }
	const void* GetMemory() const { return Memory; }

private:
	const UFunction* Function = nullptr;
	void* Memory = nullptr;
};
