//#pragma
//#include "CoreTypes.h"
//
//inline uint32 TotalMemory = 0;
//
//inline void* operator new(size_t size)
//{
//    TotalMemory += static_cast<uint32>(size);
//
//    void* ptr = std::malloc(size);
//    if (!ptr)
//        throw std::bad_alloc();
//
//    return ptr;
//}
//
//inline void operator delete(void* ptr, size_t size) noexcept
//{
//    TotalMemory -= static_cast<uint32>(size);
//
//    //std::cout << "Free\n";
//    std::free(ptr);
//}