#pragma once

#include <functional>
#include <utility>
#include "CoreTypes.h"

// ============================================================
// FDelegateHandle — 핸들 기반 델리게이트 식별/제거용
// ============================================================
class FDelegateHandle
{
public:
	FDelegateHandle() : ID(0) {}

	bool IsValid() const { return ID != 0; }
	void Reset() { ID = 0; }

	bool operator==(const FDelegateHandle& Other) const { return ID == Other.ID; }
	bool operator!=(const FDelegateHandle& Other) const { return ID != Other.ID; }

private:
	friend class FDelegateHandleFactory;
	explicit FDelegateHandle(uint64 InID) : ID(InID) {}

	uint64 ID;
};

class FDelegateHandleFactory
{
public:
	static FDelegateHandle Generate()
	{
		static uint64 GNextID = 0;
		return FDelegateHandle(++GNextID);
	}
};

// ============================================================
// TDelegate — 유니캐스트 델리게이트
// ============================================================
template<typename Signature>
class TDelegate
{
	static_assert(sizeof(Signature) == 0, "TDelegate must be instantiated with a function signature: TDelegate<RetVal(Params...)>");
};

template<typename RetVal, typename... ParamTypes>
class TDelegate<RetVal(ParamTypes...)>
{
public:
	TDelegate() = default;

	// --- Bind ---

	template<typename T>
	void BindRaw(T* Obj, RetVal(T::*MemFn)(ParamTypes...))
	{
		Handle = FDelegateHandleFactory::Generate();
		Func = [Obj, MemFn](ParamTypes... Params) -> RetVal
		{
			return (Obj->*MemFn)(Params...);
		};
	}

	template<typename T>
	void BindRaw(T* Obj, RetVal(T::*MemFn)(ParamTypes...) const)
	{
		Handle = FDelegateHandleFactory::Generate();
		Func = [Obj, MemFn](ParamTypes... Params) -> RetVal
		{
			return (Obj->*MemFn)(Params...);
		};
	}

	template<typename FunctorType>
	void BindLambda(FunctorType&& InFunctor)
	{
		Handle = FDelegateHandleFactory::Generate();
		Func = std::forward<FunctorType>(InFunctor);
	}

	void BindStatic(RetVal(*InFunc)(ParamTypes...))
	{
		Handle = FDelegateHandleFactory::Generate();
		Func = InFunc;
	}

	// --- Static Factories ---

	template<typename FunctorType>
	static TDelegate CreateLambda(FunctorType&& InFunctor)
	{
		TDelegate Result;
		Result.BindLambda(std::forward<FunctorType>(InFunctor));
		return Result;
	}

	template<typename T>
	static TDelegate CreateRaw(T* Obj, RetVal(T::*MemFn)(ParamTypes...))
	{
		TDelegate Result;
		Result.BindRaw(Obj, MemFn);
		return Result;
	}

	template<typename T>
	static TDelegate CreateRaw(T* Obj, RetVal(T::*MemFn)(ParamTypes...) const)
	{
		TDelegate Result;
		Result.BindRaw(Obj, MemFn);
		return Result;
	}

	static TDelegate CreateStatic(RetVal(*InFunc)(ParamTypes...))
	{
		TDelegate Result;
		Result.BindStatic(InFunc);
		return Result;
	}

	// --- Execute ---

	RetVal Execute(ParamTypes... Params) const
	{
		check(IsBound());
		return Func(Params...);
	}

	// void 반환 전용 ExecuteIfBound
	template<typename R = RetVal>
	typename std::enable_if<std::is_void<R>::value>::type
	ExecuteIfBound(ParamTypes... Params) const
	{
		if (IsBound())
		{
			Func(Params...);
		}
	}

	// --- State ---

	void Unbind()
	{
		Func = nullptr;
		Handle.Reset();
	}

	bool IsBound() const { return static_cast<bool>(Func); }

	FDelegateHandle GetHandle() const { return Handle; }

private:
	std::function<RetVal(ParamTypes...)> Func;
	FDelegateHandle Handle;
};

// ============================================================
// TMulticastDelegate — 멀티캐스트 델리게이트 (void 반환만)
// ============================================================
template<typename Signature>
class TMulticastDelegate
{
	static_assert(sizeof(Signature) == 0, "TMulticastDelegate must be instantiated with a function signature: TMulticastDelegate<void(Params...)>");
};

template<typename... ParamTypes>
class TMulticastDelegate<void(ParamTypes...)>
{
public:
	using FDelegate = TDelegate<void(ParamTypes...)>;

	// --- Add (returns handle) ---

	FDelegateHandle Add(FDelegate&& InDelegate)
	{
		FDelegateHandle Handle = InDelegate.GetHandle();
		InvocationList.push_back(std::move(InDelegate));
		return Handle;
	}

	FDelegateHandle Add(const FDelegate& InDelegate)
	{
		FDelegateHandle Handle = InDelegate.GetHandle();
		InvocationList.push_back(InDelegate);
		return Handle;
	}

	template<typename T>
	FDelegateHandle AddRaw(T* Obj, void(T::*MemFn)(ParamTypes...))
	{
		FDelegate D;
		D.BindRaw(Obj, MemFn);
		return Add(std::move(D));
	}

	template<typename T>
	FDelegateHandle AddRaw(T* Obj, void(T::*MemFn)(ParamTypes...) const)
	{
		FDelegate D;
		D.BindRaw(Obj, MemFn);
		return Add(std::move(D));
	}

	template<typename FunctorType>
	FDelegateHandle AddLambda(FunctorType&& InFunctor)
	{
		FDelegate D;
		D.BindLambda(std::forward<FunctorType>(InFunctor));
		return Add(std::move(D));
	}

	template<typename T>
	FDelegateHandle AddUObject(T* Obj, void(T::*MemFn)(ParamTypes...))
	{
		return AddRaw(Obj, MemFn);
	}

	template<typename T>
	FDelegateHandle AddUObject(T* Obj, void(T::*MemFn)(ParamTypes...) const)
	{
		return AddRaw(Obj, MemFn);
	}

	// --- Remove ---

	bool Remove(FDelegateHandle Handle)
	{
		for (auto It = InvocationList.begin(); It != InvocationList.end(); ++It)
		{
			if (It->GetHandle() == Handle)
			{
				InvocationList.erase(It);
				return true;
			}
		}
		return false;
	}

	void RemoveAll() { InvocationList.clear(); }
	void Clear() { InvocationList.clear(); }

	// --- Broadcast ---

	void Broadcast(ParamTypes... Params) const
	{
		// 리스트 복사 후 순회 — 순회 중 Remove 안전
		TArray<FDelegate> Copy = InvocationList;
		for (auto& D : Copy)
		{
			D.ExecuteIfBound(Params...);
		}
	}

	// --- State ---

	bool IsBound() const { return !InvocationList.empty(); }

private:
	TArray<FDelegate> InvocationList;
};

// non-void 반환 TMulticastDelegate 차단
template<typename RetVal, typename... ParamTypes>
class TMulticastDelegate<RetVal(ParamTypes...)>
{
	static_assert(std::is_void<RetVal>::value, "TMulticastDelegate only supports void return type.");
};

// ============================================================
// DECLARE_DELEGATE 매크로 — 유니캐스트
// ============================================================
#define DECLARE_DELEGATE(Name) \
	using Name = TDelegate<void()>

#define DECLARE_DELEGATE_OneParam(Name, P1) \
	using Name = TDelegate<void(P1)>

#define DECLARE_DELEGATE_TwoParams(Name, P1, P2) \
	using Name = TDelegate<void(P1, P2)>

#define DECLARE_DELEGATE_ThreeParams(Name, P1, P2, P3) \
	using Name = TDelegate<void(P1, P2, P3)>

#define DECLARE_DELEGATE_RetVal(RetVal, Name) \
	using Name = TDelegate<RetVal()>

#define DECLARE_DELEGATE_RetVal_OneParam(RetVal, Name, P1) \
	using Name = TDelegate<RetVal(P1)>

#define DECLARE_DELEGATE_RetVal_TwoParams(RetVal, Name, P1, P2) \
	using Name = TDelegate<RetVal(P1, P2)>

#define DECLARE_DELEGATE_RetVal_ThreeParams(RetVal, Name, P1, P2, P3) \
	using Name = TDelegate<RetVal(P1, P2, P3)>

// ============================================================
// DECLARE_MULTICAST_DELEGATE 매크로
// ============================================================
#define DECLARE_MULTICAST_DELEGATE(Name) \
	using Name = TMulticastDelegate<void()>

#define DECLARE_MULTICAST_DELEGATE_OneParam(Name, P1) \
	using Name = TMulticastDelegate<void(P1)>

#define DECLARE_MULTICAST_DELEGATE_TwoParams(Name, P1, P2) \
	using Name = TMulticastDelegate<void(P1, P2)>

#define DECLARE_MULTICAST_DELEGATE_ThreeParams(Name, P1, P2, P3) \
	using Name = TMulticastDelegate<void(P1, P2, P3)>
