#pragma once

#include "sombrero/shared/FastVector3.hpp"
#include "sombrero/shared/FastQuaternion.hpp"

#include "GlobalNamespace/OVRPlugin.hpp"

namespace VRMQavatars
{
    // Live play drives the avatar's hands from the raw OVR controller poses (which the hand offset settings are tuned for),
    // but replays only give us the game's saber poses, which include the game's controller offsets.
    // We measure the difference during live play and undo it when playing back a replay.
    class ReplayCorrection
    {
    public:
        enum Part
        {
            LeftHand,
            RightHand,
            PartCount
        };

        // raw OVR pose converted to Unity's coordinate system, in tracking space
        static void GetTrackedPose(GlobalNamespace::OVRPlugin::Node node, Sombrero::FastVector3& pos, Sombrero::FastQuaternion& rot);

        // record the game's pose for a part during live play, in tracking space
        static void Sample(Part part, const Sombrero::FastVector3& gamePos, const Sombrero::FastQuaternion& gameRot);

        // convert a game pose from a replay back into the equivalent raw OVR pose
        static void Apply(Part part, Sombrero::FastVector3& pos, Sombrero::FastQuaternion& rot);

        static void SaveIfDirty();
    };
}
