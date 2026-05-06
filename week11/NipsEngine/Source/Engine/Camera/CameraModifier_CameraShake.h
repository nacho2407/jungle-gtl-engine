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
    UCameraShakeBase* AddCameraShake(float Scale = 1.0f, float DurationOverride = 0.0f)
    {
        static_assert(
            std::is_base_of<UCameraShakePattern, PatternType>::value,
            "PatternType must derive from UCameraShakePattern");

        PatternType* Pattern = UObjectManager::Get().CreateObject<PatternType>();
        if (!Pattern)
        {
            return nullptr;
        }

        return AddCameraShakeWithPattern(Pattern,Scale, DurationOverride);
    }

    UCameraShakeBase* AddCameraShakeByPatternTypeName(
        const FString& PatternTypeName,
        float Scale = 1.0f,
        float DurationOverride = 0.0f)
    {
        return AddCameraShakeByPatternTypeName(PatternTypeName, Scale, DurationOverride);
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

    void RemoveCameraShakeAt(int Index);
    UCameraShakeBase* AddCameraShakeWithPatternTypeName(const FString& PatternClassName, float Scale, float DurationOverride);
    UCameraShakeBase* AddCameraShakeWithPattern(UCameraShakePattern* Pattern, float Scale, float DurationOverride);


    TArray<FActiveCameraShake> ActiveShakes;
};
