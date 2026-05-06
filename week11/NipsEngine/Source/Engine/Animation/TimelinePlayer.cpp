#include "Animation/TimelinePlayer.h"

void FTimelinePlayer::Play()
{
    bPlaying = true;
    bPaused = false;
}

void FTimelinePlayer::Pause()
{
    if (bPlaying)
    {
        bPaused = true;
    }
}

void FTimelinePlayer::Stop()
{
    bPlaying = false;
    bPaused = false;
    CurrentTime = 0.0f;
}

void FTimelinePlayer::Tick(float DeltaTime)
{
    if (!bPlaying || bPaused)
    {
        return;
    }

    CurrentTime += DeltaTime * PlayRate;

    for (FTimelineFloatTrack& Track : FloatTracks)
    {
        FCurvePlaybackDesc Playback = Track.Playback;
        Playback.bLoop = Playback.bLoop || bLoop;

        const FCurvePlaybackEvalResult Eval = FCurvePlaybackEvaluator::Evaluate(Playback, CurrentTime);
        if (!Eval.bActive || !Track.OnUpdate)
        {
            continue;
        }

        Track.OnUpdate(Eval.Value);
    }
}

void FTimelinePlayer::SetPlayRate(float InPlayRate)
{
    PlayRate = InPlayRate;
}

void FTimelinePlayer::SetLoop(bool bInLoop)
{
    bLoop = bInLoop;
}

bool FTimelinePlayer::IsPlaying() const
{
    return bPlaying;
}

bool FTimelinePlayer::IsPaused() const
{
    return bPaused;
}

float FTimelinePlayer::GetCurrentTime() const
{
    return CurrentTime;
}

void FTimelinePlayer::SetCurrentTime(float InCurrentTime)
{
    CurrentTime = InCurrentTime;
}

void FTimelinePlayer::AddFloatTrack(
    const FString& TrackName,
    const FCurvePlaybackDesc& Playback,
    std::function<void(float)> OnUpdate)
{
    FTimelineFloatTrack Track;
    Track.TrackName = TrackName;
    Track.Playback = Playback;
    Track.OnUpdate = OnUpdate;
    FloatTracks.push_back(Track);
}

void FTimelinePlayer::ClearTracks()
{
    FloatTracks.clear();
}
