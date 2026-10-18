#include "BeatTracker.hpp"

#include "main.hpp"

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BpmController.hpp"
#include "GlobalNamespace/BPMChangeBeatmapEventData.hpp"

#include "UnityEngine/Time.hpp"

namespace VRMQavatars
{
    float BeatTracker::beat = 0.0f;
    float BeatTracker::bpm = 120.0f;
    bool BeatTracker::hasSongTime = false;
    float BeatTracker::lastSongTime = 0.0f;
    int BeatTracker::lastAdvanceFrame = -100;

    bool BeatTracker::IsPlaying()
    {
        return UnityEngine::Time::get_frameCount() - lastAdvanceFrame <= 2;
    }

    void BeatTracker::OnSongTime(const float songTime)
    {
        const float delta = songTime - lastSongTime;
        // integrate the beat so bpm changes keep the phase, restart from the song time after seeking
        if(!hasSongTime || delta < 0.0f || delta > 0.5f)
            beat = songTime * bpm / 60.0f;
        else
            beat += delta * bpm / 60.0f;

        if(hasSongTime && delta > 0.0f)
            lastAdvanceFrame = UnityEngine::Time::get_frameCount();

        hasSongTime = true;
        lastSongTime = songTime;
    }

    void BeatTracker::OnBpmReset(const float startBpm)
    {
        bpm = startBpm > 0.0f ? startBpm : 120.0f;
        hasSongTime = false;
    }

    void BeatTracker::OnBpmChanged(const float newBpm)
    {
        if(newBpm > 0.0f)
            bpm = newBpm;
    }
}

MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &GlobalNamespace::AudioTimeSyncController::Update, void, GlobalNamespace::AudioTimeSyncController* self)
{
    AudioTimeSyncController_Update(self);
    if(self->get_state() == GlobalNamespace::AudioTimeSyncController::State::Playing)
        VRMQavatars::BeatTracker::OnSongTime(self->get_songTime());
}

MAKE_HOOK_MATCH(BpmController_ctor, &GlobalNamespace::BpmController::_ctor, void, GlobalNamespace::BpmController* self, GlobalNamespace::BpmController_InitData* initData, GlobalNamespace::BeatmapCallbacksController* beatmapCallbacksController)
{
    BpmController_ctor(self, initData, beatmapCallbacksController);
    VRMQavatars::BeatTracker::OnBpmReset(initData->startBpm);
}

MAKE_HOOK_MATCH(BpmController_HandleBpmChange, &GlobalNamespace::BpmController::HandleBpmChangeBeatmapEvent, void, GlobalNamespace::BpmController* self, GlobalNamespace::BPMChangeBeatmapEventData* eventData)
{
    BpmController_HandleBpmChange(self, eventData);
    VRMQavatars::BeatTracker::OnBpmChanged(self->get_currentBpm());
}

namespace VRMQavatars
{
    void BeatTracker::InstallHooks()
    {
        INSTALL_HOOK(VRMLogger, AudioTimeSyncController_Update);
        INSTALL_HOOK(VRMLogger, BpmController_ctor);
        INSTALL_HOOK(VRMLogger, BpmController_HandleBpmChange);
    }
}
