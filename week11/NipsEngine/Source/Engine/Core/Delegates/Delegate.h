#pragma once
#include <functional>
#include <vector>

#define DECLARE_DELEGATE(Name, ...) \
	using Name = TDelegate<__VA_ARGS__>;



template <typename... Args>
class TDelegate
{
public:
    using HandlerType = std::function<void(Args...)>;

	struct FDelegateInfo
    {
        uint64 Id;
        HandlerType Func;
    };

    // 일반 함수나 람다 등록
    uint64 Add(const HandlerType& Handler)
    {
        HandlerId NewId = ++Counter;
        DelegateInfos.push_back({ NewId, Handler });

		return NewId;
    }

    // 클래스 멤버 함수 바인딩
	// e.g., Target->OnTakeDamage.AddDynamic(this, &AAnotherActor::HandleDamage);
    template <typename T>
    uint64 AddDynamic(T* Instance, void (T::*Func)(Args...))
    {
        HandlerId NewId = ++Counter;

        DelegateInfos.push_back({ NewId,
								 [Instance, Func](Args... args)
								 {
									 (Instance->*Func)(args...);
								 } });

        return NewId;
    }

	void Remove(uint64 Id)
    {
        auto it = std::remove_if(
            DelegateInfos.begin(),
            DelegateInfos.end(),
            [Id](const FDelegateInfo& H)
            {
                return H.Id == Id;
            });

        DelegateInfos.erase(it, DelegateInfos.end());
    }

    void Broadcast(Args... args)
    {
        for (const FDelegateInfo& DelegateInfo : DelegateInfos)
		{
            DelegateInfo.Func(args...);
		}
    }

private:
    std::vector<FDelegateInfo> DelegateInfos;
    uint64 Counter = 0;
};