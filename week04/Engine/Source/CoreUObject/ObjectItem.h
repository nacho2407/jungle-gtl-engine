#pragma once

class UObject;

/**
 * @brief 현재는 Object만 들고 있는, ObjectArray를 위한 슬롯 아이템이지만 향후 GC 확장이나 ObjectPtr 확장이 용이하도록 별도의 슬롯 아이템으로 나누어 두었습니다.
 */
struct FObjectItem
{
    UObject* Object = nullptr;
};
