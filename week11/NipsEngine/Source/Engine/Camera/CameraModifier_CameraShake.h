#pragma once
#include "CameraModifier.h"
#include "CameraShakeBase.h"

#include <type_traits>

class UCameraModifier_CameraShake : public UCameraModifier
{
public:
    UCameraModifier_CameraShake();
    virtual ~UCameraModifier_CameraShake() override;

    template <typename PatternType>
    UCameraShakeBase* StartCameraShake(float Scale = 1.0f, float DurationOverride = 0.0f)
    {
        static_assert(std::is_base_of<UCameraShakePattern, PatternType>::value, "PatternType must derive from UCameraShakePattern");

        PatternType* Pattern = UObjectManager::Get().CreateObject<PatternType>();
        if (!Pattern)
        {
            return nullptr;
        }

        return StartCameraShakeWithPattern(Pattern, Scale, DurationOverride);
    }

    void StopCameraShake(UCameraShakeBase* Shake, bool bImmediately = false);
    void StopAllCameraShakes(bool bImmediately = false);
    bool HasActiveCameraShakes() const { return !ActiveShakes.empty(); }

protected:
    virtual bool ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView) override;

private:
    struct FActiveCameraShake
    {
        UCameraShakeBase* Shake = nullptr;
    };

    UCameraShakeBase* StartCameraShakeWithPattern(UCameraShakePattern* Pattern, float Scale, float DurationOverride);
    void RemoveCameraShakeAt(int Index);

    TArray<FActiveCameraShake> ActiveShakes;
};
