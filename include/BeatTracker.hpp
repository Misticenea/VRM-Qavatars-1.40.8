#pragma once

namespace VRMQavatars
{
    // follows the playing song so avatar motion can be synced to the beat
    class BeatTracker
    {
    public:
        static void InstallHooks();

        // true while a song is playing and its time is advancing
        static bool IsPlaying();

        // beats since the start of the song, follows bpm changes
        static float beat;
        static float bpm;

        static void OnSongTime(float songTime);
        static void OnBpmReset(float startBpm);
        static void OnBpmChanged(float newBpm);

    private:
        static bool hasSongTime;
        static float lastSongTime;
        static int lastAdvanceFrame;
    };
}
