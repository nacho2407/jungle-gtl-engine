#pragma once

#include "Animation/CurvePlayback.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"

#include <functional>

struct FTimelineFloatTrack
{
    FString TrackName;
    FCurvePlaybackDesc Playback;

    std::function<void(float)> OnUpdate;
};

class FTimelinePlayer
{
public:
    void Play();
    void Pause();
    void Stop();
    void Tick(float DeltaTime);

    void SetPlayRate(float InPlayRate);
    void SetLoop(bool bInLoop);

    bool IsPlaying() const;
    bool IsPaused() const;
    float GetCurrentTime() const;
    void SetCurrentTime(float InCurrentTime);

    void AddFloatTrack(
        const FString& TrackName,
        const FCurvePlaybackDesc& Playback,
        std::function<void(float)> OnUpdate);

    void ClearTracks();

private:
    float CurrentTime = 0.0f;
    float PlayRate = 1.0f;

    bool bPlaying = false;
    bool bPaused = false;
    bool bLoop = false;

    TArray<FTimelineFloatTrack> FloatTracks;
};
