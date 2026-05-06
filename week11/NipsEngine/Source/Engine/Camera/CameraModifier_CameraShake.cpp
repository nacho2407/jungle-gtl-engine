#include "CameraModifier_CameraShake.h"

#include <algorithm>

UCameraModifier_CameraShake::UCameraModifier_CameraShake()
{
    Priority = 10;
}

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
    StopAllCameraShakes(true);
}

UCameraShakeBase* UCameraModifier_CameraShake::StartCameraShakeWithPattern(
    UCameraShakePattern* Pattern,
    float Scale,
    float DurationOverride)
{
    if (!Pattern)
    {
        return nullptr;
    }

    UCameraShakeBase* Shake = UObjectManager::Get().CreateObject<UCameraShakeBase>();
    if (!Shake)
    {
        UObjectManager::Get().DestroyObject(Pattern);
        return nullptr;
    }

    Shake->SetRootShakePattern(Pattern);
    ActiveShakes.push_back({ Shake });
    Shake->StartShake(CameraOwner, Scale, DurationOverride);

    return Shake;
}

void UCameraModifier_CameraShake::StopCameraShake(UCameraShakeBase* Shake, bool bImmediately)
{
    if (!Shake)
    {
        return;
    }

    Shake->StopShake(bImmediately);

    if (bImmediately || Shake->IsFinished())
    {
        for (int i = 0; i < ActiveShakes.size(); ++i)
        {
            if (ActiveShakes[i].Shake == Shake)
            {
                RemoveCameraShakeAt(i);
                break;
            }
        }
    }
}

void UCameraModifier_CameraShake::StopAllCameraShakes(bool bImmediately)
{
    for (FActiveCameraShake& Entry : ActiveShakes)
    {
        if (Entry.Shake)
        {
            Entry.Shake->StopShake(bImmediately);
        }
    }

    if (bImmediately)
    {
        while (!ActiveShakes.empty())
        {
            RemoveCameraShakeAt(static_cast<int>(ActiveShakes.size()) - 1);
        }
    }
}

bool UCameraModifier_CameraShake::ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView)
{
    for (int i = 0; i < ActiveShakes.size();)
    {
        UCameraShakeBase* Shake = ActiveShakes[i].Shake;
        if (!Shake)
        {
            RemoveCameraShakeAt(i);
            continue;
        }

        Shake->UpdateAndApplyCameraShake(DeltaTime, Alpha, InOutView);

        if (Shake->IsFinished())
        {
            RemoveCameraShakeAt(i);
        }
        else
        {
            ++i;
        }
    }

    return !ActiveShakes.empty();
}

void UCameraModifier_CameraShake::RemoveCameraShakeAt(int Index)
{
    if (Index < 0 || Index >= ActiveShakes.size())
    {
        return;
    }

    FActiveCameraShake Entry = ActiveShakes[Index];
    ActiveShakes.erase(ActiveShakes.begin() + Index);

    if (Entry.Shake)
    {
        UCameraShakePattern* Pattern = Entry.Shake->GetRootShakePattern();
        UObjectManager::Get().DestroyObject(Entry.Shake);
        UObjectManager::Get().DestroyObject(Pattern);
    }
}
