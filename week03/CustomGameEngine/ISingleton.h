#pragma once

// private 생성자 접근하려면 상속받은 곳에서 friend class MyClass; 선언 필요
template <typename T>
class ISingleton {
public:
    static T& GetInstance() {
        static T instance;
        return instance;
    }

    ISingleton(const ISingleton&) = delete;
    ISingleton& operator=(const ISingleton&) = delete;

protected:
    ISingleton() = default;
    virtual ~ISingleton() = default;
};