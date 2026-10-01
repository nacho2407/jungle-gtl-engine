// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

template <typename T>
// TSingleton는 엔진 영역의 핵심 동작을 담당합니다.
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
