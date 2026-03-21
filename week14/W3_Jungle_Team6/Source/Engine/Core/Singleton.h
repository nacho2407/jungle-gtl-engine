#pragma once

// CRTP 싱글턴 베이스 클래스
// 사용법: class FMyClass : public TSingleton<FMyClass> { friend class TSingleton<FMyClass>; ... };
template<typename T>
class TSingleton
{
public:
	static T& Get()
	{
		static T Instance;
		return Instance;
	}

	TSingleton(const TSingleton&) = delete;
	TSingleton& operator=(const TSingleton&) = delete;
	TSingleton(TSingleton&&) = delete;
	TSingleton& operator=(TSingleton&&) = delete;

protected:
	TSingleton() = default;
	~TSingleton() = default;
};
