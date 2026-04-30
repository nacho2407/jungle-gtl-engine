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

    // 일반 함수나 람다 등록
    void Add(const HandlerType& handler)
    {
        Handlers.push_back(handler);
    }

    // 클래스 멤버 함수 바인딩
	// e.g., Target->OnTakeDamage.AddDynamic(this, &AAnotherActor::HandleDamage);
    template <typename T>
    void AddDynamic(T* Instance, void (T::*Func)(Args...))
    {
        Handlers.push_back(
            [Instance, Func](Args... args)
            {
                (Instance->*Func)(args...);
            });
    }

    void Broadcast(Args... args)
    {
		for (const HandlerType& Handler : Handlers)
		{
            Handler(args);
		}
    }

private:
    std::vector<HandlerType> Handlers;
};