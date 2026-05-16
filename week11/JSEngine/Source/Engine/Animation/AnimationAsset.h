#pragma once

#include "Object/Object.h"

class UAnimationAsset : public UObject
{
public:
    DECLARE_CLASS(UAnimationAsset, UObject)

    UAnimationAsset() = default;
    ~UAnimationAsset() override = default;

    virtual float GetPlayLength() const;
};
